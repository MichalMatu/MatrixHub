#!/usr/bin/env python3
"""Fail-closed, host-only CSI-to-alarm hardware observation gate.

The raw device snapshots are evidence, not a sharing artifact. They are written
as mode 0600 JSONL inside a mode 0700 directory. ``report.json``, ``report.md``
and ``closure-summary.json`` contain only an allow-listed, sanitized summary.

Operator markers are appended by a separate ``mark`` process. The collector
never reads stdin, so it remains safe to run under CI, nohup, or a serial-log
pipeline without competing for terminal input.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import sys
import time
from collections import Counter, defaultdict
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Iterable, Mapping, Protocol

from requests.exceptions import ConnectionError as RequestsConnectionError
from requests.exceptions import Timeout as RequestsTimeout

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args  # noqa: E402


SCHEMA = "matrixhub.csi_alarm_hardware_gate.v1"
REPORT_SCHEMA = "matrixhub.csi_alarm_hardware_gate.report.v1"
CLOSURE_SCHEMA = "matrixhub.csi_alarm_hardware_gate.closure.v1"
SHA_RE = re.compile(r"^[0-9a-f]{40}$")
BOOT_ID_RE = re.compile(r"^[0-9a-f]{16}$")
UINT32_MAX = (1 << 32) - 1
UINT32_HALF_RANGE = 1 << 31

SYSTEM_INFO_PATH = "/api/system/info"
WIFI_STATUS_PATH = "/api/wifisensing/status"
WIFI_CONFIG_PATH = "/api/wifisensing/config"
ALARM_RULES_PATH = "/api/alarms/rules?includeStatus=1"
NETWORK_PATH = "/api/system/network"
HEALTH_PATH = "/api/diagnostics/summary"
MUTEX_PATH = "/api/diagnostics/mutexes"

ENDPOINTS = (
    ("system_info", SYSTEM_INFO_PATH),
    ("wifi_sensing", WIFI_STATUS_PATH),
    ("wifi_config", WIFI_CONFIG_PATH),
    ("alarm_rules", ALARM_RULES_PATH),
    ("network", NETWORK_PATH),
)
MARKER_KINDS = (
    "quiet_start",
    "motion_start",
    "motion_stop",
    "presence_start",
    "presence_end",
    "reconnect_start",
    "reconnect_end",
    "rf_disturbance",
    "note",
)
MOTION_STATES = {
    "disabled",
    "needs_configuration",
    "calibrating",
    "monitoring",
    "motion_candidate",
    "motion_confirmed",
    "noisy_environment",
    "unavailable",
    "needs_calibration",
}
GATED_COUNTER_PATHS = {
    "queue_drops_total": ("wifi_sensing", ("csi", "queue_drops_total"), "drop"),
    "capture_records_dropped": (
        "wifi_sensing",
        ("csi", "capture", "records_dropped"),
        "drop",
    ),
    "capture_truncated_records": (
        "wifi_sensing",
        ("csi", "capture", "truncated_records"),
        "drop",
    ),
    "http_ws_queue_drops": ("health", ("http", "wsQueueDrops"), "drop"),
    "lock_standard_timeouts": (
        "mutexes",
        ("runtime", "standard", "timeouts"),
        "lock_timeout",
    ),
    "lock_recursive_timeouts": (
        "mutexes",
        ("runtime", "recursive", "timeouts"),
        "lock_timeout",
    ),
}
PREFLIGHT_U32_PATHS = {
    "wifi_sensing": {
        "csi.queue_drops_total": ("csi", "queue_drops_total"),
        "csi.capture.records_dropped": ("csi", "capture", "records_dropped"),
        "csi.capture.truncated_records": ("csi", "capture", "truncated_records"),
    },
    "health": {
        "uptimeSec": ("uptimeSec",),
        "boot.bootCount": ("boot", "bootCount"),
        "boot.unexpectedRestarts": ("boot", "unexpectedRestarts"),
        "http.wsQueueDrops": ("http", "wsQueueDrops"),
    },
    "mutexes": {
        "runtime.standard.timeouts": ("runtime", "standard", "timeouts"),
        "runtime.recursive.timeouts": ("runtime", "recursive", "timeouts"),
    },
}


class GateError(RuntimeError):
    """A safe-to-display gate error with no response body or credentials."""


class HttpResponse(Protocol):
    status_code: int

    def json(self) -> Any: ...


class HttpClient(Protocol):
    def get(self, path: str, **kwargs: Any) -> HttpResponse: ...


@dataclass(frozen=True)
class PreflightResult:
    firmware_commit: str
    firmware_dirty: bool
    hold_ms: int
    clear_hold_ms: int
    health_endpoint: str
    mutex_endpoint: str
    rule_contract: dict[str, Any]
    raw_system_info: dict[str, Any]
    raw_wifi_status: dict[str, Any]
    raw_wifi_config: dict[str, Any]
    raw_alarm_rules: dict[str, Any]
    raw_health: dict[str, Any]
    raw_mutexes: dict[str, Any]


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def host_stamp(
    utc_factory: Callable[[], str] = utc_now,
    monotonic_ns_factory: Callable[[], int] = time.monotonic_ns,
) -> dict[str, Any]:
    return {"utc": utc_factory(), "monotonic_ns": monotonic_ns_factory()}


def _is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def _is_u32(value: Any, *, allow_zero: bool) -> bool:
    return (
        type(value) is int
        and (allow_zero or value != 0)
        and 0 <= value <= UINT32_MAX
    )


def _reserved_u32_serial_delta(previous: int, current: int) -> tuple[str, int]:
    """Compare boot baseline 0 plus the wrapping 1..UINT32_MAX event ring."""

    if current == previous:
        return "same", 0
    if current == 0:
        return "regression", 0
    distance = (
        current - previous
        if current > previous
        else (UINT32_MAX - previous) + current
    )
    if distance == UINT32_HALF_RANGE:
        return "ambiguous", distance
    if distance > UINT32_HALF_RANGE:
        return "regression", distance
    return "forward", distance


def _u32_millis_delta(previous: int, current: int) -> tuple[str, int]:
    delta = (current - previous) & UINT32_MAX
    if delta == 0:
        return "same", 0
    if delta == UINT32_HALF_RANGE:
        return "ambiguous", delta
    if delta > UINT32_HALF_RANGE:
        return "regression", delta
    return "forward", delta


def _nested(data: Any, path: tuple[str, ...]) -> Any:
    current = data
    for key in path:
        if not isinstance(current, dict):
            return None
        current = current.get(key)
    return current


def _invalid_u32_fields(
    payload: Any,
    fields: Mapping[str, tuple[str, ...]],
) -> list[str]:
    return [
        label
        for label, path in fields.items()
        if not _is_u32(_nested(payload, path), allow_zero=True)
    ]


def _require_u32_contract(
    source_name: str,
    payload: Any,
    fields: Mapping[str, tuple[str, ...]],
) -> None:
    invalid = _invalid_u32_fields(payload, fields)
    if invalid:
        raise GateError(
            f"{source_name} requires exact uint32 field(s): {', '.join(sorted(invalid))}"
        )


def _response_json(response: HttpResponse, path: str) -> dict[str, Any]:
    try:
        data = response.json()
    except (TypeError, ValueError) as exc:
        raise GateError(f"{path} returned invalid JSON") from exc
    if not isinstance(data, dict):
        raise GateError(f"{path} returned a non-object JSON value")
    return data


def _get_json(client: HttpClient, path: str) -> dict[str, Any]:
    try:
        response = client.get(path)
    except Exception as exc:  # noqa: BLE001 - converted to a body-free diagnostic.
        raise GateError(f"{path} request failed ({type(exc).__name__})") from exc
    status = getattr(response, "status_code", None)
    if status != 200:
        raise GateError(f"{path} request failed with HTTP {status}")
    return _response_json(response, path)


def _enabled_csi_rules(payload: Any) -> list[dict[str, Any]]:
    if not isinstance(payload, dict) or not isinstance(payload.get("rules"), list):
        raise GateError(f"{ALARM_RULES_PATH} is missing the rules array")
    return [
        rule
        for rule in payload["rules"]
        if isinstance(rule, dict)
        and rule.get("source") == "wifi_csi_motion"
        and rule.get("enabled") is True
    ]


def _validate_canonical_rule(payload: Any) -> dict[str, Any]:
    rules = _enabled_csi_rules(payload)
    if len(rules) != 1:
        raise GateError(
            "exactly one enabled wifi_csi_motion alarm rule is required "
            f"(found {len(rules)})"
        )
    rule = rules[0]
    if rule.get("operator") != "above":
        raise GateError("wifi_csi_motion rule must use the canonical 'above' operator")
    threshold = rule.get("threshold")
    if not _is_number(threshold) or float(threshold) != 0.5:
        raise GateError("wifi_csi_motion rule must use the canonical 0.5 threshold")
    if not isinstance(rule.get("triggered"), bool):
        raise GateError("wifi_csi_motion rule has no observable boolean triggered status")
    if not _is_u32(rule.get("transition_seq"), allow_zero=True):
        raise GateError("wifi_csi_motion rule transition_seq must be a uint32")
    if not _is_u32(rule.get("device_millis"), allow_zero=True):
        raise GateError("wifi_csi_motion rule device_millis must be a uint32")
    if rule["transition_seq"] == 0 and rule["device_millis"] != 0:
        raise GateError("wifi_csi_motion rule seq 0 baseline requires device_millis 0")
    boot_id = rule.get("boot_id")
    if (
        not isinstance(boot_id, str)
        or not BOOT_ID_RE.fullmatch(boot_id)
        or boot_id == "0000000000000000"
    ):
        raise GateError("wifi_csi_motion rule boot_id must be nonzero lowercase 16-hex")
    return {
        "source": "wifi_csi_motion",
        "operator": "above",
        "threshold": 0.5,
    }


def _validate_runtime_ready_status(payload: Any) -> None:
    if not isinstance(payload, dict):
        raise GateError(f"{WIFI_STATUS_PATH} returned an invalid status object")
    csi = payload.get("csi")
    if not isinstance(csi, dict):
        raise GateError(f"{WIFI_STATUS_PATH} is missing the csi object")
    required_csi_values = {
        "enabled": True,
        "runtime_fault": False,
        "runtime_reconcile_pending": False,
        "queue_allocated": True,
        "calibration_state": "forced",
    }
    for field, expected in required_csi_values.items():
        if csi.get(field) != expected or type(csi.get(field)) is not type(expected):
            raise GateError(f"CSI runtime is not ready: csi.{field} must be {expected!r}")
    motion = csi.get("motion")
    if not isinstance(motion, dict):
        raise GateError("CSI runtime is not ready: csi.motion is missing")
    for field in ("enabled", "has_frame", "data_fresh", "decision_valid"):
        if motion.get(field) is not True:
            raise GateError(f"CSI runtime is not ready: csi.motion.{field} must be true")
    detected = motion.get("detected")
    expected_state = "motion_confirmed" if detected is True else "monitoring"
    if not isinstance(detected, bool) or motion.get("state") != expected_state:
        raise GateError(
            "CSI runtime is not ready: csi.motion.state must match the stable decision"
        )


def _validate_detector_config(payload: Any) -> tuple[int, int]:
    alarm = payload.get("csi_alarm") if isinstance(payload, dict) else None
    if not isinstance(alarm, dict) or alarm.get("enabled") is not True:
        raise GateError("Wi-Fi sensing config requires enabled csi_alarm")
    hold_ms = alarm.get("hold_ms")
    clear_hold_ms = alarm.get("clear_hold_ms")
    if not _is_u32(hold_ms, allow_zero=False) or hold_ms > 10_000:
        raise GateError("csi_alarm.hold_ms must be an exact uint32 in 1..10000")
    if not _is_u32(clear_hold_ms, allow_zero=False) or clear_hold_ms > 30_000:
        raise GateError("csi_alarm.clear_hold_ms must be an exact uint32 in 1..30000")
    return hold_ms, clear_hold_ms


def run_preflight(client: HttpClient, expected_firmware_sha: str) -> PreflightResult:
    if not SHA_RE.fullmatch(expected_firmware_sha):
        raise GateError("expected firmware SHA must be exactly 40 lowercase hexadecimal characters")

    info = _get_json(client, SYSTEM_INFO_PATH)
    actual_sha = info.get("firmware_commit")
    dirty = info.get("firmware_dirty")
    if not isinstance(actual_sha, str) or not SHA_RE.fullmatch(actual_sha):
        raise GateError("device firmware_commit is not an exact 40-character lowercase SHA")
    if actual_sha != expected_firmware_sha:
        raise GateError("device firmware_commit does not exactly match the expected SHA")
    if dirty is not False:
        raise GateError("device firmware must report firmware_dirty=false")

    alarm_rules = _get_json(client, ALARM_RULES_PATH)
    rule_contract = _validate_canonical_rule(alarm_rules)
    wifi_status = _get_json(client, WIFI_STATUS_PATH)
    _validate_runtime_ready_status(wifi_status)
    wifi_config = _get_json(client, WIFI_CONFIG_PATH)
    hold_ms, clear_hold_ms = _validate_detector_config(wifi_config)
    selected_rule = _enabled_csi_rules(alarm_rules)[0]
    detected = _nested(wifi_status, ("csi", "motion", "detected"))
    if not isinstance(detected, bool) or detected != selected_rule["triggered"]:
        raise GateError("preflight detector and wifi_csi_motion triggered state must match")
    health = _get_json(client, HEALTH_PATH)
    mutexes = _get_json(client, MUTEX_PATH)
    _require_u32_contract(
        WIFI_STATUS_PATH,
        wifi_status,
        PREFLIGHT_U32_PATHS["wifi_sensing"],
    )
    _require_u32_contract(HEALTH_PATH, health, PREFLIGHT_U32_PATHS["health"])
    _require_u32_contract(MUTEX_PATH, mutexes, PREFLIGHT_U32_PATHS["mutexes"])
    return PreflightResult(
        firmware_commit=actual_sha,
        firmware_dirty=False,
        hold_ms=hold_ms,
        clear_hold_ms=clear_hold_ms,
        health_endpoint=HEALTH_PATH,
        mutex_endpoint=MUTEX_PATH,
        rule_contract=rule_contract,
        raw_system_info=info,
        raw_wifi_status=wifi_status,
        raw_wifi_config=wifi_config,
        raw_alarm_rules=alarm_rules,
        raw_health=health,
        raw_mutexes=mutexes,
    )


class PrivateJsonlWriter:
    def __init__(self, path: Path):
        path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        os.chmod(path.parent, 0o700)
        flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
        self._fd = os.open(path, flags, 0o600)
        self._stream = os.fdopen(self._fd, "w", encoding="utf-8")

    def write(self, record: Mapping[str, Any]) -> None:
        self._stream.write(json.dumps(record, sort_keys=True, separators=(",", ":")) + "\n")
        self._stream.flush()

    def close(self) -> None:
        self._stream.close()

    def __enter__(self) -> "PrivateJsonlWriter":
        return self

    def __exit__(self, *_args: Any) -> None:
        self.close()


def _append_private_jsonl(path: Path, record: Mapping[str, Any]) -> None:
    parent_existed = path.parent.exists()
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    if not parent_existed:
        os.chmod(path.parent, 0o700)
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o600)
    try:
        os.chmod(path, 0o600)
        line = json.dumps(record, sort_keys=True, separators=(",", ":")) + "\n"
        os.write(fd, line.encode("utf-8"))
    finally:
        os.close(fd)


def append_marker(
    marker_file: Path,
    kind: str,
    label: str | None = None,
    *,
    utc_factory: Callable[[], str] = utc_now,
    monotonic_ns_factory: Callable[[], int] = time.monotonic_ns,
) -> None:
    if kind not in MARKER_KINDS:
        raise GateError(f"unsupported marker kind: {kind}")
    record: dict[str, Any] = {
        "schema": SCHEMA,
        "kind": kind,
        "created_utc": utc_factory(),
        "created_monotonic_ns": monotonic_ns_factory(),
    }
    if label:
        record["label"] = label[:200]
    _append_private_jsonl(marker_file, record)


class MarkerTail:
    """Tail complete marker-file lines without ever touching stdin."""

    def __init__(self, path: Path | None):
        self.path = path
        self.offset = path.stat().st_size if path and path.exists() else 0

    def drain(self) -> list[dict[str, Any]]:
        if self.path is None or not self.path.exists():
            return []
        records: list[dict[str, Any]] = []
        with self.path.open("rb") as stream:
            stream.seek(self.offset)
            while True:
                line_start = stream.tell()
                line = stream.readline()
                if not line:
                    break
                if not line.endswith(b"\n"):
                    self.offset = line_start
                    break
                self.offset = stream.tell()
                try:
                    value = json.loads(line)
                except (UnicodeDecodeError, json.JSONDecodeError):
                    records.append({"valid": False, "error": "invalid_json"})
                    continue
                if (
                    not isinstance(value, dict)
                    or value.get("kind") not in MARKER_KINDS
                    or type(value.get("created_monotonic_ns")) is not int
                    or value["created_monotonic_ns"] < 0
                ):
                    records.append({"valid": False, "error": "invalid_marker"})
                    continue
                marker = {
                    "valid": True,
                    "kind": value["kind"],
                    "created_utc": value.get("created_utc"),
                    "created_monotonic_ns": value["created_monotonic_ns"],
                }
                if isinstance(value.get("label"), str):
                    marker["label"] = value["label"][:200]
                records.append(marker)
        return records


def _poll_endpoint(client: HttpClient, path: str) -> dict[str, Any]:
    try:
        response = client.get(path)
    except (RequestsConnectionError, RequestsTimeout) as exc:
        return {"ok": False, "error": {"kind": "transport_error", "type": type(exc).__name__}}
    except DeviceClientError as exc:
        return {"ok": False, "error": {"kind": "client_error", "type": type(exc).__name__}}
    except Exception as exc:  # noqa: BLE001 - raw trace stores only exception type.
        return {"ok": False, "error": {"kind": "client_error", "type": type(exc).__name__}}
    status = getattr(response, "status_code", None)
    if status != 200:
        return {"ok": False, "error": {"kind": "http_status", "status": status}}
    try:
        data = response.json()
    except (TypeError, ValueError):
        return {"ok": False, "error": {"kind": "invalid_json"}}
    except Exception as exc:  # noqa: BLE001 - raw trace stores only exception type.
        return {"ok": False, "error": {"kind": "client_error", "type": type(exc).__name__}}
    if not isinstance(data, dict):
        return {"ok": False, "error": {"kind": "invalid_shape"}}
    return {"ok": True, "data": data}


def _wrapped_data(item: Any) -> dict[str, Any] | None:
    if not isinstance(item, dict) or item.get("ok") is not True:
        return None
    data = item.get("data")
    return data if isinstance(data, dict) else None


def _detector_alarm_mismatch(
    wifi_item: Any,
    alarm_item: Any,
) -> bool | None:
    wifi = _wrapped_data(wifi_item)
    alarms = _wrapped_data(alarm_item)
    rule, _ = _runtime_rule(alarms) if alarms is not None else (None, "missing")
    motion = _nested(wifi, ("csi", "motion"))
    if (
        not isinstance(motion, dict)
        or motion.get("decision_valid") is not True
        or motion.get("data_fresh") is not True
        or not isinstance(motion.get("detected"), bool)
        or rule is None
    ):
        return None
    return motion["detected"] != rule["triggered"]


def capture_sample(
    client: HttpClient,
    sequence: int,
    health_endpoint: str | None,
    mutex_endpoint: str | None = None,
    consistency_rereads: int = 2,
    consistency_reread_delay_seconds: float = 0.1,
    *,
    utc_factory: Callable[[], str] = utc_now,
    monotonic_ns_factory: Callable[[], int] = time.monotonic_ns,
    sleeper: Callable[[float], None] = time.sleep,
) -> dict[str, Any]:
    started = host_stamp(utc_factory, monotonic_ns_factory)
    snapshots = {name: _poll_endpoint(client, path) for name, path in ENDPOINTS}
    if health_endpoint:
        snapshots["health"] = _poll_endpoint(client, health_endpoint)
    if mutex_endpoint:
        snapshots["mutexes"] = _poll_endpoint(client, mutex_endpoint)
    attempts: list[dict[str, Any]] = []
    mismatch = _detector_alarm_mismatch(
        snapshots.get("wifi_sensing"),
        snapshots.get("alarm_rules"),
    )
    for _ in range(max(0, consistency_rereads)):
        if mismatch is not True:
            break
        if consistency_reread_delay_seconds > 0:
            sleeper(consistency_reread_delay_seconds)
        attempt = {
            "host": host_stamp(utc_factory, monotonic_ns_factory),
            "wifi_sensing": _poll_endpoint(client, WIFI_STATUS_PATH),
            "alarm_rules": _poll_endpoint(client, ALARM_RULES_PATH),
        }
        attempts.append(attempt)
        mismatch = _detector_alarm_mismatch(
            attempt["wifi_sensing"],
            attempt["alarm_rules"],
        )
    if attempts:
        snapshots["consistency_attempts"] = attempts
    completed = host_stamp(utc_factory, monotonic_ns_factory)
    return {
        "schema": SCHEMA,
        "record_type": "sample",
        "sequence": sequence,
        "host": {
            "utc": completed["utc"],
            "monotonic_ns": completed["monotonic_ns"],
            "started_utc": started["utc"],
            "started_monotonic_ns": started["monotonic_ns"],
            "completed_utc": completed["utc"],
            "completed_monotonic_ns": completed["monotonic_ns"],
        },
        "snapshots": snapshots,
    }


def marker_record(
    marker: Mapping[str, Any],
    sequence: int,
    *,
    utc_factory: Callable[[], str] = utc_now,
    monotonic_ns_factory: Callable[[], int] = time.monotonic_ns,
) -> dict[str, Any]:
    created_monotonic_ns = marker.get("created_monotonic_ns")
    action_time_valid = type(created_monotonic_ns) is int and created_monotonic_ns >= 0
    safe_marker = {
        "valid": marker.get("valid") is True and action_time_valid,
        "kind": marker.get("kind") if marker.get("kind") in MARKER_KINDS else None,
        "created_utc": (
            marker.get("created_utc")
            if isinstance(marker.get("created_utc"), str)
            else None
        ),
        "created_monotonic_ns": (
            created_monotonic_ns if action_time_valid else None
        ),
    }
    if isinstance(marker.get("label"), str):
        safe_marker["label"] = marker["label"][:200]
    if safe_marker["valid"] is not True:
        safe_marker["error"] = marker.get(
            "error",
            "invalid_marker_action_time" if not action_time_valid else "invalid_marker",
        )
    return {
        "schema": SCHEMA,
        "record_type": "marker",
        "sequence": sequence,
        "host": host_stamp(utc_factory, monotonic_ns_factory),
        "marker": safe_marker,
    }


def _snapshot_item(record: Mapping[str, Any], name: str) -> dict[str, Any] | None:
    snapshots = record.get("snapshots")
    if not isinstance(snapshots, dict):
        return None
    item = snapshots.get(name)
    if name in {"wifi_sensing", "alarm_rules"}:
        attempts = snapshots.get("consistency_attempts")
        if isinstance(attempts, list) and attempts:
            final_attempt = attempts[-1]
            if isinstance(final_attempt, dict):
                item = final_attempt.get(name)
    return item if isinstance(item, dict) else None


def _snapshot_data(record: Mapping[str, Any], name: str) -> dict[str, Any] | None:
    return _wrapped_data(_snapshot_item(record, name))


def _safe_int(value: Any) -> int | None:
    return value if _is_u32(value, allow_zero=True) else None


def _runtime_rule(payload: Any) -> tuple[dict[str, Any] | None, str | None]:
    try:
        rules = _enabled_csi_rules(payload)
    except GateError:
        return None, "invalid_rules_payload"
    if len(rules) != 1:
        return None, "enabled_rule_count"
    rule = rules[0]
    if rule.get("operator") != "above" or not _is_number(rule.get("threshold")):
        return None, "noncanonical_rule"
    if float(rule["threshold"]) != 0.5 or not isinstance(rule.get("triggered"), bool):
        return None, "noncanonical_rule"
    if not _is_u32(rule.get("transition_seq"), allow_zero=True):
        return None, "invalid_transition_seq"
    if not _is_u32(rule.get("device_millis"), allow_zero=True):
        return None, "invalid_device_millis"
    if rule["transition_seq"] == 0 and rule["device_millis"] != 0:
        return None, "invalid_zero_sequence_baseline"
    boot_id = rule.get("boot_id")
    if (
        not isinstance(boot_id, str)
        or not BOOT_ID_RE.fullmatch(boot_id)
        or boot_id == "0000000000000000"
    ):
        return None, "invalid_boot_id"
    return rule, None


def analyze_records(
    records: Iterable[Mapping[str, Any]],
    preflight: PreflightResult,
    *,
    require_state_cycle: bool = True,
    minimum_duration_seconds: float = 0.0,
    minimum_operator_cycles: int = 0,
    minimum_verified_reconnect_gaps: int = 0,
    minimum_motion_during_reconnect: int = 0,
    poll_interval_seconds: float = 1.0,
    operator_deadline_margin_seconds: float = 1.0,
    max_invalid_run_seconds: float = 10.0,
    max_cumulative_invalid_seconds: float = 30.0,
    max_reconnect_gap_seconds: float = 60.0,
    max_cumulative_reconnect_gap_seconds: float = 120.0,
    max_sample_gap_seconds: float = math.inf,
    minimum_sample_coverage_ratio: float = 0.0,
) -> dict[str, Any]:
    finite_non_negative = (
        minimum_duration_seconds,
        operator_deadline_margin_seconds,
        max_invalid_run_seconds,
        max_cumulative_invalid_seconds,
        max_reconnect_gap_seconds,
        max_cumulative_reconnect_gap_seconds,
        minimum_sample_coverage_ratio,
    )
    if any(not math.isfinite(value) or value < 0 for value in finite_non_negative):
        raise GateError("analysis timing/coverage limits must be finite and non-negative")
    if not math.isfinite(poll_interval_seconds) or poll_interval_seconds <= 0:
        raise GateError("analysis poll interval must be finite and positive")
    if math.isnan(max_sample_gap_seconds) or max_sample_gap_seconds <= 0:
        raise GateError("maximum sample gap must be positive and not NaN")
    if minimum_sample_coverage_ratio > 1:
        raise GateError("minimum sample coverage ratio must not exceed 1")
    if (
        type(minimum_operator_cycles) is not int
        or minimum_operator_cycles < 0
        or type(minimum_verified_reconnect_gaps) is not int
        or minimum_verified_reconnect_gaps < 0
        or type(minimum_motion_during_reconnect) is not int
        or minimum_motion_during_reconnect < 0
    ):
        raise GateError("minimum operator/reconnect counts must be non-negative integers")

    events: list[tuple[str, str, int, dict[str, Any]]] = []
    totals = Counter()
    marker_kinds = Counter()
    drop_deltas: defaultdict[str, int] = defaultdict(int)
    lock_timeout_deltas: defaultdict[str, int] = defaultdict(int)
    previous_counters: dict[str, int] = {}
    previous_boot: dict[str, int] = {}
    previous_detector: bool | None = None
    previous_alarm: bool | None = None
    previous_rule_transition: dict[str, Any] | None = None
    reconnect_open = False
    reconnect_started_action_ns: int | None = None
    reconnect_pending: dict[str, Any] | None = None
    reconnect_expected_states: list[bool] = []
    reconnect_recovery_completed = False
    mismatch_polarities: set[str] = set()
    first_monotonic_ns: int | None = None
    last_monotonic_ns: int | None = None
    maximum_sample_gap_seconds = 0.0
    maximum_capture_duration_seconds = 0.0
    invalid_run_start_ns: int | None = None
    invalid_last_ns: int | None = None
    invalid_cumulative_seconds = 0.0
    invalid_max_run_seconds = 0.0
    reconnect_gap_start_ns: int | None = None
    reconnect_gap_last_ns: int | None = None
    reconnect_gap_cumulative_seconds = 0.0
    reconnect_gap_max_run_seconds = 0.0
    operator_phase = "await_quiet_start"
    operator_pending: dict[str, Any] | None = None
    operator_latencies_ms: defaultdict[str, list[int]] = defaultdict(list)
    last_marker_action_ns: int | None = None

    poll_margin_seconds = poll_interval_seconds + operator_deadline_margin_seconds

    def begin_operator_expectation(
        step: str,
        expected: bool,
        action_ns: int,
        hold_ms: int,
        sequence: int,
    ) -> dict[str, Any]:
        return {
            "step": step,
            "expected": expected,
            "action_ns": action_ns,
            "deadline_ns": action_ns
            + int((hold_ms / 1000.0 + poll_margin_seconds) * 1_000_000_000),
            "marker_sequence": sequence,
        }

    def qualify_motion_during_observed_reconnect(pending: dict[str, Any]) -> None:
        if pending.get("qualified_motion_during_reconnect") is True:
            return
        pending["qualified_motion_during_reconnect"] = True
        pending["reconnect_candidate"] = False
        pending["suspended_for_reconnect"] = True
        pending["deadline_ns"] = None
        totals["motion_markers_during_reconnect"] += 1
        reconnect_expected_states.append(True)

    baseline_rule, _ = _runtime_rule(preflight.raw_alarm_rules)
    if baseline_rule is not None:
        previous_rule_transition = {
            "boot_id": baseline_rule["boot_id"],
            "sequence": baseline_rule["transition_seq"],
            "triggered": baseline_rule["triggered"],
            "device_millis": baseline_rule["device_millis"],
        }
        previous_alarm = baseline_rule["triggered"]
    baseline_motion = _nested(preflight.raw_wifi_status, ("csi", "motion"))
    baseline_detected = _nested(preflight.raw_wifi_status, ("csi", "motion", "detected"))
    baseline_valid = _nested(
        preflight.raw_wifi_status,
        ("csi", "motion", "decision_valid"),
    )
    baseline_fresh = _nested(preflight.raw_wifi_status, ("csi", "motion", "data_fresh"))
    if (
        isinstance(baseline_motion, dict)
        and isinstance(baseline_detected, bool)
        and baseline_valid is True
        and baseline_fresh is True
    ):
        previous_detector = baseline_detected

    baseline_sources = {
        "wifi_sensing": preflight.raw_wifi_status,
        "wifi_config": preflight.raw_wifi_config,
        "health": preflight.raw_health,
        "mutexes": preflight.raw_mutexes,
    }
    for name, (source_name, path, _category) in GATED_COUNTER_PATHS.items():
        value = _safe_int(_nested(baseline_sources[source_name], path))
        if value is not None:
            previous_counters[name] = value
    for name, value in (
        ("uptime", _safe_int(_nested(preflight.raw_health, ("uptimeSec",)))),
        ("boot_count", _safe_int(_nested(preflight.raw_health, ("boot", "bootCount")))),
        (
            "unexpected",
            _safe_int(_nested(preflight.raw_health, ("boot", "unexpectedRestarts"))),
        ),
    ):
        if value is not None:
            previous_boot[name] = value

    def add(code: str, severity: str, sequence: int, **details: Any) -> None:
        events.append((code, severity, sequence, details))

    def timeline_key(record: Mapping[str, Any]) -> tuple[int, int, int]:
        if record.get("record_type") == "marker":
            marker = record.get("marker")
            timestamp = marker.get("created_monotonic_ns") if isinstance(marker, dict) else None
            kind_order = 0
        else:
            host = record.get("host")
            timestamp = (
                host.get("completed_monotonic_ns")
                if isinstance(host, dict)
                else None
            )
            kind_order = 1
        safe_timestamp = timestamp if type(timestamp) is int and timestamp >= 0 else (1 << 127)
        sequence = record.get("sequence")
        safe_sequence = sequence if isinstance(sequence, int) else -1
        return safe_timestamp, kind_order, safe_sequence

    ordered_records = sorted(list(records), key=timeline_key)
    for record in ordered_records:
        sequence = record.get("sequence")
        sequence = sequence if isinstance(sequence, int) else -1
        if record.get("record_type") == "marker":
            marker = record.get("marker")
            if not isinstance(marker, dict) or marker.get("valid") is not True:
                add("invalid_operator_marker", "error", sequence)
                continue
            kind = marker.get("kind")
            if kind in MARKER_KINDS:
                marker_kinds[kind] += 1
            action_ns = marker.get("created_monotonic_ns")
            if (
                type(action_ns) is not int
                or action_ns < 0
                or (last_marker_action_ns is not None and action_ns < last_marker_action_ns)
            ):
                add("operator_marker_time_invalid", "error", sequence)
                continue
            last_marker_action_ns = action_ns
            if kind in {"quiet_start", "motion_start", "motion_stop"}:
                expected_phase = {
                    "quiet_start": "await_quiet_start",
                    "motion_start": "await_motion_start",
                    "motion_stop": "await_motion_stop",
                }[kind]
                if operator_pending is not None or operator_phase != expected_phase:
                    add("operator_marker_out_of_order", "error", sequence)
                elif (
                    kind == "motion_start"
                    and previous_detector is True
                    and previous_alarm is True
                ):
                    add("operator_expected_state_preexisted", "error", sequence)
                elif (
                    kind == "motion_stop"
                    and previous_detector is False
                    and previous_alarm is False
                ):
                    add("operator_expected_state_preexisted", "error", sequence)
                else:
                    expected = kind == "motion_start"
                    hold_ms = preflight.hold_ms if expected else preflight.clear_hold_ms
                    operator_pending = begin_operator_expectation(
                        kind,
                        expected,
                        action_ns,
                        hold_ms,
                        sequence,
                    )
                    if kind == "motion_start" and reconnect_open:
                        if not reconnect_recovery_completed:
                            operator_pending["reconnect_candidate"] = True
                            operator_pending["suspended_for_reconnect"] = True
                            operator_pending["deadline_ns"] = None
            if kind == "reconnect_start":
                if reconnect_open or reconnect_pending is not None:
                    add("overlapping_reconnect_marker", "error", sequence)
                reconnect_open = True
                reconnect_started_action_ns = action_ns
                reconnect_expected_states = []
                reconnect_recovery_completed = False
            elif kind == "reconnect_end":
                reconnect_open = False
                if reconnect_pending is None:
                    reconnect_started_action_ns = None
            continue
        if record.get("record_type") != "sample":
            continue

        totals["samples"] += 1
        host = record.get("host")
        sample_utc = host.get("completed_utc") if isinstance(host, dict) else None
        sample_monotonic_ns = (
            host.get("completed_monotonic_ns") if isinstance(host, dict) else None
        )
        sample_started_utc = host.get("started_utc") if isinstance(host, dict) else None
        sample_started_ns = (
            host.get("started_monotonic_ns") if isinstance(host, dict) else None
        )
        legacy_utc = host.get("utc") if isinstance(host, dict) else None
        legacy_monotonic_ns = host.get("monotonic_ns") if isinstance(host, dict) else None
        sample_window_valid = (
            isinstance(sample_started_utc, str)
            and sample_started_utc.endswith("Z")
            and isinstance(sample_utc, str)
            and sample_utc.endswith("Z")
            and legacy_utc == sample_utc
            and type(sample_started_ns) is int
            and sample_started_ns >= 0
            and type(sample_monotonic_ns) is int
            and sample_monotonic_ns >= sample_started_ns
            and legacy_monotonic_ns == sample_monotonic_ns
        )
        sample_timeline_valid = sample_window_valid and (
            last_monotonic_ns is None or sample_started_ns >= last_monotonic_ns
        )
        if not sample_timeline_valid:
            add("host_timestamp_invalid", "error", sequence)
        else:
            maximum_capture_duration_seconds = max(
                maximum_capture_duration_seconds,
                (sample_monotonic_ns - sample_started_ns) / 1_000_000_000,
            )
            if first_monotonic_ns is None:
                first_monotonic_ns = sample_monotonic_ns
            if last_monotonic_ns is not None:
                maximum_sample_gap_seconds = max(
                    maximum_sample_gap_seconds,
                    (sample_monotonic_ns - last_monotonic_ns) / 1_000_000_000,
                )
            last_monotonic_ns = sample_monotonic_ns
        snapshots = record.get("snapshots")
        attempts = snapshots.get("consistency_attempts") if isinstance(snapshots, dict) else None
        if isinstance(attempts, list) and attempts:
            totals["consistency_rereads"] += len(attempts)
            initial_mismatch = _detector_alarm_mismatch(
                snapshots.get("wifi_sensing"),
                snapshots.get("alarm_rules"),
            )
            final_attempt = attempts[-1] if isinstance(attempts[-1], dict) else {}
            final_mismatch = _detector_alarm_mismatch(
                final_attempt.get("wifi_sensing"),
                final_attempt.get("alarm_rules"),
            )
            if initial_mismatch is True and final_mismatch is False:
                totals["resolved_consistency_races"] += 1
            elif initial_mismatch is True and final_mismatch is None:
                add("consistency_reread_unresolved", "error", sequence)
        required = {
            name: _snapshot_data(record, name)
            for name in (
                "system_info",
                "wifi_sensing",
                "wifi_config",
                "alarm_rules",
                "network",
            )
        }
        health = _snapshot_data(record, "health") if preflight.health_endpoint else None
        mutexes = _snapshot_data(record, "mutexes") if preflight.mutex_endpoint else None
        if preflight.health_endpoint:
            required["health"] = health
        if preflight.mutex_endpoint:
            required["mutexes"] = mutexes
        missing = sorted(name for name, value in required.items() if value is None)
        for name in missing:
            error_kind = _nested(_snapshot_item(record, name), ("error", "kind"))
            if error_kind != "transport_error":
                add(
                    "endpoint_non_transport_failure",
                    "error",
                    sequence,
                    endpoint=name,
                    kind=error_kind if isinstance(error_kind, str) else "invalid_wrapper",
                )
        network_payload = required["network"]
        sta_connected = _nested(network_payload, ("wifi", "sta_connected"))
        network_disconnect_observed = sta_connected is False
        core_transport_errors = {
            name
            for name, _path in ENDPOINTS
            if _nested(_snapshot_item(record, name), ("error", "kind"))
            == "transport_error"
        }
        broad_transport_outage = len(core_transport_errors) >= 2
        reconnect_gap_observed = bool(missing) or network_disconnect_observed
        gap_signals = list(missing)
        if network_disconnect_observed:
            gap_signals.append("network_connectivity")
        if missing:
            totals["endpoint_gap_samples"] += 1
        if reconnect_gap_observed:
            if reconnect_pending is None and reconnect_open:
                network_evidence = network_disconnect_observed or broad_transport_outage
                if not network_evidence:
                    add(
                        "reconnect_network_evidence_missing",
                        "error",
                        sequence,
                    )
                elif (
                    sample_timeline_valid
                    and type(reconnect_started_action_ns) is int
                    and sample_started_ns >= reconnect_started_action_ns
                ):
                    reconnect_pending = {
                        "rule": dict(previous_rule_transition)
                        if previous_rule_transition is not None
                        else None,
                        "boot": dict(previous_boot),
                        "missing_endpoints": set(),
                        "action_ns": reconnect_started_action_ns,
                    }
                else:
                    add(
                        "reconnect_gap_temporally_ambiguous",
                        "error",
                        sequence,
                    )
            if reconnect_pending is not None:
                reconnect_pending["missing_endpoints"].update(gap_signals)
                if sample_timeline_valid:
                    if reconnect_gap_start_ns is None:
                        reconnect_gap_start_ns = sample_started_ns
                    reconnect_gap_last_ns = sample_monotonic_ns
                    if (
                        operator_pending is not None
                        and operator_pending.get("reconnect_candidate") is True
                        and reconnect_open
                        and not reconnect_recovery_completed
                        and (network_disconnect_observed or broad_transport_outage)
                        and operator_pending["action_ns"] <= sample_started_ns
                    ):
                        qualify_motion_during_observed_reconnect(operator_pending)
                add(
                    "planned_reconnect_gap",
                    "warning",
                    sequence,
                    missing_endpoints=gap_signals,
                )
            else:
                add(
                    "unobservable_gap",
                    "error",
                    sequence,
                    missing_endpoints=gap_signals,
                )

        for source_name, payload in (
            ("wifi_sensing", required["wifi_sensing"]),
            ("health", health),
            ("mutexes", mutexes),
        ):
            if payload is None:
                continue
            for field in _invalid_u32_fields(
                payload,
                PREFLIGHT_U32_PATHS[source_name],
            ):
                add(
                    "observability_contract_invalid",
                    "error",
                    sequence,
                    source=source_name,
                    field=field,
                )

        if (
            network_payload is not None
            and type(_nested(network_payload, ("wifi", "sta_connected"))) is not bool
        ):
            add(
                "observability_contract_invalid",
                "error",
                sequence,
                source="network",
                field="wifi.sta_connected",
            )

        info = required["system_info"]
        if info is not None:
            if (
                info.get("firmware_commit") != preflight.firmware_commit
                or info.get("firmware_dirty") is not False
            ):
                add("firmware_identity_drift", "error", sequence)

        if (
            required["wifi_config"] is not None
            and required["wifi_config"] != preflight.raw_wifi_config
        ):
            add("detector_config_drift", "error", sequence)

        if sta_connected is False:
            add("wifi_disconnected", "warning", sequence)

        wifi = required["wifi_sensing"]
        alarms = required["alarm_rules"]
        rule, rule_error = _runtime_rule(alarms) if alarms is not None else (None, "missing")
        if alarms is not None and rule_error:
            add("rule_contract_drift", "error", sequence, reason=rule_error)

        prior_rule_transition = previous_rule_transition
        current_rule_transition: dict[str, Any] | None = None
        if rule is not None:
            current_rule_transition = {
                "boot_id": rule["boot_id"],
                "sequence": rule["transition_seq"],
                "triggered": rule["triggered"],
                "device_millis": rule["device_millis"],
            }
            if previous_rule_transition is not None:
                if current_rule_transition["boot_id"] != previous_rule_transition["boot_id"]:
                    add("rule_boot_id_changed", "error", sequence)
                    serial_kind, serial_delta = "boot_changed", 0
                else:
                    serial_kind, serial_delta = _reserved_u32_serial_delta(
                        previous_rule_transition["sequence"],
                        current_rule_transition["sequence"],
                    )
                if serial_kind == "same":
                    if (
                        current_rule_transition["triggered"]
                        != previous_rule_transition["triggered"]
                        or current_rule_transition["device_millis"]
                        != previous_rule_transition["device_millis"]
                    ):
                        add("transition_metadata_inconsistent", "error", sequence)
                elif serial_kind == "forward" and serial_delta == 1:
                    if (
                        current_rule_transition["triggered"]
                        == previous_rule_transition["triggered"]
                    ):
                        add("transition_state_did_not_toggle", "error", sequence)
                    millis_kind, _ = _u32_millis_delta(
                        previous_rule_transition["device_millis"],
                        current_rule_transition["device_millis"],
                    )
                    if millis_kind != "forward":
                        add(
                            "transition_millis_not_advanced",
                            "error",
                            sequence,
                            reason=millis_kind,
                        )
                    totals["rule_transitions"] += 1
                elif serial_kind == "forward":
                    add(
                        "missed_rule_transition",
                        "error",
                        sequence,
                        delta=serial_delta,
                    )
                elif serial_kind != "boot_changed":
                    add(
                        "transition_sequence_invalid",
                        "error",
                        sequence,
                        reason=serial_kind,
                    )
            previous_rule_transition = current_rule_transition

        csi = wifi.get("csi") if isinstance(wifi, dict) else None
        motion = csi.get("motion") if isinstance(csi, dict) else None
        if wifi is not None and not isinstance(csi, dict):
            add("csi_runtime_not_ready", "error", sequence, field="csi")
        elif isinstance(csi, dict):
            if csi.get("runtime_fault") is True:
                add("csi_runtime_fault", "error", sequence)
            if csi.get("runtime_reconcile_pending") is True:
                add("csi_runtime_reconcile_pending", "error", sequence)
            runtime_contract = {
                "enabled": True,
                "runtime_fault": False,
                "runtime_reconcile_pending": False,
                "queue_allocated": True,
                "calibration_state": "forced",
            }
            for field, expected in runtime_contract.items():
                if csi.get(field) != expected or type(csi.get(field)) is not type(expected):
                    add("csi_runtime_not_ready", "error", sequence, field=field)
            if not isinstance(motion, dict):
                add("csi_runtime_not_ready", "error", sequence, field="motion")
            else:
                for field in (
                    "enabled",
                    "has_frame",
                    "decision_valid",
                    "data_fresh",
                    "detected",
                ):
                    if type(motion.get(field)) is not bool:
                        add(
                            "csi_runtime_contract_invalid",
                            "error",
                            sequence,
                            field=f"motion.{field}",
                        )
                state = motion.get("state")
                if type(state) is not str or state not in MOTION_STATES:
                    add(
                        "csi_runtime_contract_invalid",
                        "error",
                        sequence,
                        field="motion.state",
                    )
                if motion.get("enabled") is not True:
                    add("csi_runtime_not_ready", "error", sequence, field="motion.enabled")
                if motion.get("has_frame") is not True:
                    add("csi_runtime_not_ready", "error", sequence, field="motion.has_frame")
                if (
                    type(motion.get("decision_valid")) is bool
                    and motion.get("decision_valid") is True
                    and type(motion.get("detected")) is bool
                ):
                    expected_state = (
                        "motion_confirmed"
                        if motion.get("detected") is True
                        else "monitoring"
                    )
                    if state != expected_state:
                        add("csi_runtime_not_ready", "error", sequence, field="motion.state")

        counter_sources = {
            "wifi_sensing": wifi,
            "health": health,
            "mutexes": mutexes,
        }
        for name, (source_name, path, category) in GATED_COUNTER_PATHS.items():
            source_payload = counter_sources[source_name]
            current = _safe_int(_nested(source_payload, path))
            if current is None:
                continue
            previous = previous_counters.get(name)
            if previous is not None and current > previous:
                delta = current - previous
                if category == "lock_timeout":
                    lock_timeout_deltas[name] += delta
                    add("lock_timeout_delta", "error", sequence, counter=name, delta=delta)
                else:
                    drop_deltas[name] += delta
                    add("drop_counter_delta", "error", sequence, counter=name, delta=delta)
            elif previous is not None and current < previous:
                add("gated_counter_reset", "error", sequence, counter=name)
            previous_counters[name] = current

        detected = motion.get("detected") if isinstance(motion, dict) else None
        decision_valid = motion.get("decision_valid") if isinstance(motion, dict) else None
        data_fresh = motion.get("data_fresh") if isinstance(motion, dict) else None
        triggered = rule.get("triggered") if isinstance(rule, dict) else None
        detector_before_sample = previous_detector
        alarm_before_sample = previous_alarm
        detector_observable = (
            isinstance(detected, bool) and decision_valid is True and data_fresh is True
        )
        alarm_observable = isinstance(triggered, bool)
        if wifi is None or not isinstance(motion, dict):
            pass
        elif not detector_observable:
            totals["invalid_or_stale_samples"] += 1
            add("detector_unobservable", "warning", sequence)
            if alarm_observable and previous_alarm is not None and triggered != previous_alarm:
                add("invalid_retention_violation", "error", sequence)
        elif alarm_observable:
            totals["comparable_samples"] += 1
            totals[f"detector_{str(detected).lower()}_samples"] += 1
            if detected != triggered:
                if detected is True:
                    mismatch_polarities.add("false_clear")
                    add("false_clear", "error", sequence)
                else:
                    mismatch_polarities.add("false_trigger")
                    add("false_trigger", "error", sequence)
            if previous_detector is not None and detected != previous_detector:
                totals["detector_transitions"] += 1
            if previous_alarm is not None and triggered != previous_alarm:
                totals["alarm_transitions"] += 1
            previous_detector = detected

        if alarm_observable:
            previous_alarm = triggered

        timestamp_valid = sample_timeline_valid
        detector_invalid = (
            wifi is not None
            and isinstance(motion, dict)
            and not detector_observable
        )
        detector_endpoint_gap = wifi is None or not isinstance(motion, dict)
        if timestamp_valid:
            if detector_invalid:
                if invalid_run_start_ns is None:
                    invalid_run_start_ns = sample_monotonic_ns
                invalid_last_ns = sample_monotonic_ns
            elif invalid_run_start_ns is not None:
                run_end_ns = (
                    invalid_last_ns
                    if detector_endpoint_gap and invalid_last_ns is not None
                    else sample_monotonic_ns
                )
                run_seconds = max(0.0, (run_end_ns - invalid_run_start_ns) / 1_000_000_000)
                invalid_max_run_seconds = max(invalid_max_run_seconds, run_seconds)
                invalid_cumulative_seconds += run_seconds
                invalid_run_start_ns = None
                invalid_last_ns = None

            if operator_pending is not None:
                action_ns = operator_pending["action_ns"]
                sample_crosses_action = sample_started_ns < action_ns <= sample_monotonic_ns
                sample_started_after_action = sample_started_ns >= action_ns
                if sample_crosses_action:
                    crossing_is_causally_safe = (
                        operator_pending["step"] == "quiet_start"
                        or (
                            detector_observable
                            and alarm_observable
                            and isinstance(detector_before_sample, bool)
                            and isinstance(alarm_before_sample, bool)
                            and detected is not operator_pending["expected"]
                            and triggered is not operator_pending["expected"]
                            and detected == detector_before_sample
                            and triggered == alarm_before_sample
                            and prior_rule_transition is not None
                            and current_rule_transition == prior_rule_transition
                        )
                    )
                    if not crossing_is_causally_safe:
                        add(
                            "operator_response_temporally_ambiguous",
                            "error",
                            sequence,
                            step=operator_pending["step"],
                        )
                        operator_pending = None
                        operator_phase = "failed"
                elif not sample_started_after_action:
                    add("operator_sample_precedes_marker", "error", sequence)
                else:
                    can_evaluate_operator = True
                    if operator_pending.get("suspended_for_reconnect") is True:
                        if (
                            reconnect_gap_observed
                            or not detector_observable
                            or not alarm_observable
                        ):
                            can_evaluate_operator = False
                        else:
                            operator_pending["suspended_for_reconnect"] = False
                            hold_ms = (
                                preflight.hold_ms
                                if operator_pending["expected"] is True
                                else preflight.clear_hold_ms
                            )
                            operator_pending["deadline_ns"] = sample_monotonic_ns + int(
                                (hold_ms / 1000.0 + poll_margin_seconds)
                                * 1_000_000_000
                            )
                    deadline_ns = operator_pending.get("deadline_ns")
                    if can_evaluate_operator and type(deadline_ns) is int:
                        reached = (
                            detector_observable
                            and alarm_observable
                            and detected is operator_pending["expected"]
                            and triggered is operator_pending["expected"]
                        )
                        if reached and sample_monotonic_ns <= deadline_ns:
                            step = operator_pending["step"]
                            latency_ms = (sample_monotonic_ns - action_ns) // 1_000_000
                            operator_latencies_ms[step].append(int(latency_ms))
                            if step == "quiet_start":
                                operator_phase = "await_motion_start"
                            elif step == "motion_start":
                                operator_phase = "await_motion_stop"
                            else:
                                operator_phase = "await_quiet_start"
                                totals["verified_operator_cycles"] += 1
                            operator_pending = None
                        elif sample_monotonic_ns > deadline_ns:
                            add(
                                "operator_response_deadline_missed",
                                "error",
                                sequence,
                                step=operator_pending["step"],
                            )
                            operator_pending = None
                            operator_phase = "failed"

        uptime = _safe_int(_nested(health, ("uptimeSec",)))
        boot_count = _safe_int(_nested(health, ("boot", "bootCount")))
        unexpected = _safe_int(_nested(health, ("boot", "unexpectedRestarts")))
        if uptime is None:
            uptime = _safe_int(_nested(info, ("uptime",)))
        restart_signals: list[str] = []
        if uptime is not None and "uptime" in previous_boot and uptime < previous_boot["uptime"]:
            restart_signals.append("uptime")
        if (
            boot_count is not None
            and "boot_count" in previous_boot
            and boot_count > previous_boot["boot_count"]
        ):
            restart_signals.append("boot_count")
        if (
            unexpected is not None
            and "unexpected" in previous_boot
            and unexpected > previous_boot["unexpected"]
        ):
            restart_signals.append("unexpected_restarts")
        if restart_signals:
            totals["restarts"] += 1
            add("device_restart", "error", sequence, signals=sorted(restart_signals))
            if (
                prior_rule_transition is not None
                and current_rule_transition is not None
                and prior_rule_transition["boot_id"] == current_rule_transition["boot_id"]
            ):
                add("boot_id_not_rotated_after_reboot", "error", sequence)
        if uptime is not None:
            previous_boot["uptime"] = uptime
        if boot_count is not None:
            previous_boot["boot_count"] = boot_count
        if unexpected is not None:
            previous_boot["unexpected"] = unexpected

        if not missing and sta_connected is True and reconnect_pending is not None:
            if reconnect_gap_start_ns is not None and type(sample_monotonic_ns) is int:
                gap_seconds = max(
                    0.0,
                    (sample_monotonic_ns - reconnect_gap_start_ns) / 1_000_000_000,
                )
                reconnect_gap_max_run_seconds = max(
                    reconnect_gap_max_run_seconds,
                    gap_seconds,
                )
                reconnect_gap_cumulative_seconds += gap_seconds
                reconnect_gap_start_ns = None
                reconnect_gap_last_ns = None
            before_rule = reconnect_pending.get("rule")
            before_boot = reconnect_pending.get("boot")
            current_boot = {
                "uptime": uptime,
                "boot_count": boot_count,
                "unexpected": unexpected,
            }
            boot_proven = (
                isinstance(before_boot, dict)
                and all(
                    type(before_boot.get(key)) is int and type(current_boot.get(key)) is int
                    for key in ("uptime", "boot_count", "unexpected")
                )
                and current_boot["boot_count"] == before_boot["boot_count"]
                and current_boot["unexpected"] == before_boot["unexpected"]
                and current_boot["uptime"] >= before_boot["uptime"]
            )
            continuity_proven = False
            if isinstance(before_rule, dict) and current_rule_transition is not None:
                unchanged = current_rule_transition == before_rule
                expected_state = before_rule["triggered"]
                expected_transition_count = 0
                for state in reconnect_expected_states:
                    if state != expected_state:
                        expected_state = state
                        expected_transition_count += 1
                expected_committed = False
                if (
                    expected_transition_count > 0
                    and current_rule_transition["boot_id"] == before_rule["boot_id"]
                    and current_rule_transition["triggered"] == expected_state
                ):
                    seq_kind, seq_delta = _reserved_u32_serial_delta(
                        before_rule["sequence"],
                        current_rule_transition["sequence"],
                    )
                    millis_kind, _ = _u32_millis_delta(
                        before_rule["device_millis"],
                        current_rule_transition["device_millis"],
                    )
                    expected_committed = (
                        seq_kind == "forward"
                        and seq_delta == expected_transition_count
                        and millis_kind == "forward"
                    )
                continuity_proven = unchanged or expected_committed
            if not boot_proven:
                add("reconnect_reboot_unverified", "error", sequence)
            if not continuity_proven:
                add("reconnect_hidden_or_unverified_transition", "error", sequence)
            if boot_proven and continuity_proven:
                totals["verified_reconnect_gaps"] += 1
            reconnect_pending = None
            reconnect_expected_states = []
            reconnect_recovery_completed = True
            reconnect_started_action_ns = None

    if mismatch_polarities == {"false_clear", "false_trigger"}:
        add("inverted_alarm_semantics", "error", -1)
    if totals["samples"] == 0:
        add("no_samples", "error", -1)
    if totals["comparable_samples"] == 0:
        add("no_comparable_samples", "error", -1)
    if require_state_cycle and (
        totals["detector_true_samples"] == 0
        or totals["detector_false_samples"] == 0
        or totals["detector_transitions"] < 2
        or totals["alarm_transitions"] < 2
        or totals["rule_transitions"] < 2
    ):
        add("insufficient_state_cycle_coverage", "error", -1)
    observed_duration_seconds = (
        (last_monotonic_ns - first_monotonic_ns) / 1_000_000_000
        if first_monotonic_ns is not None and last_monotonic_ns is not None
        else 0.0
    )
    if observed_duration_seconds < minimum_duration_seconds:
        add("observation_duration_too_short", "error", -1)
    expected_sample_count = max(
        1,
        int(observed_duration_seconds / poll_interval_seconds) + 1,
    )
    sample_coverage_ratio = min(1.0, totals["samples"] / expected_sample_count)
    if maximum_sample_gap_seconds > max_sample_gap_seconds:
        add("sample_cadence_gap_exceeded", "error", -1)
    if sample_coverage_ratio < minimum_sample_coverage_ratio:
        add("sample_coverage_too_low", "error", -1)

    if invalid_run_start_ns is not None and invalid_last_ns is not None:
        run_seconds = max(
            0.0,
            (invalid_last_ns - invalid_run_start_ns) / 1_000_000_000,
        )
        invalid_max_run_seconds = max(invalid_max_run_seconds, run_seconds)
        invalid_cumulative_seconds += run_seconds
        add("detector_invalid_at_end", "error", -1)
    if invalid_max_run_seconds > max_invalid_run_seconds:
        add("invalid_decision_run_exceeded", "error", -1)
    if invalid_cumulative_seconds > max_cumulative_invalid_seconds:
        add("invalid_decision_cumulative_exceeded", "error", -1)

    if reconnect_gap_start_ns is not None and reconnect_gap_last_ns is not None:
        gap_seconds = max(
            0.0,
            (reconnect_gap_last_ns - reconnect_gap_start_ns) / 1_000_000_000,
        )
        reconnect_gap_max_run_seconds = max(reconnect_gap_max_run_seconds, gap_seconds)
        reconnect_gap_cumulative_seconds += gap_seconds
    if reconnect_gap_max_run_seconds > max_reconnect_gap_seconds:
        add("reconnect_gap_run_exceeded", "error", -1)
    if reconnect_gap_cumulative_seconds > max_cumulative_reconnect_gap_seconds:
        add("reconnect_gap_cumulative_exceeded", "error", -1)

    if operator_pending is not None:
        add(
            "operator_response_unverified",
            "error",
            -1,
            step=operator_pending["step"],
        )
    if totals["verified_operator_cycles"] < minimum_operator_cycles:
        add("operator_cycle_requirement_not_met", "error", -1)
    if totals["verified_reconnect_gaps"] < minimum_verified_reconnect_gaps:
        add("verified_reconnect_requirement_not_met", "error", -1)
    if totals["motion_markers_during_reconnect"] < minimum_motion_during_reconnect:
        add("motion_during_reconnect_requirement_not_met", "error", -1)
    if reconnect_pending is not None:
        add(
            "reconnect_unverified_gap",
            "error",
            -1,
            missing_endpoints=sorted(reconnect_pending["missing_endpoints"]),
        )
    if reconnect_open:
        add("reconnect_marker_unclosed", "error", -1)
    grouped: dict[str, dict[str, Any]] = {}
    severity_rank = {"warning": 0, "error": 1}
    for code, severity, sequence, details in events:
        item = grouped.setdefault(
            code,
            {
                "code": code,
                "severity": severity,
                "count": 0,
                "first_sequence": sequence,
                "last_sequence": sequence,
            },
        )
        item["count"] += 1
        item["first_sequence"] = min(item["first_sequence"], sequence)
        item["last_sequence"] = max(item["last_sequence"], sequence)
        if severity_rank[severity] > severity_rank[item["severity"]]:
            item["severity"] = severity
        if code in {
            "unobservable_gap",
            "planned_reconnect_gap",
            "reconnect_unverified_gap",
        }:
            names = item.setdefault("missing_endpoints", [])
            names.extend(details.get("missing_endpoints", []))
        if code == "device_restart":
            signals = item.setdefault("signals", [])
            signals.extend(details.get("signals", []))
    for item in grouped.values():
        if "missing_endpoints" in item:
            item["missing_endpoints"] = sorted(set(item["missing_endpoints"]))
        if "signals" in item:
            item["signals"] = sorted(set(item["signals"]))

    findings = [grouped[code] for code in sorted(grouped)]
    verdict = "fail" if any(item["severity"] == "error" for item in findings) else "pass"
    return {
        "schema": REPORT_SCHEMA,
        "verdict": verdict,
        "firmware": {"commit": preflight.firmware_commit, "dirty": False},
        "rule_contract": dict(preflight.rule_contract),
        "health_observed": preflight.health_endpoint is not None,
        "observation": {
            "duration_seconds": round(observed_duration_seconds, 3),
            "minimum_required_seconds": round(minimum_duration_seconds, 3),
            "maximum_sample_gap_seconds": round(maximum_sample_gap_seconds, 3),
            "maximum_capture_duration_seconds": round(
                maximum_capture_duration_seconds,
                3,
            ),
            "maximum_allowed_sample_gap_seconds": (
                round(max_sample_gap_seconds, 3)
                if math.isfinite(max_sample_gap_seconds)
                else None
            ),
            "sample_coverage_ratio": round(sample_coverage_ratio, 6),
            "minimum_sample_coverage_ratio": round(minimum_sample_coverage_ratio, 6),
        },
        "samples": {
            "total": totals["samples"],
            "comparable": totals["comparable_samples"],
            "invalid_or_stale": totals["invalid_or_stale_samples"],
            "endpoint_gap": totals["endpoint_gap_samples"],
            "detector_true": totals["detector_true_samples"],
            "detector_false": totals["detector_false_samples"],
        },
        "transitions": {
            "detector": totals["detector_transitions"],
            "alarm": totals["alarm_transitions"],
            "rule_metadata": totals["rule_transitions"],
        },
        "markers": {
            "total": sum(marker_kinds.values()),
            "kinds": dict(sorted(marker_kinds.items())),
        },
        "operator": {
            "verified_cycles": totals["verified_operator_cycles"],
            "minimum_required_cycles": minimum_operator_cycles,
            "hold_ms": preflight.hold_ms,
            "clear_hold_ms": preflight.clear_hold_ms,
            "poll_margin_seconds": round(poll_margin_seconds, 3),
            "latencies_ms": {
                key: list(values)
                for key, values in sorted(operator_latencies_ms.items())
            },
        },
        "detector_observability": {
            "max_invalid_run_seconds": round(invalid_max_run_seconds, 3),
            "max_allowed_invalid_run_seconds": round(max_invalid_run_seconds, 3),
            "cumulative_invalid_seconds": round(invalid_cumulative_seconds, 3),
            "max_allowed_cumulative_invalid_seconds": round(
                max_cumulative_invalid_seconds,
                3,
            ),
        },
        "reconnect_observability": {
            "max_gap_seconds": round(reconnect_gap_max_run_seconds, 3),
            "max_allowed_gap_seconds": round(max_reconnect_gap_seconds, 3),
            "cumulative_gap_seconds": round(reconnect_gap_cumulative_seconds, 3),
            "max_allowed_cumulative_gap_seconds": round(
                max_cumulative_reconnect_gap_seconds,
                3,
            ),
        },
        "runtime": {
            "restarts": totals["restarts"],
            "verified_reconnect_gaps": totals["verified_reconnect_gaps"],
            "minimum_required_reconnect_gaps": minimum_verified_reconnect_gaps,
            "motion_markers_during_reconnect": totals[
                "motion_markers_during_reconnect"
            ],
            "minimum_required_motion_during_reconnect": minimum_motion_during_reconnect,
            "drop_deltas": dict(sorted(drop_deltas.items())),
            "lock_timeout_deltas": dict(sorted(lock_timeout_deltas.items())),
        },
        "consistency": {
            "rereads": totals["consistency_rereads"],
            "resolved_races": totals["resolved_consistency_races"],
        },
        "findings": findings,
    }


def build_closure_summary(report: Mapping[str, Any]) -> dict[str, Any]:
    """Build a strict allow-list summary; unknown/raw keys cannot escape."""

    findings = []
    raw_findings = report.get("findings")
    if isinstance(raw_findings, list):
        for item in raw_findings:
            if not isinstance(item, dict):
                continue
            findings.append(
                {
                    "code": str(item.get("code", "unknown")),
                    "severity": "error" if item.get("severity") == "error" else "warning",
                    "count": (
                        int(item.get("count", 0))
                        if isinstance(item.get("count"), int)
                        else 0
                    ),
                }
            )
    firmware = report.get("firmware") if isinstance(report.get("firmware"), dict) else {}
    samples = report.get("samples") if isinstance(report.get("samples"), dict) else {}
    transitions = report.get("transitions") if isinstance(report.get("transitions"), dict) else {}
    runtime = report.get("runtime") if isinstance(report.get("runtime"), dict) else {}
    observation = report.get("observation") if isinstance(report.get("observation"), dict) else {}
    operator = report.get("operator") if isinstance(report.get("operator"), dict) else {}
    detector_observability = (
        report.get("detector_observability")
        if isinstance(report.get("detector_observability"), dict)
        else {}
    )
    reconnect_observability = (
        report.get("reconnect_observability")
        if isinstance(report.get("reconnect_observability"), dict)
        else {}
    )
    consistency = report.get("consistency") if isinstance(report.get("consistency"), dict) else {}
    markers = report.get("markers") if isinstance(report.get("markers"), dict) else {}
    marker_kinds = markers.get("kinds") if isinstance(markers.get("kinds"), dict) else {}
    evidence = report.get("evidence") if isinstance(report.get("evidence"), dict) else {}
    raw_trace_hash = evidence.get("trace_sha256")
    safe_trace_hash = (
        raw_trace_hash
        if isinstance(raw_trace_hash, str)
        and re.fullmatch(r"sha256:[0-9a-f]{64}", raw_trace_hash)
        else "unavailable"
    )
    return {
        "schema": CLOSURE_SCHEMA,
        "verdict": "pass" if report.get("verdict") == "pass" else "fail",
        "firmware_commit": (
            firmware.get("commit")
            if isinstance(firmware.get("commit"), str)
            else "unknown"
        ),
        "trace_sha256": safe_trace_hash,
        "health_observed": report.get("health_observed") is True,
        "observation": {
            key: float(observation.get(key, 0.0))
            if _is_number(observation.get(key))
            else 0.0
            for key in (
                "duration_seconds",
                "minimum_required_seconds",
                "maximum_sample_gap_seconds",
                "maximum_capture_duration_seconds",
                "maximum_allowed_sample_gap_seconds",
                "sample_coverage_ratio",
                "minimum_sample_coverage_ratio",
            )
        },
        "samples": {
            key: int(samples.get(key, 0)) if isinstance(samples.get(key), int) else 0
            for key in (
                "total",
                "comparable",
                "invalid_or_stale",
                "endpoint_gap",
                "detector_true",
                "detector_false",
            )
        },
        "transitions": {
            key: int(transitions.get(key, 0)) if isinstance(transitions.get(key), int) else 0
            for key in ("detector", "alarm", "rule_metadata")
        },
        "runtime": {
            "restarts": (
                int(runtime.get("restarts", 0))
                if isinstance(runtime.get("restarts"), int)
                else 0
            ),
            "verified_reconnect_gaps": (
                int(runtime.get("verified_reconnect_gaps", 0))
                if isinstance(runtime.get("verified_reconnect_gaps"), int)
                else 0
            ),
            "minimum_required_reconnect_gaps": (
                int(runtime.get("minimum_required_reconnect_gaps", 0))
                if isinstance(runtime.get("minimum_required_reconnect_gaps"), int)
                else 0
            ),
            "motion_markers_during_reconnect": (
                int(runtime.get("motion_markers_during_reconnect", 0))
                if isinstance(runtime.get("motion_markers_during_reconnect"), int)
                else 0
            ),
            "minimum_required_motion_during_reconnect": (
                int(runtime.get("minimum_required_motion_during_reconnect", 0))
                if isinstance(runtime.get("minimum_required_motion_during_reconnect"), int)
                else 0
            ),
            "drop_delta_total": sum(
                value
                for value in (runtime.get("drop_deltas") or {}).values()
                if isinstance(value, int) and value >= 0
            )
            if isinstance(runtime.get("drop_deltas"), dict)
            else 0,
            "lock_timeout_delta_total": sum(
                value
                for value in (runtime.get("lock_timeout_deltas") or {}).values()
                if isinstance(value, int) and value >= 0
            )
            if isinstance(runtime.get("lock_timeout_deltas"), dict)
            else 0,
        },
        "operator": {
            "verified_cycles": int(operator.get("verified_cycles", 0))
            if isinstance(operator.get("verified_cycles"), int)
            else 0,
            "minimum_required_cycles": int(operator.get("minimum_required_cycles", 0))
            if isinstance(operator.get("minimum_required_cycles"), int)
            else 0,
            "hold_ms": int(operator.get("hold_ms", 0))
            if isinstance(operator.get("hold_ms"), int)
            else 0,
            "clear_hold_ms": int(operator.get("clear_hold_ms", 0))
            if isinstance(operator.get("clear_hold_ms"), int)
            else 0,
        },
        "detector_observability": {
            key: float(detector_observability.get(key, 0.0))
            if _is_number(detector_observability.get(key))
            else 0.0
            for key in (
                "max_invalid_run_seconds",
                "max_allowed_invalid_run_seconds",
                "cumulative_invalid_seconds",
                "max_allowed_cumulative_invalid_seconds",
            )
        },
        "reconnect_observability": {
            key: float(reconnect_observability.get(key, 0.0))
            if _is_number(reconnect_observability.get(key))
            else 0.0
            for key in (
                "max_gap_seconds",
                "max_allowed_gap_seconds",
                "cumulative_gap_seconds",
                "max_allowed_cumulative_gap_seconds",
            )
        },
        "consistency": {
            key: int(consistency.get(key, 0))
            if isinstance(consistency.get(key), int)
            else 0
            for key in ("rereads", "resolved_races")
        },
        "markers": {
            "total": int(markers.get("total", 0)) if isinstance(markers.get("total"), int) else 0,
            "kinds": {
                kind: int(marker_kinds.get(kind, 0))
                for kind in MARKER_KINDS
                if isinstance(marker_kinds.get(kind), int) and marker_kinds.get(kind, 0) > 0
            },
        },
        "findings": findings,
        "evidence": "Raw endpoint snapshots remain in the private trace.jsonl artifact.",
    }


def render_report_markdown(report: Mapping[str, Any]) -> str:
    closure = build_closure_summary(report)
    samples = closure["samples"]
    runtime = closure["runtime"]
    transitions = closure["transitions"]
    observation = closure["observation"]
    operator = closure["operator"]
    detector_observability = closure["detector_observability"]
    reconnect_observability = closure["reconnect_observability"]
    consistency = closure["consistency"]
    lines = [
        "# CSI to alarm hardware gate",
        "",
        f"- Verdict: **{str(closure['verdict']).upper()}**",
        f"- Firmware commit: `{closure['firmware_commit']}`",
        f"- Private trace: `{closure['trace_sha256']}`",
        f"- Observation: {observation['duration_seconds']:.3f}s "
        f"(required {observation['minimum_required_seconds']:.3f}s)",
        f"- Cadence: max gap {observation['maximum_sample_gap_seconds']:.3f}s, "
        f"coverage {observation['sample_coverage_ratio']:.3f}",
        f"- Longest capture window: "
        f"{observation['maximum_capture_duration_seconds']:.3f}s",
        f"- Samples: {samples['total']} total, {samples['comparable']} comparable, "
        f"{samples['invalid_or_stale']} invalid/stale, {samples['endpoint_gap']} endpoint-gap",
        f"- Detector states: {samples['detector_true']} motion, "
        f"{samples['detector_false']} clear",
        f"- Transitions: detector {transitions['detector']}, alarm {transitions['alarm']}, "
        f"rule metadata {transitions['rule_metadata']}",
        f"- Runtime: {runtime['restarts']} restarts, "
        f"{runtime['drop_delta_total']} drop-counter delta, "
        f"{runtime['lock_timeout_delta_total']} lock-timeout delta",
        f"- Verified reconnect gaps: {runtime['verified_reconnect_gaps']} "
        f"(required {runtime['minimum_required_reconnect_gaps']})",
        f"- Motion markers during reconnect: {runtime['motion_markers_during_reconnect']} "
        f"(required {runtime['minimum_required_motion_during_reconnect']})",
        f"- Verified operator cycles: {operator['verified_cycles']} "
        f"(required {operator['minimum_required_cycles']})",
        f"- Invalid/stale decisions: max {detector_observability['max_invalid_run_seconds']:.3f}s, "
        f"cumulative {detector_observability['cumulative_invalid_seconds']:.3f}s",
        f"- Planned reconnect gaps: max {reconnect_observability['max_gap_seconds']:.3f}s, "
        f"cumulative {reconnect_observability['cumulative_gap_seconds']:.3f}s",
        f"- Consistency rereads: {consistency['rereads']}, "
        f"resolved races {consistency['resolved_races']}",
        "",
        "## Findings",
        "",
    ]
    findings = closure["findings"]
    if findings:
        lines.extend(
            f"- {item['severity'].upper()}: `{item['code']}` ({item['count']})"
            for item in findings
        )
    else:
        lines.append("- None")
    lines.extend(["", str(closure["evidence"]), ""])
    return "\n".join(lines)


def _write_private(path: Path, content: str) -> None:
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as stream:
        stream.write(content)


def trace_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(128 * 1024), b""):
            digest.update(chunk)
    return f"sha256:{digest.hexdigest()}"


def write_reports(output_dir: Path, report: Mapping[str, Any]) -> dict[str, Any]:
    closure = build_closure_summary(report)
    _write_private(output_dir / "report.json", json.dumps(report, indent=2, sort_keys=True) + "\n")
    _write_private(output_dir / "report.md", render_report_markdown(report))
    _write_private(
        output_dir / "closure-summary.json",
        json.dumps(closure, indent=2, sort_keys=True) + "\n",
    )
    return closure


def _default_output_dir() -> Path:
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    return Path(__file__).resolve().parents[2] / "artifacts" / "csi" / "alarm-gate" / stamp


def _make_polling_client(client: DeviceClient, timeout_seconds: float) -> DeviceClient:
    polling_client = DeviceClient(
        base_url=client.base_url,
        username=client.username,
        password=client.password,
        timeout=timeout_seconds,
        verify=client.verify,
        retries=0,
    )
    if client.token:
        polling_client.set_token(client.token)
    return polling_client


def run_gate(args: argparse.Namespace) -> int:
    client = DeviceClient.from_args(args)
    preflight = run_preflight(client, args.expected_firmware_sha)
    polling_client = _make_polling_client(client, args.sample_timeout_seconds)
    output_dir = args.output_dir or _default_output_dir()
    output_dir.mkdir(parents=True, exist_ok=False, mode=0o700)
    os.chmod(output_dir, 0o700)
    trace_path = output_dir / "trace.jsonl"
    marker_tail = MarkerTail(args.marker_file)
    records: list[dict[str, Any]] = []
    interrupted = False

    preflight_record = {
        "schema": SCHEMA,
        "record_type": "preflight",
        "host": host_stamp(),
        "expected_firmware_sha": args.expected_firmware_sha,
        "system_info": preflight.raw_system_info,
        "wifi_sensing": preflight.raw_wifi_status,
        "alarm_rules": preflight.raw_alarm_rules,
        "health": preflight.raw_health,
        "mutexes": preflight.raw_mutexes,
    }
    with PrivateJsonlWriter(trace_path) as writer:
        writer.write(preflight_record)
        schedule_origin_ns = time.monotonic_ns()
        sample_index = 0
        first_sample_monotonic_ns: int | None = None
        sequence = 0
        try:
            while True:
                for marker in marker_tail.drain():
                    record = marker_record(marker, sequence)
                    records.append(record)
                    writer.write(record)
                    sequence += 1
                record = capture_sample(
                    polling_client,
                    sequence,
                    preflight.health_endpoint,
                    preflight.mutex_endpoint,
                    consistency_rereads=args.consistency_rereads,
                    consistency_reread_delay_seconds=args.consistency_reread_delay_seconds,
                )
                records.append(record)
                writer.write(record)
                sequence += 1
                sample_index += 1
                sample_monotonic_ns = record["host"]["monotonic_ns"]
                if first_sample_monotonic_ns is None:
                    first_sample_monotonic_ns = sample_monotonic_ns
                observed_seconds = (
                    sample_monotonic_ns - first_sample_monotonic_ns
                ) / 1_000_000_000
                if observed_seconds >= args.duration_seconds:
                    break
                next_deadline_ns = schedule_origin_ns + int(
                    sample_index * args.poll_interval_seconds * 1_000_000_000
                )
                sleep_seconds = (next_deadline_ns - time.monotonic_ns()) / 1_000_000_000
                if sleep_seconds > 0:
                    time.sleep(
                        min(
                            sleep_seconds,
                            args.duration_seconds - observed_seconds,
                        )
                    )
        except KeyboardInterrupt:
            interrupted = True
        for marker in marker_tail.drain():
            record = marker_record(marker, sequence)
            records.append(record)
            writer.write(record)
            sequence += 1

    report = analyze_records(
        records,
        preflight,
        minimum_duration_seconds=args.minimum_duration_seconds,
        minimum_operator_cycles=args.minimum_operator_cycles,
        minimum_verified_reconnect_gaps=args.minimum_verified_reconnect_gaps,
        minimum_motion_during_reconnect=args.minimum_motion_during_reconnect,
        poll_interval_seconds=args.poll_interval_seconds,
        operator_deadline_margin_seconds=args.operator_deadline_margin_seconds,
        max_invalid_run_seconds=args.max_invalid_run_seconds,
        max_cumulative_invalid_seconds=args.max_cumulative_invalid_seconds,
        max_reconnect_gap_seconds=args.max_reconnect_gap_seconds,
        max_cumulative_reconnect_gap_seconds=args.max_cumulative_reconnect_gap_seconds,
        max_sample_gap_seconds=args.max_sample_gap_seconds,
        minimum_sample_coverage_ratio=args.minimum_sample_coverage_ratio,
    )
    report["evidence"] = {"trace_sha256": trace_sha256(trace_path)}
    if interrupted:
        report["findings"].append(
            {
                "code": "operator_interrupted",
                "severity": "error",
                "count": 1,
                "first_sequence": -1,
                "last_sequence": -1,
            }
        )
        report["findings"] = sorted(report["findings"], key=lambda item: item["code"])
        report["verdict"] = "fail"
    closure = write_reports(output_dir, report)
    if args.json:
        print(json.dumps(closure, indent=2, sort_keys=True))
    else:
        print(render_report_markdown(report), end="")
        print(f"Evidence directory: {output_dir}")
    return 0 if report["verdict"] == "pass" else 2


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    run_parser = subparsers.add_parser("run", help="Run the observation gate.")
    add_common_device_args(run_parser)
    run_parser.add_argument("--expected-firmware-sha", required=True)
    run_parser.add_argument("--duration-seconds", type=float, default=3600.0)
    run_parser.add_argument("--minimum-duration-seconds", type=float, default=3600.0)
    run_parser.add_argument("--poll-interval-seconds", type=float, default=1.0)
    run_parser.add_argument("--sample-timeout-seconds", type=float, default=1.0)
    run_parser.add_argument("--minimum-operator-cycles", type=int, default=1)
    run_parser.add_argument("--minimum-verified-reconnect-gaps", type=int, default=1)
    run_parser.add_argument("--minimum-motion-during-reconnect", type=int, default=1)
    run_parser.add_argument("--operator-deadline-margin-seconds", type=float, default=1.0)
    run_parser.add_argument("--max-invalid-run-seconds", type=float, default=10.0)
    run_parser.add_argument("--max-cumulative-invalid-seconds", type=float, default=30.0)
    run_parser.add_argument("--max-reconnect-gap-seconds", type=float, default=60.0)
    run_parser.add_argument(
        "--max-cumulative-reconnect-gap-seconds",
        type=float,
        default=120.0,
    )
    run_parser.add_argument("--max-sample-gap-seconds", type=float, default=90.0)
    run_parser.add_argument("--minimum-sample-coverage-ratio", type=float, default=0.8)
    run_parser.add_argument("--consistency-rereads", type=int, default=2)
    run_parser.add_argument("--consistency-reread-delay-seconds", type=float, default=0.1)
    run_parser.add_argument("--output-dir", type=Path)
    run_parser.add_argument("--marker-file", type=Path)

    mark_parser = subparsers.add_parser("mark", help="Append an operator marker without stdin.")
    mark_parser.add_argument("--marker-file", type=Path, required=True)
    mark_parser.add_argument("--kind", choices=MARKER_KINDS, required=True)
    mark_parser.add_argument("--label")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        if args.command == "mark":
            append_marker(args.marker_file, args.kind, args.label)
            return 0
        finite_values = (
            args.duration_seconds,
            args.poll_interval_seconds,
            args.sample_timeout_seconds,
            args.minimum_duration_seconds,
            args.operator_deadline_margin_seconds,
            args.max_invalid_run_seconds,
            args.max_cumulative_invalid_seconds,
            args.max_reconnect_gap_seconds,
            args.max_cumulative_reconnect_gap_seconds,
            args.max_sample_gap_seconds,
            args.minimum_sample_coverage_ratio,
            args.consistency_reread_delay_seconds,
        )
        if any(not math.isfinite(value) for value in finite_values):
            raise GateError("all numeric gate arguments must be finite")
        if (
            args.duration_seconds <= 0
            or args.poll_interval_seconds <= 0
            or args.sample_timeout_seconds <= 0
            or args.minimum_duration_seconds < 0
            or args.operator_deadline_margin_seconds < 0
            or args.max_invalid_run_seconds < 0
            or args.max_cumulative_invalid_seconds < 0
            or args.max_reconnect_gap_seconds < 0
            or args.max_cumulative_reconnect_gap_seconds < 0
            or args.max_sample_gap_seconds <= 0
            or not 0 <= args.minimum_sample_coverage_ratio <= 1
            or args.consistency_reread_delay_seconds < 0
        ):
            raise GateError(
                "duration/poll interval must be positive and minimum duration non-negative"
            )
        if args.duration_seconds < args.minimum_duration_seconds:
            raise GateError("duration must be at least the minimum required duration")
        if (
            args.minimum_operator_cycles < 0
            or args.minimum_verified_reconnect_gaps < 0
            or args.minimum_motion_during_reconnect < 0
            or not 0 <= args.consistency_rereads <= 10
        ):
            raise GateError("minimum counts/rereads are outside the accepted range")
        if (
            (
                args.minimum_operator_cycles > 0
                or args.minimum_verified_reconnect_gaps > 0
                or args.minimum_motion_during_reconnect > 0
            )
            and args.marker_file is None
        ):
            raise GateError(
                "--marker-file is required when operator cycles or reconnect gaps are required"
            )
        return run_gate(args)
    except (GateError, FileExistsError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
