#!/usr/bin/env python3
"""Collect, annotate, verify, report, and promote real MatrixHub CSI captures.

Python is intentionally limited to transport, annotation, integrity checks, and
reporting.  This tool never runs a motion detector and never turns observed
firmware output into an expected result.  Production detector replay belongs to
the native C++ harness.
"""

from __future__ import annotations

import argparse
import asyncio
import datetime as dt
import hashlib
import json
import math
import os
import re
import shutil
import sys
import time
from collections import Counter
from dataclasses import asdict
from pathlib import Path
from typing import Any, Iterable

try:
    import websockets
except ImportError:  # Offline verify/report/promote do not require WebSockets.
    websockets = None  # type: ignore[assignment]

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args  # noqa: E402

try:  # Package import in tests; direct import when executed as a script.
    from .csi_capture_format import (  # type: ignore[import-not-found]
        BATCH_DATA,
        BATCH_END,
        BATCH_ERROR,
        BATCH_HELLO,
        CaptureEnd,
        CaptureHello,
        CsiFrame,
        MAX_MHCB_MESSAGE_BYTES,
        MhcfWriter,
        decode_capture_end,
        decode_capture_error,
        decode_capture_hello,
        decode_mhcb_frames,
        decode_mhcb_header,
        format_mac,
        iter_mhcf_frames,
        validate_mhcf,
        write_mhcf,
    )
except ImportError:
    from csi_capture_format import (  # noqa: E402
        BATCH_DATA,
        BATCH_END,
        BATCH_ERROR,
        BATCH_HELLO,
        CaptureEnd,
        CaptureHello,
        CsiFrame,
        MAX_MHCB_MESSAGE_BYTES,
        MhcfWriter,
        decode_capture_end,
        decode_capture_error,
        decode_capture_hello,
        decode_mhcb_frames,
        decode_mhcb_header,
        format_mac,
        iter_mhcf_frames,
        validate_mhcf,
        write_mhcf,
    )


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_RAW_ROOT = REPO_ROOT / "artifacts" / "csi" / "raw"
DEFAULT_FIXTURE_ROOT = REPO_ROOT / "test" / "fixtures" / "csi"
CAPTURE_FILENAME = "frames.mhcf"
MANIFEST_FILENAME = "capture.json"
SCENARIO_FILENAME = "scenario.json"
MANIFEST_SCHEMA = "matrixhub.csi.capture-manifest/v1"
SCENARIO_SCHEMA = "matrixhub.csi.scenario/v1"
CAPTURE_ENDPOINT = "/ws/csi-capture/v1"
BOARD_ENV = "waveshare_esp32s3_matrix"

MOTION_VALUES = ("none", "present", "unknown")
OCCUPANCY_VALUES = ("empty", "occupied", "unknown")
ENVIRONMENT_VALUES = ("stable", "rf_disturbance", "reconnect", "unknown")
EVIDENCE_VALUES = ("operator", "scripted_action", "video_timestamp", "unknown")
CONFIDENCE_VALUES = ("high", "medium", "low", "unknown")
ACCEPTANCE_FIELDS = (
    "max_false_positive_ms",
    "max_false_negative_ms",
    "max_invalid_decision_ms",
    "max_detection_latency_ms",
    "max_clear_latency_ms",
    "max_motion_dropout_ms",
    "max_missed_motion_intervals",
    "max_uncleared_transitions",
)

ERROR_NAMES = {
    1: "busy",
    2: "already_started",
    3: "not_started",
    4: "unsupported_command",
    5: "transport_unavailable",
}

DURATION_RE = re.compile(r"^\s*(\d+(?:\.\d+)?)\s*(ms|s|m|h)?\s*$", re.IGNORECASE)
SLUG_RE = re.compile(r"^[a-z0-9][a-z0-9-]{0,79}$")
FIRMWARE_COMMIT_RE = re.compile(r"^[0-9a-f]{40}(?:-dirty)?$")
MAC_TEXT_RE = re.compile(r"(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}")
IPV4_TEXT_RE = re.compile(r"(?<!\d)(?:\d{1,3}\.){3}\d{1,3}(?!\d)")
EMAIL_TEXT_RE = re.compile(r"(?i)\b[^\s@]+@[^\s@]+\.[^\s@]+\b")
LOCAL_HOSTNAME_RE = re.compile(
    r"(?i)\b[a-z0-9](?:[a-z0-9-]{0,62}\.)+(?:local|lan|home|internal)\b"
)
SENSITIVE_TEXT_LABEL_RE = re.compile(
    r"(?i)\b(?:ssid|bssid|hostname|password|token|secret|device[-_ ]?url)\b"
)
SENSITIVE_KEY_PARTS = (
    "bssid",
    "ssid",
    "mac_address",
    "device_url",
    "password",
    "token",
    "secret",
)


class CaptureWorkflowError(RuntimeError):
    """Raised when a capture cannot be safely finalized or promoted."""


def parse_duration(value: str, *, allow_zero: bool = False) -> float:
    match = DURATION_RE.match(value or "")
    if not match:
        raise argparse.ArgumentTypeError(f"invalid duration: {value!r}")
    amount = float(match.group(1))
    unit = (match.group(2) or "s").lower()
    seconds = amount * {"ms": 0.001, "s": 1.0, "m": 60.0, "h": 3600.0}[unit]
    if seconds < 0 or (seconds == 0 and not allow_zero):
        qualifier = "non-negative" if allow_zero else "greater than zero"
        raise argparse.ArgumentTypeError(f"duration must be {qualifier}")
    return seconds


def parse_milliseconds(value: str) -> int:
    seconds = parse_duration(value, allow_zero=True)
    milliseconds = round(seconds * 1000.0)
    if milliseconds < 0 or milliseconds > 0xFFFFFFFF:
        raise argparse.ArgumentTypeError("timeline value must fit uint32 milliseconds")
    return milliseconds


def parse_nonnegative_int(value: str) -> int:
    try:
        parsed = int(value, 10)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"invalid integer: {value!r}") from exc
    if parsed < 0 or parsed >= (1 << 31):
        raise argparse.ArgumentTypeError(
            "value must be non-negative and fit the modular uint32 half-range"
        )
    return parsed


def require_slug(value: str, label: str) -> str:
    normalized = value.strip().lower()
    if not SLUG_RE.fullmatch(normalized):
        raise CaptureWorkflowError(
            f"{label} must match {SLUG_RE.pattern!r}; got {value!r}"
        )
    return normalized


