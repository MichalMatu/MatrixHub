#!/usr/bin/env python3
"""Collect a commercial-grade runtime memory budget report from the device.

The report combines diagnostic snapshots, optional feature enablement passes,
endpoint stress, and a short built-in soak. It avoids fake external secrets:
notification transports are observed but not configured, and heartbeat is only
enabled when an existing slot already has a URL.
"""

from __future__ import annotations

import argparse
import copy
import csv
import datetime as dt
import json
import socket
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable
from urllib.parse import urlsplit

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args  # noqa: E402


STRESS_ENDPOINTS = (
    ("GET", "/api/system/info"),
    ("GET", "/api/system/tasks?details=1"),
    ("GET", "/api/diagnostics/summary"),
    ("GET", "/api/diagnostics/heap"),
    ("GET", "/api/diagnostics/tasks?details=1"),
    ("GET", "/api/diagnostics/mutexes"),
    ("GET", "/api/diagnostics/features"),
    ("GET", "/api/config"),
    ("GET", "/api/ble/status"),
    ("GET", "/api/ble/settings"),
    ("GET", "/api/wifisensing/config"),
    ("GET", "/api/keyboard/config"),
    ("GET", "/api/airmouse/status"),
    ("GET", "/api/airmouse/config"),
    ("GET", "/api/macros/status"),
    ("GET", "/api/macros/settings"),
    ("GET", "/api/usbterminal/config"),
    ("GET", "/api/udp"),
    ("GET", "/api/heartbeat"),
    ("GET", "/api/notifications/settings"),
)


CSV_FIELDS = (
    "label",
    "timestamp_iso",
    "uptime_sec",
    "internal_free",
    "internal_minimum_free",
    "internal_largest_block",
    "internal_fragmentation_percent",
    "psram_free",
    "psram_minimum_free",
    "psram_largest_block",
    "psram_fragmentation_percent",
    "default_free",
    "default_minimum_free",
    "default_largest_block",
    "task_count",
    "worst_stack_task",
    "worst_stack_hwm",
    "ws_queue_drops",
    "ws_heap_fallbacks",
    "lock_standard_timeouts",
    "lock_recursive_timeouts",
    "lock_standard_slow",
    "lock_recursive_slow",
    "boot_count",
    "unexpected_restarts",
    "current_reset_reason",
)


Mutation = Callable[[dict[str, Any], argparse.Namespace], bool]


@dataclass(frozen=True)
class FeatureToggle:
    name: str
    endpoint: str
    mutate: Mutation
    restart_recommended: bool = False
    note: str = ""


def utc_now() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")


def nested(data: dict[str, Any] | None, path: tuple[str, ...], default: Any = None) -> Any:
    current: Any = data or {}
    for key in path:
        if not isinstance(current, dict):
            return default
        current = current.get(key)
    return current if current is not None else default


def region_from(
    tasks: dict[str, Any],
    heap: dict[str, Any],
    name: str,
) -> dict[str, Any]:
    task_region = nested(tasks, ("memory", name))
    if isinstance(task_region, dict):
        return task_region
    heap_region = nested(heap, ("regions", name))
    return heap_region if isinstance(heap_region, dict) else {}


def worst_stack(tasks: dict[str, Any]) -> tuple[str, int | None]:
    stack = tasks.get("stack")
    if isinstance(stack, dict) and stack.get("worstHighWaterMark") is not None:
        return str(stack.get("worstTask") or ""), int(stack["worstHighWaterMark"])

    worst_name = ""
    worst_hwm: int | None = None
    for task in tasks.get("tasks") or []:
        hwm = task.get("stackHighWaterMark")
        if hwm is None:
            continue
        hwm = int(hwm)
        if worst_hwm is None or hwm < worst_hwm:
            worst_hwm = hwm
            worst_name = str(task.get("name") or "")
    return worst_name, worst_hwm


def collect_snapshot(client: DeviceClient, label: str) -> dict[str, Any]:
    info = client.json("GET", "/api/system/info")
    tasks = client.json("GET", "/api/system/tasks?details=1")
    summary = client.json("GET", "/api/diagnostics/summary")
    heap = client.json("GET", "/api/diagnostics/heap")
    mutexes = client.json("GET", "/api/diagnostics/mutexes")
    features = client.json("GET", "/api/diagnostics/features")

    internal = region_from(tasks, heap, "internal")
    psram = region_from(tasks, heap, "psram")
    default = region_from(tasks, heap, "default")
    worst_name, worst_hwm = worst_stack(tasks)

    snapshot = {
        "label": label,
        "timestamp_iso": utc_now(),
        "uptime_sec": info.get("uptime") or summary.get("uptimeSec"),
        "internal_free": internal.get("free") or info.get("free_heap"),
        "internal_minimum_free": internal.get("minimumFree") or info.get("min_free_heap"),
        "internal_largest_block": internal.get("largestBlock") or info.get("max_alloc_heap"),
        "internal_fragmentation_percent": internal.get("fragmentationPercent"),
        "psram_free": psram.get("free") or info.get("free_psram"),
        "psram_minimum_free": psram.get("minimumFree"),
        "psram_largest_block": psram.get("largestBlock"),
        "psram_fragmentation_percent": psram.get("fragmentationPercent"),
        "default_free": default.get("free"),
        "default_minimum_free": default.get("minimumFree"),
        "default_largest_block": default.get("largestBlock"),
        "task_count": tasks.get("taskCount"),
        "worst_stack_task": worst_name,
        "worst_stack_hwm": worst_hwm,
        "ws_queue_drops": nested(summary, ("http", "wsQueueDrops")),
        "ws_heap_fallbacks": nested(summary, ("http", "wsHeapFallbacks")),
        "lock_standard_timeouts": nested(mutexes, ("runtime", "standard", "timeouts")),
        "lock_recursive_timeouts": nested(mutexes, ("runtime", "recursive", "timeouts")),
        "lock_standard_slow": nested(mutexes, ("runtime", "standard", "slowAcquires")),
        "lock_recursive_slow": nested(mutexes, ("runtime", "recursive", "slowAcquires")),
        "boot_count": nested(summary, ("boot", "bootCount")),
        "unexpected_restarts": nested(summary, ("boot", "unexpectedRestarts")),
        "current_reset_reason": nested(summary, ("boot", "currentResetReason")),
        "_raw": {
            "info": info,
            "tasks": tasks,
            "summary": summary,
            "heap": heap,
            "mutexes": mutexes,
            "features": features,
        },
    }
    print(
        "[SNAPSHOT] "
        f"{label} uptime={snapshot['uptime_sec']} "
        f"int_free={snapshot['internal_free']} "
        f"int_largest={snapshot['internal_largest_block']} "
        f"psram_free={snapshot['psram_free']} "
        f"stack={snapshot['worst_stack_hwm']}:{snapshot['worst_stack_task']} "
        f"wsDrops={snapshot['ws_queue_drops']} "
        f"lockTimeouts={snapshot['lock_standard_timeouts']}/{snapshot['lock_recursive_timeouts']}",
        flush=True,
    )
    return snapshot


def set_key(payload: dict[str, Any], key: str, value: Any) -> bool:
    changed = payload.get(key) != value
    payload[key] = value
    return changed


def enable_bool(key: str = "enabled") -> Mutation:
    def mutate(payload: dict[str, Any], _args: argparse.Namespace) -> bool:
        return set_key(payload, key, True)

    return mutate


def enable_airmouse(payload: dict[str, Any], _args: argparse.Namespace) -> bool:
    changed = False
    changed |= set_key(payload, "movement_enabled", True)
    changed |= set_key(payload, "click_enabled", True)
    return changed


def enable_jiggler(payload: dict[str, Any], _args: argparse.Namespace) -> bool:
    changed = enable_airmouse(payload, _args)
    jiggler = payload.get("jiggler")
    if not isinstance(jiggler, dict):
        jiggler = {}
        payload["jiggler"] = jiggler
        changed = True
    changed |= set_key(jiggler, "mode", 1)
    changed |= set_key(jiggler, "interval", max(1000, int(getattr(_args, "jiggler_interval_ms", 30000))))
    changed |= set_key(jiggler, "move_distance", max(1, int(getattr(_args, "jiggler_move_distance", 1))))
    changed |= set_key(jiggler, "random_interval", False)
    return changed