def require_safe_raw_root(path: Path) -> Path:
    resolved = path.resolve()
    try:
        resolved.relative_to(REPO_ROOT)
    except ValueError:
        return resolved

    allowed_root = (REPO_ROOT / "artifacts").resolve()
    try:
        resolved.relative_to(allowed_root)
    except ValueError as exc:
        raise CaptureWorkflowError(
            "raw capture output inside the repository must stay under ignored "
            f"{allowed_root}; use promote for test/fixtures"
        ) from exc
    return resolved


def parse_firmware_commit(value: str) -> str:
    normalized = value.strip().lower()
    if not FIRMWARE_COMMIT_RE.fullmatch(normalized):
        raise argparse.ArgumentTypeError(
            "firmware commit must be a full 40-hex Git SHA, optionally suffixed "
            "with -dirty for non-promotable smoke captures"
        )
    return normalized


def require_promotable_provenance(source: dict[str, Any]) -> None:
    commit = source.get("firmware_commit")
    if not isinstance(commit, str) or not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise CaptureWorkflowError(
            "promotion requires an exact clean 40-hex firmware_commit"
        )
    if source.get("firmware_dirty") is not False:
        raise CaptureWorkflowError("promotion requires firmware_dirty=false")
    if source.get("firmware_identity_verified") is not True:
        raise CaptureWorkflowError(
            "promotion requires collector-verified firmware identity"
        )
    required_fields = (
        "board_env",
        "firmware_version",
        "build_target",
        "esp_platform",
        "sdk_version",
        "arduino_version",
    )
    incomplete = [
        field
        for field in required_fields
        if not isinstance(source.get(field), str)
        or not source[field].strip()
        or source[field].strip().lower() == "unknown"
    ]
    if incomplete:
        raise CaptureWorkflowError(
            "promotion requires complete firmware provenance: " + ", ".join(incomplete)
        )


def utc_timestamp() -> str:
    return dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def utc_iso() -> str:
    return dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def atomic_write_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.tmp")
    temporary.write_text(
        json.dumps(
            data,
            indent=2,
            sort_keys=True,
            ensure_ascii=False,
            allow_nan=False,
        )
        + "\n",
        encoding="utf-8",
    )
    os.chmod(temporary, 0o600)
    os.replace(temporary, path)


def load_json(path: Path) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise CaptureWorkflowError(f"missing file: {path}") from exc
    except json.JSONDecodeError as exc:
        raise CaptureWorkflowError(f"invalid JSON in {path}: {exc}") from exc
    if not isinstance(data, dict):
        raise CaptureWorkflowError(f"expected a JSON object in {path}")
    return data


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while chunk := handle.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def resolve_capture_paths(target: Path) -> tuple[Path, Path | None, Path | None]:
    if target.is_dir():
        capture = target / CAPTURE_FILENAME
        manifest = target / MANIFEST_FILENAME
        scenario = target / SCENARIO_FILENAME
        return capture, manifest if manifest.exists() else None, scenario if scenario.exists() else None
    return target, None, None


def connect_ws(uri: str, headers: dict[str, str], ssl_context):
    if websockets is None:
        raise CaptureWorkflowError(
            "collect requires the 'websockets' package from requirements.txt"
        )
    kwargs = {
        "ssl": ssl_context,
        "max_size": MAX_MHCB_MESSAGE_BYTES,
        "ping_timeout": 20,
    }
    try:
        return websockets.connect(uri, additional_headers=headers, **kwargs)
    except TypeError:
        return websockets.connect(uri, extra_headers=headers, **kwargs)


def get_json_object(client: DeviceClient, path: str) -> dict[str, Any]:
    value = client.json("GET", path)
    if not isinstance(value, dict):
        raise DeviceClientError(f"GET {path} returned a non-object JSON value")
    return value


def require_capture_ready_status(status: dict[str, Any]) -> None:
    csi = status.get("csi")
    if not isinstance(csi, dict):
        raise CaptureWorkflowError("Wi-Fi sensing status has no csi object")
    if csi.get("enabled") is not True:
        raise CaptureWorkflowError("CSI must be enabled before lossless capture")
    if csi.get("runtime_fault") is not False:
        raise CaptureWorkflowError("CSI runtime fault must be cleared before lossless capture")
    if csi.get("runtime_reconcile_pending") is not False:
        raise CaptureWorkflowError(
            "CSI runtime reconciliation must finish before lossless capture"
        )
    if csi.get("queue_allocated") is not True:
        raise CaptureWorkflowError("CSI queue must be allocated before lossless capture")
    if csi.get("calibration_state") != "forced":
        raise CaptureWorkflowError(
            "CSI gain must be forced/stable before capture; wait for calibration_state=forced"
        )

    motion = csi.get("motion")
    if not isinstance(motion, dict) or motion.get("enabled") is not True:
        raise CaptureWorkflowError("CSI alarm detector must be active before lossless capture")
    if motion.get("state") in (None, "disabled", "unavailable"):
        raise CaptureWorkflowError("CSI alarm detector is unavailable for lossless capture")
    if motion.get("has_frame") is not True or motion.get("data_fresh") is not True:
        raise CaptureWorkflowError(
            "CSI alarm detector requires a fresh valid frame before lossless capture"
        )


def require_capture_ready_config(config: dict[str, Any]) -> None:
    alarm = config.get("csi_alarm")
    if not isinstance(alarm, dict):
        raise CaptureWorkflowError("Wi-Fi sensing config has no csi_alarm object")
    if alarm.get("enabled") is not True:
        raise CaptureWorkflowError("CSI alarm must be enabled before lossless capture")

    bands = alarm.get("bands")
    if not isinstance(bands, list) or not any(
        isinstance(band, dict)
        and isinstance(band.get("start"), int)
        and not isinstance(band.get("start"), bool)
        and isinstance(band.get("end"), int)
        and not isinstance(band.get("end"), bool)
        and 0 <= band["start"] <= band["end"] <= 255
        for band in bands
    ):
        raise CaptureWorkflowError(
            "CSI alarm requires at least one valid band with integer "
            "0 <= start <= end <= 255 before lossless capture"
        )


def detector_config(config: dict[str, Any]) -> dict[str, Any]:
    value = config.get("csi_alarm")
    if not isinstance(value, dict):
        raise CaptureWorkflowError("Wi-Fi sensing config has no csi_alarm object")
    # Round-trip through JSON so later comparisons and manifests cannot retain
    # object subclasses or non-JSON values.
    return json.loads(json.dumps(value, sort_keys=True))


def firmware_provenance(
    system_info: dict[str, Any],
    firmware_commit: str,
) -> dict[str, Any]:
    expected = firmware_commit.strip().lower()
    expected_dirty = expected.endswith("-dirty")
    expected_commit = expected.removesuffix("-dirty")
    reported_commit = str(system_info.get("firmware_commit", "")).strip().lower()
    reported_dirty = system_info.get("firmware_dirty")
    if not re.fullmatch(r"[0-9a-f]{40}", reported_commit):
        raise CaptureWorkflowError(
            "device /api/system/info has no exact 40-hex firmware_commit; "
            "build and flash firmware with embedded identity"
        )
    if not isinstance(reported_dirty, bool):
        raise CaptureWorkflowError(
            "device /api/system/info has no boolean firmware_dirty identity"
        )
    if reported_commit != expected_commit or reported_dirty != expected_dirty:
        reported = f"{reported_commit}{'-dirty' if reported_dirty else ''}"
        raise CaptureWorkflowError(
            f"flashed firmware identity {reported} does not match expected {expected}"
        )

    mapping = {
        "board_env": BOARD_ENV,
        "firmware_version": str(system_info.get("firmware_version", "unknown")),
        "firmware_commit": reported_commit + ("-dirty" if reported_dirty else ""),
        "firmware_dirty": reported_dirty,
        "firmware_identity_verified": True,
        "build_target": str(system_info.get("firmware_built_target", "unknown")),
        "esp_platform": str(system_info.get("esp_platform", "unknown")),
        "sdk_version": str(system_info.get("sdk_version", "unknown")),
        "arduino_version": str(system_info.get("arduino_version", "unknown")),
    }
    return mapping


def relative_ms(value: int, first: int) -> int:
    return (value - first) & 0xFFFFFFFF


def capture_duration_ms(capture_path: Path) -> int:
    validation = validate_mhcf(capture_path)
    assert validation.first_process_now_ms is not None
    assert validation.last_process_now_ms is not None
    return relative_ms(validation.last_process_now_ms, validation.first_process_now_ms) + 1


def initial_scenario(
    scenario_id: str,
    description: str,
    capture_path: Path,
    config: dict[str, Any],
    system_info: dict[str, Any],
    firmware_commit: str,
) -> dict[str, Any]:
    duration_ms = capture_duration_ms(capture_path)
    digest = sha256_file(capture_path)
    return {
        "schema": SCENARIO_SCHEMA,
        "fixture_id": scenario_id,
        "description": description,
        "capture_file": CAPTURE_FILENAME,
        "capture_sha256": f"sha256:{digest}",
        "source": {
            "kind": "real_device",
            **firmware_provenance(system_info, firmware_commit),
        },
        "detector_config": detector_config(config),
        "ground_truth": {
            "reviewed": False,
            "timeline": [
                {
                    "start_ms": 0,
                    "end_ms": duration_ms,
                    "motion": "unknown",
                    "occupancy": "unknown",
                    "environment": "unknown",
                    "evidence": "unknown",
                    "confidence": "unknown",
                    "note": "Unannotated capture interval.",
                }
            ],
        },
        "acceptance": {
            "reviewed": False,
            **{field: None for field in ACCEPTANCE_FIELDS},
        },
    }


def validate_end(
    hello: CaptureHello,
    end: CaptureEnd,
    writer: MhcfWriter,
) -> None:
    failures: list[str] = []
    if end.end_reason != 1:
        failures.append(f"unexpected end reason {end.end_reason}")
    if end.records_offered != end.records_enqueued:
        failures.append(
            f"records offered/enqueued differ ({end.records_offered}/{end.records_enqueued})"
        )
    if end.records_enqueued != writer.frame_count:
        failures.append(
            f"END records={end.records_enqueued}, collector frames={writer.frame_count}"
        )
    if end.records_dropped:
        failures.append(f"capture transport dropped {end.records_dropped} record(s)")
    if end.batches_offered != end.batches_enqueued:
        failures.append(
            f"batches offered/enqueued differ ({end.batches_offered}/{end.batches_enqueued})"
        )
    if end.batches_dropped:
        failures.append(f"capture transport dropped {end.batches_dropped} batch(es)")
    if end.truncated_records:
        failures.append(f"firmware truncated {end.truncated_records} record(s)")
    if end.error_flags:
        failures.append(f"session error flags are 0x{end.error_flags:08x}")
    if not writer.initial_replay_origin:
        failures.append("first captured frame has no deterministic replay origin")
    if writer.frame_count:
        expected_first = (hello.rx_accepted_start + 1) & 0xFFFFFFFF
        expected_records = (
            end.rx_accepted_end - hello.rx_accepted_start
        ) & 0xFFFFFFFF
        if expected_records >= 0x80000000:
            failures.append("accepted-sequence fence spans an invalid half-range")
        if end.records_offered != expected_records:
            failures.append(
                "source window count differs "
                f"(expected={expected_records}, offered={end.records_offered})"
            )
        if writer.first_sequence != expected_first:
            failures.append(
                "START fence differs "
                f"(expected first={expected_first}, file={writer.first_sequence})"
            )
        if end.first_sequence != writer.first_sequence:
            failures.append(
                f"first sequence differs (END={end.first_sequence}, file={writer.first_sequence})"
            )
        if end.last_sequence != writer.last_sequence:
            failures.append(
                f"last sequence differs (END={end.last_sequence}, file={writer.last_sequence})"
            )
    if failures:
        raise CaptureWorkflowError("capture is incomplete: " + "; ".join(failures))


async def receive_data_until_end(
    websocket,
    writer: MhcfWriter,
    session_id: int,
    *,
    deadline: float,
    stop_timeout: float,
) -> tuple[CaptureEnd, int]:
    batches = 0
    stop_sent = False
    stop_deadline = 0.0

    while True:
        now = time.monotonic()
        if not stop_sent and now >= deadline:
            await websocket.send("STOP")
            stop_sent = True
            stop_deadline = now + stop_timeout

        timeout = 1.0
        if stop_sent:
            remaining = stop_deadline - now
            if remaining <= 0:
                raise CaptureWorkflowError("timed out waiting for capture END")
            timeout = min(timeout, remaining)
        else:
            timeout = min(timeout, max(0.001, deadline - now))

        try:
            message = await asyncio.wait_for(websocket.recv(), timeout=timeout)
        except asyncio.TimeoutError:
            continue
        if not isinstance(message, bytes):
            raise CaptureWorkflowError("capture endpoint returned an unexpected text frame")

        header = decode_mhcb_header(message)
        if header.session_id != session_id:
            raise CaptureWorkflowError(
                f"capture session changed from {session_id} to {header.session_id}"
            )

        if header.message_type == BATCH_HELLO:
            raise CaptureWorkflowError("duplicate capture HELLO")
        if header.message_type == BATCH_DATA:
            _, frames = decode_mhcb_frames(message)
            for frame in frames:
                writer.append(frame)
            batches += 1
            continue
        if header.message_type == BATCH_ERROR:
            _, code = decode_capture_error(message)
            raise CaptureWorkflowError(
                f"device rejected capture command: {ERROR_NAMES.get(code, f'error_{code}')}"
            )
        if header.message_type == BATCH_END:
            if not stop_sent:
                raise CaptureWorkflowError("capture END arrived before collector STOP")
            _, end = decode_capture_end(message)
            return end, batches