def enable_udp(payload: dict[str, Any], args: argparse.Namespace) -> bool:
    changed = set_key(payload, "enabled", True)
    host = payload.get("host") or args.udp_target_host
    port = int(payload.get("port") or args.udp_target_port)
    changed |= set_key(payload, "host", host)
    changed |= set_key(payload, "port", port)
    changed |= set_key(payload, "format", payload.get("format") or "json")
    changed |= set_key(payload, "interval_ms", max(1000, int(payload.get("interval_ms") or 60000)))
    return changed


def enable_heartbeat_if_configured(payload: dict[str, Any], _args: argparse.Namespace) -> bool:
    slots = payload.get("slots")
    if not isinstance(slots, list):
        return False
    changed = False
    configured = False
    for slot in slots:
        if not isinstance(slot, dict):
            continue
        if str(slot.get("url") or "").strip():
            configured = True
            changed |= set_key(slot, "enabled", True)
    return changed if configured else False


FEATURES: tuple[FeatureToggle, ...] = (
    FeatureToggle("ble", "/api/ble/settings", enable_bool()),
    FeatureToggle("wifi_sensing", "/api/wifisensing/config", enable_bool(), restart_recommended=True),
    FeatureToggle("keyboard", "/api/keyboard/config", enable_bool(), restart_recommended=True),
    FeatureToggle("airmouse", "/api/airmouse/config", enable_airmouse, restart_recommended=True),
    FeatureToggle("jiggler", "/api/airmouse/config", enable_jiggler, restart_recommended=True),
    FeatureToggle("macros", "/api/macros/settings", enable_bool(), restart_recommended=True),
    FeatureToggle("usb_terminal", "/api/usbterminal/config", enable_bool(), restart_recommended=True),
    FeatureToggle("udp", "/api/udp", enable_udp),
    FeatureToggle(
        "heartbeat",
        "/api/heartbeat",
        enable_heartbeat_if_configured,
        note="skips when no heartbeat URL is configured",
    ),
)


def run_stress(client: DeviceClient, cycles: int, delay: float) -> dict[str, Any]:
    results: list[dict[str, Any]] = []
    failures = 0
    for _ in range(max(0, cycles)):
        for method, path in STRESS_ENDPOINTS:
            start = time.perf_counter()
            try:
                response = client.request(method, path)
                ok = response.status_code == 200
                result = {
                    "method": method,
                    "path": path,
                    "status": response.status_code,
                    "ok": ok,
                    "latency_ms": round((time.perf_counter() - start) * 1000.0, 2),
                    "bytes": len(response.content),
                }
            except Exception as exc:  # noqa: BLE001 - report every transport failure.
                ok = False
                result = {
                    "method": method,
                    "path": path,
                    "status": None,
                    "ok": False,
                    "latency_ms": 0,
                    "bytes": 0,
                    "error": str(exc),
                }
            failures += 0 if ok else 1
            results.append(result)
            if delay > 0:
                time.sleep(delay)

    successes = len(results) - failures
    avg_latency = (
        sum(item["latency_ms"] for item in results if item["ok"]) / successes
        if successes
        else 0.0
    )
    return {
        "requests": len(results),
        "success": successes,
        "failed": failures,
        "avg_latency_ms": round(avg_latency, 2),
        "results": results,
    }