async def collect_capture(args: argparse.Namespace) -> Path:
    scenario_id = require_slug(args.scenario_id, "scenario-id")
    raw_root = require_safe_raw_root(args.output_root)

    client = DeviceClient.from_args(args)
    config_before = get_json_object(client, "/api/wifisensing/config")
    require_capture_ready_config(config_before)
    status_before = get_json_object(client, "/api/wifisensing/status")
    require_capture_ready_status(status_before)
    system_info = get_json_object(client, "/api/system/info")
    # Verify device-reported build identity before creating even a partial
    # artifact. The CLI value is an expected identity, never the evidence
    # source written into scenario metadata.
    firmware_provenance(system_info, args.firmware_commit)

    raw_root.mkdir(parents=True, exist_ok=True)
    base_name = f"{utc_timestamp()}-{scenario_id}"
    partial_dir = raw_root / f"{base_name}.partial"
    final_dir = raw_root / base_name
    if partial_dir.exists() or final_dir.exists():
        raise CaptureWorkflowError(f"capture output already exists for {base_name}")
    partial_dir.mkdir(mode=0o700)

    partial_capture = partial_dir / f"{CAPTURE_FILENAME}.partial"
    final_capture = partial_dir / CAPTURE_FILENAME
    manifest_path = partial_dir / MANIFEST_FILENAME
    scenario_path = partial_dir / SCENARIO_FILENAME
    started_iso = utc_iso()
    started_monotonic = time.monotonic()
    manifest: dict[str, Any] = {
        "schema": MANIFEST_SCHEMA,
        "state": "incomplete",
        "source_kind": "real_device",
        "scenario_id": scenario_id,
        "capture_endpoint": CAPTURE_ENDPOINT,
        "started_utc": started_iso,
        "requested_duration_ms": round(args.duration * 1000),
    }
    atomic_write_json(manifest_path, manifest)

    writer: MhcfWriter | None = None
    try:
        uri = client.ws_url(CAPTURE_ENDPOINT)

        async with connect_ws(uri, client.ws_cookie_header(), client.ws_ssl_context()) as websocket:
            await websocket.send("START")
            first = await asyncio.wait_for(websocket.recv(), timeout=args.start_timeout)
            if not isinstance(first, bytes):
                raise CaptureWorkflowError("capture endpoint returned text instead of HELLO")
            first_header = decode_mhcb_header(first)
            if first_header.message_type == BATCH_ERROR:
                _, code = decode_capture_error(first)
                raise CaptureWorkflowError(
                    f"device rejected START: {ERROR_NAMES.get(code, f'error_{code}')}"
                )
            if first_header.message_type != BATCH_HELLO:
                raise CaptureWorkflowError("first capture message was not HELLO")
            hello_header, hello = decode_capture_hello(first)
            writer = MhcfWriter(partial_capture, hello_header.session_id)

            deadline = time.monotonic() + args.duration
            end, batch_count = await receive_data_until_end(
                websocket,
                writer,
                hello_header.session_id,
                deadline=deadline,
                stop_timeout=args.stop_timeout,
            )

        config_after = get_json_object(client, "/api/wifisensing/config")
        status_after = get_json_object(client, "/api/wifisensing/status")
        if config_before != config_after:
            raise CaptureWorkflowError("Wi-Fi sensing config changed during capture")
        validate_end(hello, end, writer)
        header = writer.finalize()
        writer = None
        os.replace(partial_capture, final_capture)
        validation = validate_mhcf(final_capture)
        digest = sha256_file(final_capture)
        scenario = initial_scenario(
            scenario_id,
            args.description,
            final_capture,
            config_before,
            system_info,
            args.firmware_commit,
        )
        atomic_write_json(scenario_path, scenario)
        manifest.update(
            {
                "state": "complete",
                "completed_utc": utc_iso(),
                "elapsed_ms": round((time.monotonic() - started_monotonic) * 1000),
                "file": {
                    "name": CAPTURE_FILENAME,
                    "sha256": f"sha256:{digest}",
                    "bytes": final_capture.stat().st_size,
                    "frame_count": header.frame_count,
                    "frames_section_bytes": header.frames_section_bytes,
                },
                "transport": {
                    "batch_count": batch_count,
                    "hello": asdict(hello),
                    "end": asdict(end),
                    "first_sequence": validation.first_sequence,
                    "last_sequence": validation.last_sequence,
                },
                "snapshots": {
                    "config_before": config_before,
                    "config_after": config_after,
                    "status_before": status_before,
                    "status_after": status_after,
                    "system_info": system_info,
                },
            }
        )
        atomic_write_json(manifest_path, manifest)
        os.replace(partial_dir, final_dir)
        return final_dir
    except BaseException as exc:
        if writer is not None:
            writer.abort()
        manifest["state"] = "incomplete"
        manifest["failed_utc"] = utc_iso()
        manifest["error"] = f"{type(exc).__name__}: {exc}"
        atomic_write_json(manifest_path, manifest)
        raise