def wait_for_device_after_restart(
    client: DeviceClient,
    before_boot_count: int | None,
    before_uptime: int | None,
    timeout_sec: float,
    grace_sec: float,
) -> dict[str, Any]:
    time.sleep(max(0.0, grace_sec))
    deadline = time.monotonic() + max(1.0, timeout_sec)
    last_error = ""
    while time.monotonic() < deadline:
        try:
            client.login(force=True)
            summary = client.json("GET", "/api/diagnostics/summary")
            boot_count = nested(summary, ("boot", "bootCount"))
            uptime = summary.get("uptimeSec")
            restart_observed = False
            if isinstance(before_boot_count, int) and isinstance(boot_count, int):
                restart_observed = boot_count != before_boot_count
            if (
                not restart_observed
                and isinstance(before_uptime, int)
                and isinstance(uptime, int)
            ):
                restart_observed = uptime < before_uptime
            return {
                "ok": True,
                "restart_observed": restart_observed,
                "boot_count": boot_count,
                "uptime_sec": uptime,
            }
        except Exception as exc:  # noqa: BLE001 - device may be rebooting.
            last_error = str(exc)
            time.sleep(2.0)
    return {"ok": False, "error": last_error or "timeout"}


def apply_feature_pass(
    client: DeviceClient,
    args: argparse.Namespace,
    snapshots: list[dict[str, Any]],
) -> list[dict[str, Any]]:
    events: list[dict[str, Any]] = []
    originals: dict[str, dict[str, Any]] = {}

    selected = {name.strip() for name in args.features.split(",") if name.strip()}
    if "all" in selected:
        selected = {feature.name for feature in FEATURES}

    for feature in FEATURES:
        if feature.name not in selected:
            continue
        event = {
            "feature": feature.name,
            "endpoint": feature.endpoint,
            "restart_recommended": feature.restart_recommended,
            "note": feature.note,
            "status": "pending",
        }
        try:
            original = client.json("GET", feature.endpoint)
            if feature.endpoint not in originals:
                originals[feature.endpoint] = copy.deepcopy(original)
            payload = copy.deepcopy(original)
            changed = feature.mutate(payload, args)
            if not changed:
                event["status"] = "already_enabled_or_not_configured"
            else:
                before_summary = nested(snapshots[-1], ("_raw", "summary"), {})
                before_boot_count = nested(before_summary, ("boot", "bootCount"))
                before_uptime = before_summary.get("uptimeSec") if isinstance(before_summary, dict) else None
                client.json("POST", feature.endpoint, json=payload)
                event["status"] = "enabled"
                print(f"[FEATURE] {feature.name}: enabled", flush=True)
                if feature.restart_recommended and args.wait_restarts:
                    wait_result = wait_for_device_after_restart(
                        client,
                        before_boot_count if isinstance(before_boot_count, int) else None,
                        before_uptime if isinstance(before_uptime, int) else None,
                        args.restart_timeout_sec,
                        args.restart_grace_sec,
                    )
                    event["restart"] = wait_result
                    if not wait_result.get("ok"):
                        event["status"] = "failed_waiting_for_restart"
                        events.append(event)
                        continue
            time.sleep(max(0.0, args.feature_settle_sec))
            snapshots.append(collect_snapshot(client, f"after_{feature.name}"))
        except Exception as exc:  # noqa: BLE001 - report and continue to next feature.
            event["status"] = "failed"
            event["error"] = str(exc)
            print(f"[FEATURE] {feature.name}: failed: {exc}", flush=True)
        events.append(event)

    if args.restore_at_end:
        for endpoint, original in originals.items():
            try:
                client.json("POST", endpoint, json=original)
                events.append({"feature": "restore", "endpoint": endpoint, "status": "restored"})
            except Exception as exc:  # noqa: BLE001
                events.append(
                    {
                        "feature": "restore",
                        "endpoint": endpoint,
                        "status": "failed",
                        "error": str(exc),
                    }
                )
        time.sleep(max(0.0, args.feature_settle_sec))
        snapshots.append(collect_snapshot(client, "after_restore"))

    return events


def run_inline_soak(
    client: DeviceClient,
    duration_sec: float,
    interval_sec: float,
    snapshots: list[dict[str, Any]],
) -> None:
    if duration_sec <= 0:
        return
    deadline = time.monotonic() + duration_sec
    index = 0
    while True:
        snapshots.append(collect_snapshot(client, f"soak_{index:03d}"))
        index += 1
        if time.monotonic() >= deadline:
            break
        time.sleep(max(1.0, interval_sec))


def int_values(snapshots: list[dict[str, Any]], key: str) -> list[int]:
    values: list[int] = []
    for snapshot in snapshots:
        value = snapshot.get(key)
        if isinstance(value, bool):
            continue
        if isinstance(value, (int, float)):
            values.append(int(value))
    return values


def add_growth_failure(
    failures: list[str],
    snapshots: list[dict[str, Any]],
    key: str,
    label: str,
) -> None:
    values = int_values(snapshots, key)
    if len(values) >= 2 and values[-1] > values[0]:
        failures.append(f"{label} grew from {values[0]} to {values[-1]}")


def stability_window(snapshots: list[dict[str, Any]]) -> list[dict[str, Any]]:
    window = [
        snapshot
        for snapshot in snapshots
        if str(snapshot.get("label", "")).startswith("soak_")
        or snapshot.get("label") in {"after_stress", "after_soak"}
    ]
    return window if len(window) >= 2 else snapshots


def evaluate(snapshots: list[dict[str, Any]], args: argparse.Namespace) -> tuple[list[str], list[str]]:
    failures: list[str] = []
    warnings: list[str] = []
    if not snapshots:
        return ["no snapshots collected"], warnings

    stable = stability_window(snapshots)

    min_free = int_values(stable, "internal_minimum_free")
    if len(min_free) >= 2 and min_free[0] > 0:
        drift = (min_free[0] - min_free[-1]) / min_free[0]
        if drift > args.max_min_free_drift_percent / 100.0:
            failures.append(
                "internal minimum free heap drift "
                f"{drift:.1%} exceeds {args.max_min_free_drift_percent:.1f}%"
            )

    boot_counts = int_values(stable, "boot_count")
    if len(boot_counts) >= 2 and boot_counts[-1] != boot_counts[0]:
        failures.append(f"boot_count changed from {boot_counts[0]} to {boot_counts[-1]}")

    restarts = int_values(stable, "unexpected_restarts")
    if len(restarts) >= 2 and restarts[-1] != restarts[0]:
        failures.append(
            f"unexpected_restarts changed from {restarts[0]} to {restarts[-1]}"
        )

    all_restarts = int_values(snapshots, "unexpected_restarts")
    if (
        len(all_restarts) >= 2
        and all_restarts[-1] > all_restarts[0]
        and (not restarts or restarts[-1] == restarts[0])
    ):
        warnings.append(
            "BootTracker unexpectedRestarts increased during feature activation "
            f"({all_restarts[0]} -> {all_restarts[-1]}). Feature restarts are expected "
            "when their contract requires them, but a missing planned-restart marker "
            "or extra reboot still needs follow-up. Counter stayed stable during soak"
        )

    add_growth_failure(failures, stable, "ws_queue_drops", "ws_queue_drops")
    add_growth_failure(failures, stable, "lock_standard_timeouts", "standard lock timeouts")
    add_growth_failure(failures, stable, "lock_recursive_timeouts", "recursive lock timeouts")

    largest = int_values(snapshots, "internal_largest_block")
    if largest and min(largest) < args.warn_internal_largest_block:
        warnings.append(
            "internal largest block dropped below warning threshold "
            f"({min(largest)} < {args.warn_internal_largest_block})"
        )

    stack = int_values(snapshots, "worst_stack_hwm")
    if stack and args.min_stack_hwm > 0 and min(stack) < args.min_stack_hwm:
        failures.append(
            f"worst stack HWM {min(stack)} below required {args.min_stack_hwm}"
        )
    elif stack and min(stack) < args.warn_stack_hwm:
        warnings.append(
            f"worst stack HWM {min(stack)} below warning threshold {args.warn_stack_hwm}"
        )

    return failures, warnings


def write_csv(path: Path, snapshots: list[dict[str, Any]]) -> None:
    with path.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=CSV_FIELDS)
        writer.writeheader()
        for snapshot in snapshots:
            writer.writerow({key: snapshot.get(key) for key in CSV_FIELDS})