def validate_manifest(capture_path: Path, manifest: dict[str, Any]) -> None:
    if manifest.get("schema") != MANIFEST_SCHEMA:
        raise CaptureWorkflowError("unsupported capture manifest schema")
    if manifest.get("state") != "complete":
        raise CaptureWorkflowError("capture manifest is not complete")
    if manifest.get("source_kind") != "real_device":
        raise CaptureWorkflowError("detector fixtures must originate from a real device")
    file_data = manifest.get("file")
    if not isinstance(file_data, dict):
        raise CaptureWorkflowError("capture manifest has no file object")
    expected_hash = file_data.get("sha256")
    actual_hash = f"sha256:{sha256_file(capture_path)}"
    if expected_hash != actual_hash:
        raise CaptureWorkflowError("capture SHA-256 does not match its manifest")
    validation = validate_mhcf(capture_path)
    if file_data.get("frame_count") != validation.header.frame_count:
        raise CaptureWorkflowError("manifest frame count does not match MHCF header")
    if file_data.get("frames_section_bytes") != validation.header.frames_section_bytes:
        raise CaptureWorkflowError("manifest frame section size does not match MHCF header")
    if file_data.get("bytes") != capture_path.stat().st_size:
        raise CaptureWorkflowError("manifest file size does not match frames.mhcf")
    transport = manifest.get("transport")
    if not isinstance(transport, dict):
        raise CaptureWorkflowError("capture manifest has no transport result")
    hello_data = transport.get("hello")
    end_data = transport.get("end")
    if not isinstance(hello_data, dict) or not isinstance(end_data, dict):
        raise CaptureWorkflowError("capture manifest is missing HELLO/END evidence")
    hello = CaptureHello(**hello_data)
    end = CaptureEnd(**end_data)
    if end.end_reason != 1:
        raise CaptureWorkflowError(f"capture END has unexpected reason {end.end_reason}")
    if end.records_dropped or end.batches_dropped or end.truncated_records or end.error_flags:
        raise CaptureWorkflowError("capture manifest records loss/truncation/error flags")
    if end.records_enqueued != validation.header.frame_count:
        raise CaptureWorkflowError("END record count does not match MHCF frame count")
    if not validation.initial_replay_origin:
        raise CaptureWorkflowError("capture has no deterministic replay origin")
    if end.first_sequence != validation.first_sequence or end.last_sequence != validation.last_sequence:
        raise CaptureWorkflowError("END sequence bounds do not match frames.mhcf")
    if end.records_offered != end.records_enqueued:
        raise CaptureWorkflowError("capture END reports records lost before enqueue")
    if end.batches_offered != end.batches_enqueued:
        raise CaptureWorkflowError("capture END reports batches lost before enqueue")
    expected_records = (end.rx_accepted_end - hello.rx_accepted_start) & 0xFFFFFFFF
    if expected_records >= 0x80000000:
        raise CaptureWorkflowError("capture accepted-sequence fence spans an invalid half-range")
    if end.records_offered != expected_records:
        raise CaptureWorkflowError("capture source-window count does not match its fences")
    if validation.first_sequence != ((hello.rx_accepted_start + 1) & 0xFFFFFFFF):
        raise CaptureWorkflowError("capture first sequence does not follow its START fence")
    if validation.last_sequence != end.rx_accepted_end:
        raise CaptureWorkflowError("capture last sequence does not match its STOP fence")
    snapshots = manifest.get("snapshots")
    if not isinstance(snapshots, dict):
        raise CaptureWorkflowError("capture manifest has no snapshots")
    if snapshots.get("config_before") != snapshots.get("config_after"):
        raise CaptureWorkflowError("Wi-Fi sensing config changed during capture")


def validate_scenario(
    scenario: dict[str, Any],
    capture_path: Path,
    *,
    require_reviewed: bool,
    require_acceptance_reviewed: bool | None = None,
) -> None:
    if require_acceptance_reviewed is None:
        require_acceptance_reviewed = require_reviewed
    if scenario.get("schema") != SCENARIO_SCHEMA:
        raise CaptureWorkflowError("unsupported scenario schema")
    require_slug(str(scenario.get("fixture_id", "")), "fixture_id")
    if scenario.get("capture_file") != CAPTURE_FILENAME:
        raise CaptureWorkflowError(f"scenario capture_file must be {CAPTURE_FILENAME!r}")
    actual_hash = f"sha256:{sha256_file(capture_path)}"
    if scenario.get("capture_sha256") != actual_hash:
        raise CaptureWorkflowError("scenario capture SHA-256 does not match frames.mhcf")
    source = scenario.get("source")
    if not isinstance(source, dict) or source.get("kind") != "real_device":
        raise CaptureWorkflowError("scenario source.kind must be real_device")
    if not isinstance(scenario.get("detector_config"), dict):
        raise CaptureWorkflowError("scenario detector_config must be an object")
    if not validate_mhcf(capture_path).initial_replay_origin:
        raise CaptureWorkflowError("scenario capture has no deterministic replay origin")
    ground_truth = scenario.get("ground_truth")
    if not isinstance(ground_truth, dict):
        raise CaptureWorkflowError("scenario ground_truth must be an object")
    if require_reviewed and ground_truth.get("reviewed") is not True:
        raise CaptureWorkflowError("ground truth has not been marked reviewed")
    timeline = ground_truth.get("timeline")
    if not isinstance(timeline, list) or not timeline:
        raise CaptureWorkflowError("ground truth timeline must be a non-empty array")
    duration_ms = capture_duration_ms(capture_path)
    expected_start = 0
    known_motion_intervals = 0
    for index, interval in enumerate(timeline):
        if not isinstance(interval, dict):
            raise CaptureWorkflowError(f"timeline interval {index} must be an object")
        start = interval.get("start_ms")
        end = interval.get("end_ms")
        if not isinstance(start, int) or not isinstance(end, int) or start >= end:
            raise CaptureWorkflowError(f"timeline interval {index} has invalid bounds")
        if start != expected_start:
            raise CaptureWorkflowError(
                f"timeline must be gap-free; interval {index} starts at {start}, expected {expected_start}"
            )
        expected_start = end
        enum_fields = {
            "motion": MOTION_VALUES,
            "occupancy": OCCUPANCY_VALUES,
            "environment": ENVIRONMENT_VALUES,
            "evidence": EVIDENCE_VALUES,
            "confidence": CONFIDENCE_VALUES,
        }
        for field, allowed in enum_fields.items():
            if interval.get(field) not in allowed:
                raise CaptureWorkflowError(
                    f"timeline interval {index} has invalid {field}={interval.get(field)!r}"
                )
        if not isinstance(interval.get("note", ""), str):
            raise CaptureWorkflowError(f"timeline interval {index} note must be a string")
        if interval.get("motion") != "unknown":
            known_motion_intervals += 1
            for field in ("occupancy", "environment", "evidence", "confidence"):
                if interval.get(field) == "unknown":
                    raise CaptureWorkflowError(
                        f"timeline interval {index} has known motion but unknown {field}"
                    )
    if expected_start != duration_ms:
        raise CaptureWorkflowError(
            f"timeline ends at {expected_start} ms, capture ends at {duration_ms} ms"
        )
    if require_reviewed and known_motion_intervals == 0:
        raise CaptureWorkflowError("reviewed ground truth has no known motion interval")
    acceptance = scenario.get("acceptance")
    if acceptance is None and not require_acceptance_reviewed:
        return
    if not isinstance(acceptance, dict):
        raise CaptureWorkflowError("scenario acceptance must be an object")
    if not isinstance(acceptance.get("reviewed"), bool):
        raise CaptureWorkflowError("scenario acceptance.reviewed must be a boolean")
    if require_acceptance_reviewed and acceptance.get("reviewed") is not True:
        raise CaptureWorkflowError("acceptance thresholds have not been reviewed")
    for field in ACCEPTANCE_FIELDS:
        value = acceptance.get(field)
        if value is None and not require_acceptance_reviewed:
            continue
        if isinstance(value, bool) or not isinstance(value, int):
            raise CaptureWorkflowError(f"acceptance {field} must be a non-negative integer")
        if value < 0 or value >= (1 << 31):
            raise CaptureWorkflowError(
                f"acceptance {field} must fit the modular uint32 half-range"
            )