def markdown_table(snapshots: list[dict[str, Any]]) -> list[str]:
    lines = [
        "| label | uptime | int_free | int_largest | int_frag | psram_free | psram_largest | worst_stack | ws_drops | lock_timeouts |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | ---: | --- |",
    ]
    for snapshot in snapshots:
        lock_timeouts = (
            f"{snapshot.get('lock_standard_timeouts')}/"
            f"{snapshot.get('lock_recursive_timeouts')}"
        )
        worst_stack = (
            f"{snapshot.get('worst_stack_hwm')} "
            f"{snapshot.get('worst_stack_task') or ''}".strip()
        )
        lines.append(
            "| {label} | {uptime} | {internal_free} | {internal_largest_block} | "
            "{internal_fragmentation_percent} | {psram_free} | {psram_largest_block} | "
            "{worst_stack} | {ws_queue_drops} | {lock_timeouts} |".format(
                uptime=snapshot.get("uptime_sec"),
                worst_stack=worst_stack,
                lock_timeouts=lock_timeouts,
                **snapshot,
            )
        )
    return lines


def write_report(
    path: Path,
    args: argparse.Namespace,
    snapshots: list[dict[str, Any]],
    events: list[dict[str, Any]],
    stress: dict[str, Any] | None,
    failures: list[str],
    warnings: list[str],
) -> None:
    lines: list[str] = [
        "# Runtime Memory Budget Report",
        "",
        f"- generated_utc: {utc_now()}",
        f"- target: {args.device_url}",
        f"- feature_pass: {not args.skip_feature_pass}",
        f"- restore_at_end: {args.restore_at_end}",
        f"- stress_cycles: {args.stress_cycles}",
        f"- soak_duration_sec: {args.soak_duration_sec}",
        "",
        "## Verdict",
        "",
        "FAIL" if failures else "PASS",
        "",
    ]
    if failures:
        lines.extend(["### Failures", ""])
        lines.extend(f"- {failure}" for failure in failures)
        lines.append("")
    if warnings:
        lines.extend(["### Warnings", ""])
        lines.extend(f"- {warning}" for warning in warnings)
        lines.append("")

    if stress is not None:
        lines.extend(
            [
                "## Stress",
                "",
                f"- requests: {stress['requests']}",
                f"- success: {stress['success']}",
                f"- failed: {stress['failed']}",
                f"- avg_latency_ms: {stress['avg_latency_ms']}",
                "",
            ]
        )

    lines.extend(["## Feature Events", ""])
    if events:
        for event in events:
            note = f" ({event['note']})" if event.get("note") else ""
            restart = " restart_recommended" if event.get("restart_recommended") else ""
            error = f": {event['error']}" if event.get("error") else ""
            lines.append(
                f"- {event['feature']} {event['status']}{restart}{note}{error}"
            )
    else:
        lines.append("- none")
    lines.append("")

    lines.extend(["## Snapshots", ""])
    lines.extend(markdown_table(snapshots))
    lines.append("")

    path.write_text("\n".join(lines), encoding="utf-8")


def default_udp_host(device_url: str) -> str:
    host = urlsplit(device_url).hostname
    if host:
        return host
    try:
        return socket.gethostbyname(socket.gethostname())
    except OSError:
        return "192.168.0.30"


def parse_duration(text: str) -> float:
    text = text.strip().lower()
    if not text:
        raise argparse.ArgumentTypeError("empty duration")
    unit = text[-1]
    if unit in {"s", "m", "h"}:
        value = float(text[:-1])
        return value * {"s": 1, "m": 60, "h": 3600}[unit]
    return float(text)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    parser.add_argument("--output-dir", default=None)
    parser.add_argument(
        "--render-from-raw",
        default=None,
        help="Regenerate Markdown/CSV from a previous memory_budget_raw.json without contacting the device.",
    )
    parser.add_argument("--features", default="all")
    parser.add_argument("--skip-feature-pass", action="store_true")
    parser.add_argument("--restore-at-end", action="store_true")
    parser.add_argument("--feature-settle-sec", type=float, default=2.0)
    parser.add_argument("--wait-restarts", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument("--restart-timeout-sec", type=float, default=120.0)
    parser.add_argument("--restart-grace-sec", type=float, default=4.0)
    parser.add_argument("--stress-cycles", type=int, default=10)
    parser.add_argument("--stress-delay", type=float, default=0.02)
    parser.add_argument("--soak-duration-sec", type=parse_duration, default=0.0)
    parser.add_argument("--soak-interval-sec", type=parse_duration, default=10.0)
    parser.add_argument("--max-min-free-drift-percent", type=float, default=10.0)
    parser.add_argument("--warn-internal-largest-block", type=int, default=32768)
    parser.add_argument("--warn-stack-hwm", type=int, default=256)
    parser.add_argument("--min-stack-hwm", type=int, default=0)
    parser.add_argument("--udp-target-host", default=None)
    parser.add_argument("--udp-target-port", type=int, default=9)
    parser.add_argument("--jiggler-interval-ms", type=int, default=30000)
    parser.add_argument("--jiggler-move-distance", type=int, default=1)
    args = parser.parse_args(argv)

    if args.render_from_raw:
        raw_path = Path(args.render_from_raw)
        raw = json.loads(raw_path.read_text(encoding="utf-8"))
        output_dir = Path(args.output_dir) if args.output_dir else raw_path.parent
        output_dir.mkdir(parents=True, exist_ok=True)
        snapshots = raw.get("snapshots") or []
        events = raw.get("events") or []
        stress = raw.get("stress")
        failures, warnings = evaluate(snapshots, args)
        raw["failures"] = failures
        raw["warnings"] = warnings
        raw["rendered_utc"] = utc_now()
        write_csv(output_dir / "memory_budget_snapshots.csv", snapshots)
        (output_dir / "memory_budget_raw.json").write_text(
            json.dumps(raw, indent=2, sort_keys=True),
            encoding="utf-8",
        )
        write_report(
            output_dir / "memory_budget_report.md",
            args,
            snapshots,
            events,
            stress,
            failures,
            warnings,
        )
        print(f"report: {output_dir / 'memory_budget_report.md'}")
        print(f"csv   : {output_dir / 'memory_budget_snapshots.csv'}")
        print("PASS" if not failures else "FAIL")
        return 0 if not failures else 1

    if args.udp_target_host is None:
        args.udp_target_host = default_udp_host(args.device_url)

    stamp = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output_dir = Path(args.output_dir) if args.output_dir else Path("artifacts/diagnostics") / stamp / "memory"
    output_dir.mkdir(parents=True, exist_ok=True)

    client = DeviceClient.from_args(args)
    client.login()

    snapshots: list[dict[str, Any]] = []
    events: list[dict[str, Any]] = []
    stress: dict[str, Any] | None = None

    snapshots.append(collect_snapshot(client, "baseline"))
    if not args.skip_feature_pass:
        events = apply_feature_pass(client, args, snapshots)

    if args.stress_cycles > 0:
        print(f"[STRESS] cycles={args.stress_cycles}", flush=True)
        stress = run_stress(client, args.stress_cycles, max(0.0, args.stress_delay))
        snapshots.append(collect_snapshot(client, "after_stress"))

    run_inline_soak(client, args.soak_duration_sec, args.soak_interval_sec, snapshots)
    if args.soak_duration_sec > 0:
        snapshots.append(collect_snapshot(client, "after_soak"))

    failures, warnings = evaluate(snapshots, args)

    raw = {
        "generated_utc": utc_now(),
        "target": client.base_url,
        "events": events,
        "stress": stress,
        "failures": failures,
        "warnings": warnings,
        "snapshots": snapshots,
    }

    write_csv(output_dir / "memory_budget_snapshots.csv", snapshots)
    (output_dir / "memory_budget_raw.json").write_text(
        json.dumps(raw, indent=2, sort_keys=True),
        encoding="utf-8",
    )
    write_report(
        output_dir / "memory_budget_report.md",
        args,
        snapshots,
        events,
        stress,
        failures,
        warnings,
    )

    print(f"report: {output_dir / 'memory_budget_report.md'}")
    print(f"csv   : {output_dir / 'memory_budget_snapshots.csv'}")
    print("PASS" if not failures else "FAIL")
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