def set_acceptance(args: argparse.Namespace) -> Path:
    target = args.target.resolve()
    capture_path, _, scenario_path = resolve_capture_paths(target)
    if scenario_path is None:
        scenario_path = (
            target / SCENARIO_FILENAME
            if target.is_dir()
            else target.with_name(SCENARIO_FILENAME)
        )
    scenario = load_json(scenario_path)
    validate_scenario(
        scenario,
        capture_path,
        require_reviewed=True,
        require_acceptance_reviewed=False,
    )
    scenario["acceptance"] = {
        "reviewed": True,
        **{field: getattr(args, field) for field in ACCEPTANCE_FIELDS},
    }
    validate_scenario(scenario, capture_path, require_reviewed=True)
    atomic_write_json(scenario_path, scenario)
    return scenario_path


def overlay_interval(
    timeline: list[dict[str, Any]],
    replacement: dict[str, Any],
    *,
    replace_known: bool,
) -> list[dict[str, Any]]:
    start = replacement["start_ms"]
    end = replacement["end_ms"]
    result: list[dict[str, Any]] = []
    inserted = False
    for current in timeline:
        current_start = int(current["start_ms"])
        current_end = int(current["end_ms"])
        if current_end <= start or current_start >= end:
            if not inserted and current_start >= end:
                result.append(replacement)
                inserted = True
            result.append(current)
            continue
        current_is_unknown = all(
            current.get(field) == "unknown"
            for field in ("motion", "occupancy", "environment", "evidence", "confidence")
        )
        if not current_is_unknown and not replace_known:
            raise CaptureWorkflowError(
                "annotation overlaps reviewed data; pass --replace to overwrite it"
            )
        if current_start < start:
            before = dict(current)
            before["end_ms"] = start
            result.append(before)
        if not inserted:
            result.append(replacement)
            inserted = True
        if current_end > end:
            after = dict(current)
            after["start_ms"] = end
            result.append(after)
    if not inserted:
        result.append(replacement)
    result.sort(key=lambda item: item["start_ms"])
    return merge_adjacent_intervals(result)


def merge_adjacent_intervals(timeline: list[dict[str, Any]]) -> list[dict[str, Any]]:
    merged: list[dict[str, Any]] = []
    for interval in timeline:
        if merged:
            previous = merged[-1]
            same_labels = all(
                previous.get(field) == interval.get(field)
                for field in (
                    "motion",
                    "occupancy",
                    "environment",
                    "evidence",
                    "confidence",
                    "note",
                )
            )
            if same_labels and previous["end_ms"] == interval["start_ms"]:
                previous["end_ms"] = interval["end_ms"]
                continue
        merged.append(dict(interval))
    return merged


def annotate_capture(args: argparse.Namespace) -> Path:
    target = args.target.resolve()
    capture_path, _, scenario_path = resolve_capture_paths(target)
    if scenario_path is None:
        scenario_path = target / SCENARIO_FILENAME if target.is_dir() else target.with_name(SCENARIO_FILENAME)
    scenario = load_json(scenario_path)
    validate_scenario(scenario, capture_path, require_reviewed=False)
    ground_truth = scenario["ground_truth"]
    if args.review:
        ground_truth["reviewed"] = True
        validate_scenario(
            scenario,
            capture_path,
            require_reviewed=True,
            require_acceptance_reviewed=False,
        )
        atomic_write_json(scenario_path, scenario)
        return scenario_path

    duration_ms = capture_duration_ms(capture_path)
    if args.start_ms is None or args.end_ms is None:
        raise CaptureWorkflowError("annotation requires --from and --to")
    if args.start_ms < 0 or args.end_ms > duration_ms or args.start_ms >= args.end_ms:
        raise CaptureWorkflowError(
            f"annotation range must satisfy 0 <= from < to <= {duration_ms} ms"
        )
    replacement = {
        "start_ms": args.start_ms,
        "end_ms": args.end_ms,
        "motion": args.motion,
        "occupancy": args.occupancy,
        "environment": args.environment,
        "evidence": args.evidence,
        "confidence": args.confidence,
        "note": args.note,
    }
    ground_truth["timeline"] = overlay_interval(
        ground_truth["timeline"], replacement, replace_known=args.replace
    )
    ground_truth["reviewed"] = False
    acceptance = scenario.get("acceptance")
    if acceptance is None:
        scenario["acceptance"] = {
            "reviewed": False,
            **{field: None for field in ACCEPTANCE_FIELDS},
        }
    else:
        acceptance["reviewed"] = False
    validate_scenario(scenario, capture_path, require_reviewed=False)
    atomic_write_json(scenario_path, scenario)
    return scenario_path


class MacPseudonymizer:
    def __init__(self, reserved: Iterable[bytes] = ()) -> None:
        self._mapping: dict[bytes, bytes] = {}
        self._reserved = set(reserved)

    @staticmethod
    def _is_nonidentifying(value: bytes) -> bool:
        return value in {b"\x00" * 6, b"\xff" * 6}

    def pseudonym(self, value: bytes) -> bytes:
        if self._is_nonidentifying(value):
            return value
        existing = self._mapping.get(value)
        if existing is not None:
            return existing
        index = len(self._mapping) + 1
        while True:
            if index > 0xFFFFFF:
                raise CaptureWorkflowError("too many unique MAC addresses to anonymize")
            pseudonym = bytes(
                (
                    0x02,
                    0x00,
                    0x00,
                    (index >> 16) & 0xFF,
                    (index >> 8) & 0xFF,
                    index & 0xFF,
                )
            )
            if pseudonym not in self._reserved and pseudonym not in self._mapping.values():
                break
            index += 1
        self._mapping[value] = pseudonym
        return pseudonym

    @property
    def mapping(self) -> dict[bytes, bytes]:
        return dict(self._mapping)


def anonymized_frames(
    capture_path: Path,
    pseudonymizer: MacPseudonymizer,
) -> Iterable[CsiFrame]:
    for frame in iter_mhcf_frames(capture_path):
        yield frame.with_anonymized_macs(
            pseudonymizer.pseudonym(frame.source_mac),
            pseudonymizer.pseudonym(frame.destination_mac),
        )


def find_sensitive_metadata(value: Any, path: str = "$") -> list[str]:
    findings: list[str] = []
    if isinstance(value, dict):
        for key, child in value.items():
            lowered = str(key).lower()
            if any(part in lowered for part in SENSITIVE_KEY_PARTS):
                findings.append(f"{path}.{key} has a sensitive key")
            findings.extend(find_sensitive_metadata(child, f"{path}.{key}"))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            findings.extend(find_sensitive_metadata(child, f"{path}[{index}]"))
    elif isinstance(value, str):
        if MAC_TEXT_RE.search(value):
            findings.append(f"{path} contains a MAC-like value")
        if IPV4_TEXT_RE.search(value):
            findings.append(f"{path} contains an IPv4-like value")
        if EMAIL_TEXT_RE.search(value):
            findings.append(f"{path} contains an email-like value")
        if LOCAL_HOSTNAME_RE.search(value):
            findings.append(f"{path} contains a local-hostname-like value")
        if SENSITIVE_TEXT_LABEL_RE.search(value):
            findings.append(f"{path} contains a sensitive metadata label")
    return findings


def promote_capture(args: argparse.Namespace) -> Path:
    source_dir = args.target.resolve()
    capture_path, manifest_path, scenario_path = resolve_capture_paths(source_dir)
    if manifest_path is None or scenario_path is None:
        raise CaptureWorkflowError("promotion requires a complete raw capture directory")
    manifest = load_json(manifest_path)
    scenario = load_json(scenario_path)
    validate_manifest(capture_path, manifest)
    validate_scenario(scenario, capture_path, require_reviewed=True)
    if not args.reviewed:
        raise CaptureWorkflowError("promotion requires explicit --reviewed confirmation")
    require_promotable_provenance(scenario["source"])

    fixture_id = require_slug(args.fixture_id, "fixture-id")
    output_root = args.output_root.resolve()
    destination = output_root / fixture_id
    temporary = output_root / f".{fixture_id}.tmp"
    if destination.exists() or temporary.exists():
        raise CaptureWorkflowError(f"fixture output already exists: {destination}")
    output_root.mkdir(parents=True, exist_ok=True)
    temporary.mkdir()

    original_macs: set[bytes] = set()
    for frame in iter_mhcf_frames(capture_path):
        original_macs.add(frame.source_mac)
        original_macs.add(frame.destination_mac)
    pseudonymizer = MacPseudonymizer(original_macs)
    promoted_capture = temporary / CAPTURE_FILENAME
    try:
        validation = validate_mhcf(capture_path)
        write_mhcf(
            promoted_capture,
            validation.header.session_id,
            anonymized_frames(capture_path, pseudonymizer),
        )
        promoted_validation = validate_mhcf(promoted_capture)
        if promoted_validation.header.frame_count != validation.header.frame_count:
            raise CaptureWorkflowError("anonymization changed the frame count")

        promoted_scenario = json.loads(json.dumps(scenario))
        promoted_scenario["fixture_id"] = fixture_id
        promoted_scenario["capture_sha256"] = f"sha256:{sha256_file(promoted_capture)}"
        promoted_scenario["ground_truth"]["reviewed"] = True
        source = promoted_scenario["source"]
        promoted_scenario["source"] = {
            "kind": "real_device",
            "board_env": str(source.get("board_env", BOARD_ENV)),
            "firmware_version": str(source.get("firmware_version", "unknown")),
            "firmware_commit": str(source.get("firmware_commit", "unknown")),
            "firmware_dirty": source["firmware_dirty"],
            "firmware_identity_verified": source["firmware_identity_verified"],
            "build_target": str(source.get("build_target", "unknown")),
            "esp_platform": str(source.get("esp_platform", "unknown")),
            "sdk_version": str(source.get("sdk_version", "unknown")),
            "arduino_version": str(source.get("arduino_version", "unknown")),
        }
        require_promotable_provenance(promoted_scenario["source"])
        findings = find_sensitive_metadata(promoted_scenario)
        if findings:
            raise CaptureWorkflowError("refusing sensitive scenario metadata: " + "; ".join(findings))
        scenario_output = temporary / SCENARIO_FILENAME
        atomic_write_json(scenario_output, promoted_scenario)
        validate_scenario(promoted_scenario, promoted_capture, require_reviewed=True)

        identifying_originals = set(pseudonymizer.mapping)
        for frame in iter_mhcf_frames(promoted_capture):
            for value in (frame.source_mac, frame.destination_mac):
                if value in identifying_originals:
                    raise CaptureWorkflowError(
                        f"raw MAC {format_mac(value)} remains in a promoted identity field"
                    )
        os.replace(temporary, destination)
        return destination
    except BaseException:
        shutil.rmtree(temporary, ignore_errors=True)
        raise


def report_capture(target: Path) -> dict[str, Any]:
    capture_path, manifest_path, scenario_path = resolve_capture_paths(target.resolve())
    validation = validate_mhcf(capture_path)
    channels: Counter[int] = Counter()
    lengths: Counter[int] = Counter()
    observed_scores: list[float] = []
    non_finite_observed_scores = 0
    for frame in iter_mhcf_frames(capture_path):
        channels[frame.channel] += 1
        lengths[frame.stored_len] += 1
        score = frame.observed_motion_score
        if math.isfinite(score):
            observed_scores.append(score)
        else:
            non_finite_observed_scores += 1
    duration_ms = capture_duration_ms(capture_path)
    report: dict[str, Any] = {
        "format": "MHCF/1.0",
        "path": str(capture_path),
        "sha256": f"sha256:{sha256_file(capture_path)}",
        "bytes": capture_path.stat().st_size,
        "session_id": validation.header.session_id,
        "frame_count": validation.header.frame_count,
        "first_sequence": validation.first_sequence,
        "last_sequence": validation.last_sequence,
        "duration_ms": duration_ms,
        "average_frames_per_second": round(
            validation.header.frame_count / max(duration_ms / 1000.0, 0.001), 3
        ),
        "observed_motion_frames": validation.observed_motion_frames,
        "observed_motion_score_min": min(observed_scores) if observed_scores else None,
        "observed_motion_score_max": max(observed_scores) if observed_scores else None,
        "observed_motion_score_non_finite": non_finite_observed_scores,
        "first_word_invalid_frames": validation.first_word_invalid_frames,
        "initial_replay_origin": validation.initial_replay_origin,
        "unique_source_macs": validation.unique_source_macs,
        "unique_destination_macs": validation.unique_destination_macs,
        "channels": {str(key): value for key, value in sorted(channels.items())},
        "iq_lengths": {str(key): value for key, value in sorted(lengths.items())},
    }
    if manifest_path is not None:
        manifest = load_json(manifest_path)
        report["manifest_state"] = manifest.get("state")
        transport = manifest.get("transport")
        if isinstance(transport, dict):
            hello_data = transport.get("hello")
            end_data = transport.get("end")
            if isinstance(hello_data, dict) and isinstance(end_data, dict):
                report["source_queue_drop_delta_diagnostic"] = (
                    int(end_data.get("source_drops_end", 0))
                    - int(hello_data.get("source_drops_start", 0))
                ) & 0xFFFFFFFF
    if scenario_path is not None:
        scenario = load_json(scenario_path)
        ground_truth = scenario.get("ground_truth") or {}
        timeline = ground_truth.get("timeline") or []
        report["ground_truth_reviewed"] = ground_truth.get("reviewed") is True
        report["ground_truth_intervals"] = len(timeline)
        report["ground_truth_motion_ms"] = sum(
            int(item["end_ms"]) - int(item["start_ms"])
            for item in timeline
            if isinstance(item, dict) and item.get("motion") == "present"
        )
        report["ground_truth_unknown_ms"] = sum(
            int(item["end_ms"]) - int(item["start_ms"])
            for item in timeline
            if isinstance(item, dict) and item.get("motion") == "unknown"
        )
    return report


def verify_capture(target: Path) -> dict[str, Any]:
    capture_path, manifest_path, scenario_path = resolve_capture_paths(target.resolve())
    validation = validate_mhcf(capture_path)
    if manifest_path is not None:
        validate_manifest(capture_path, load_json(manifest_path))
    if scenario_path is not None:
        scenario = load_json(scenario_path)
        validate_scenario(
            scenario,
            capture_path,
            require_reviewed=manifest_path is None,
        )
        if manifest_path is None:
            require_promotable_provenance(scenario["source"])
    return {
        "ok": True,
        "path": str(capture_path),
        "frame_count": validation.header.frame_count,
        "sha256": f"sha256:{sha256_file(capture_path)}",
        "manifest": manifest_path is not None,
        "scenario": scenario_path is not None,
    }


def print_result(value: Any, *, as_json: bool) -> None:
    if as_json or isinstance(value, (dict, list)):
        print(
            json.dumps(
                value,
                indent=2,
                sort_keys=True,
                ensure_ascii=False,
                allow_nan=False,
            )
        )
    else:
        print(value)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    collect = subparsers.add_parser("collect", help="Collect a real-device CSI capture.")
    add_common_device_args(collect)
    collect.add_argument("--scenario-id", required=True)
    collect.add_argument("--description", default="")
    collect.add_argument(
        "--firmware-commit",
        required=True,
        type=parse_firmware_commit,
        help=(
            "Full Git SHA of the firmware actually flashed; append -dirty only "
            "for a smoke capture that will not be promoted."
        ),
    )
    collect.add_argument("--duration", type=parse_duration, default=parse_duration("2m"))
    collect.add_argument("--start-timeout", type=float, default=10.0)
    collect.add_argument("--stop-timeout", type=float, default=10.0)
    collect.add_argument("--output-root", type=Path, default=DEFAULT_RAW_ROOT)

    annotate = subparsers.add_parser("annotate", help="Add human ground truth to a capture.")
    annotate.add_argument("target", type=Path)
    annotate.add_argument("--from", dest="start_ms", type=parse_milliseconds)
    annotate.add_argument("--to", dest="end_ms", type=parse_milliseconds)
    annotate.add_argument("--motion", choices=MOTION_VALUES, default="unknown")
    annotate.add_argument("--occupancy", choices=OCCUPANCY_VALUES, default="unknown")
    annotate.add_argument("--environment", choices=ENVIRONMENT_VALUES, default="unknown")
    annotate.add_argument("--evidence", choices=EVIDENCE_VALUES, default="operator")
    annotate.add_argument("--confidence", choices=CONFIDENCE_VALUES, default="high")
    annotate.add_argument("--note", default="")
    annotate.add_argument("--replace", action="store_true")
    annotate.add_argument("--review", action="store_true")

    acceptance = subparsers.add_parser(
        "acceptance",
        help="Set and review native replay acceptance thresholds.",
    )
    acceptance.add_argument("target", type=Path)
    acceptance.add_argument(
        "--max-false-positive-ms", type=parse_milliseconds, required=True
    )
    acceptance.add_argument(
        "--max-false-negative-ms", type=parse_milliseconds, required=True
    )
    acceptance.add_argument(
        "--max-invalid-decision-ms", type=parse_milliseconds, required=True
    )
    acceptance.add_argument(
        "--max-detection-latency-ms", type=parse_milliseconds, required=True
    )
    acceptance.add_argument(
        "--max-clear-latency-ms", type=parse_milliseconds, required=True
    )
    acceptance.add_argument(
        "--max-motion-dropout-ms", type=parse_milliseconds, required=True
    )
    acceptance.add_argument(
        "--max-missed-motion-intervals", type=parse_nonnegative_int, required=True
    )
    acceptance.add_argument(
        "--max-uncleared-transitions", type=parse_nonnegative_int, required=True
    )

    verify = subparsers.add_parser("verify", help="Validate capture integrity and sidecars.")
    verify.add_argument("target", type=Path)
    verify.add_argument("--json", action="store_true")

    report = subparsers.add_parser("report", help="Report transport and annotation statistics.")
    report.add_argument("target", type=Path)
    report.add_argument("--json", action="store_true")

    promote = subparsers.add_parser("promote", help="Anonymize and promote a real capture fixture.")
    promote.add_argument("target", type=Path)
    promote.add_argument("--fixture-id", required=True)
    promote.add_argument("--output-root", type=Path, default=DEFAULT_FIXTURE_ROOT)
    promote.add_argument(
        "--reviewed",
        action="store_true",
        required=True,
        help="Confirm that the human ground-truth timeline was reviewed.",
    )
    promote.add_argument("--json", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.command == "collect":
            path = asyncio.run(collect_capture(args))
            print_result({"ok": True, "capture": str(path)}, as_json=args.json)
        elif args.command == "annotate":
            path = annotate_capture(args)
            print(path)
        elif args.command == "acceptance":
            path = set_acceptance(args)
            print(path)
        elif args.command == "verify":
            print_result(verify_capture(args.target), as_json=args.json)
        elif args.command == "report":
            print_result(report_capture(args.target), as_json=args.json)
        elif args.command == "promote":
            path = promote_capture(args)
            print_result({"ok": True, "fixture": str(path)}, as_json=args.json)
        else:  # pragma: no cover - argparse enforces a known command.
            raise CaptureWorkflowError(f"unsupported command {args.command}")
    except KeyboardInterrupt:
        print(
            "ERROR: capture interrupted; incomplete data remains in the selected .partial directory",
            file=sys.stderr,
        )
        return 130
    except Exception as exc:  # CLI boundary: preserve the incomplete manifest and report one line.
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
