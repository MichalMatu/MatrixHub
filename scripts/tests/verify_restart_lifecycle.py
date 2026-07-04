#!/usr/bin/env python3
"""Verify controlled restart lifecycle over HTTPS/JWT.

The smoke test requests /rest/restart repeatedly, waits for the device to
return, then validates boot markers, watchdog state, task-count stability,
runtime diagnostics, live-tail logs, and an optional /ws/system reconnect.
"""

from __future__ import annotations

import argparse
import base64
import os
import json
import socket
import ssl
import sys
import time
from pathlib import Path
from typing import Any
from urllib.parse import urlsplit, urlunsplit

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args  # noqa: E402


RESTART_COMMAND = 2
ESP_RST_SW = 3

RESET_REASONS = {
    0: "unknown",
    1: "power_on",
    2: "external",
    3: "software",
    4: "panic",
    5: "interrupt_wdt",
    6: "task_wdt",
    7: "wdt",
    8: "deep_sleep",
    9: "brownout",
    10: "sdio",
    11: "usb",
    12: "jtag",
    13: "efuse",
    14: "power_glitch",
    15: "cpu_lockup",
}

SHUTDOWN_REASONS = {
    0: "unknown",
    1: "clean_sleep",
    2: "restart_command",
    3: "ota_update",
    4: "factory_reset",
    5: "watchdog_restart",
    6: "low_memory",
    7: "hygiene_sleep",
}

BAD_LOG_PATTERNS = (
    "guru meditation",
    "panic",
    "abort() was called",
    "assert failed",
    "task watchdog got triggered",
    "watchdog timeout",
    "wdt reset",
    "brownout",
    "unexpected restart detected",
)


def nested(data: dict[str, Any], path: tuple[str, ...], default: Any = None) -> Any:
    current: Any = data
    for key in path:
        if not isinstance(current, dict):
            return default
        current = current.get(key)
    return current if current is not None else default


def login_with_retry(client: DeviceClient, max_wait_s: float = 75.0) -> str:
    deadline = time.monotonic() + max_wait_s
    while True:
        response = client.session.post(
            f"{client.base_url}/rest/signIn",
            json={"username": client.username, "password": client.password},
            timeout=client.timeout,
        )
        if response.status_code == 429 and time.monotonic() < deadline:
            print("[AUTH] login rate limited; waiting 5s", flush=True)
            time.sleep(5.0)
            continue
        if response.status_code != 200:
            raise DeviceClientError(
                f"signIn failed: HTTP {response.status_code} {response.text[:200]}"
            )
        try:
            data = response.json()
        except ValueError as exc:
            raise DeviceClientError("signIn returned non-JSON response") from exc
        token = data.get("access_token")
        if not isinstance(token, str) or not token.strip():
            raise DeviceClientError("signIn succeeded but no access_token was returned")
        client.set_token(token)
        return token


def collect_snapshot(client: DeviceClient, tail_lines: int) -> dict[str, Any]:
    summary = client.json("GET", "/api/diagnostics/summary")
    tasks = client.json("GET", "/api/system/tasks?details=1")
    tail = client.json("GET", f"/rest/logs/tail?lines={max(1, min(tail_lines, 128))}")
    return {"summary": summary, "tasks": tasks, "tail": tail}


def summarize_snapshot(snapshot: dict[str, Any]) -> dict[str, Any]:
    summary = snapshot["summary"]
    tasks = snapshot["tasks"]
    boot = summary.get("boot") or {}
    stack = tasks.get("stack") or {}
    return {
        "boot_count": boot.get("bootCount"),
        "unexpected_restarts": boot.get("unexpectedRestarts"),
        "last_boot_unexpected": boot.get("lastBootUnexpected"),
        "last_shutdown_reason": boot.get("lastShutdownReason"),
        "last_shutdown_reason_name": SHUTDOWN_REASONS.get(boot.get("lastShutdownReason"), "unknown"),
        "current_reset_reason": boot.get("currentResetReason"),
        "current_reset_reason_name": RESET_REASONS.get(boot.get("currentResetReason"), "unknown"),
        "uptime_sec": summary.get("uptimeSec"),
        "task_count": tasks.get("taskCount"),
        "watchdog_initialized": nested(summary, ("watchdog", "initialized")),
        "watchdog_timeout_sec": nested(summary, ("watchdog", "timeoutSec")),
        "worst_task": stack.get("worstTask"),
        "worst_stack_hwm": stack.get("worstHighWaterMark"),
    }


def scan_tail(snapshot: dict[str, Any]) -> list[str]:
    matches: list[str] = []
    lines = nested(snapshot, ("tail", "lines"), []) or []
    for line in lines:
        message = f"{line.get('g', '')}: {line.get('m', '')}"
        lower = message.lower()
        if any(pattern in lower for pattern in BAD_LOG_PATTERNS):
            matches.append(message)
    return matches


def probe_ws_system(client: DeviceClient, timeout: float, require: bool) -> tuple[bool, str]:
    _ = require
    try:
        uri = client.ws_url("/ws/system")
        parts = urlsplit(uri)
        host = parts.hostname
        if not host:
            return False, f"invalid websocket URL: {uri}"
        port = parts.port or (443 if parts.scheme == "wss" else 80)
        path = urlunsplit(("", "", parts.path or "/", parts.query, ""))
        key = base64.b64encode(os.urandom(16)).decode("ascii")
        headers = [
            f"GET {path} HTTP/1.1",
            f"Host: {parts.netloc}",
            "Upgrade: websocket",
            "Connection: Upgrade",
            "Sec-WebSocket-Version: 13",
            f"Sec-WebSocket-Key: {key}",
            f"Cookie: {client.ws_cookie_header()['Cookie']}",
            "\r\n",
        ]

        raw = socket.create_connection((host, port), timeout=timeout)
        try:
            sock: socket.socket | ssl.SSLSocket = raw
            if parts.scheme == "wss":
                context = client.ws_ssl_context()
                if context is None:
                    context = ssl.create_default_context()
                sock = context.wrap_socket(raw, server_hostname=host)
            sock.settimeout(timeout)
            sock.sendall(("\r\n".join(headers)).encode("ascii"))

            response = b""
            while b"\r\n\r\n" not in response and len(response) < 4096:
                chunk = sock.recv(512)
                if not chunk:
                    break
                response += chunk

            header = response.decode("iso-8859-1", errors="replace")
            status_line = header.splitlines()[0] if header else ""
            if " 101 " not in status_line:
                return False, f"handshake failed: {status_line or 'no response'}"
            if "upgrade: websocket" not in header.lower():
                return False, "handshake missing websocket upgrade header"
            try:
                sock.sendall(b"\x88\x00")
            except OSError:
                pass
            return True, "ok"
        finally:
            raw.close()
    except Exception as exc:  # noqa: BLE001 - report transport errors as smoke failures.
        return False, str(exc)


def request_restart(client: DeviceClient) -> None:
    try:
        response = client.post("/rest/restart", timeout=2)
        if response.status_code not in (200, 202):
            raise DeviceClientError(f"/rest/restart returned HTTP {response.status_code}")
    except Exception as exc:  # noqa: BLE001 - the socket may close while the reboot starts.
        print(f"[RESTART] request socket closed/failed after submit: {exc}", flush=True)


def wait_for_online(
    client: DeviceClient,
    before_boot_count: int | None,
    before_uptime: int | None,
    args: argparse.Namespace,
) -> dict[str, Any]:
    time.sleep(max(0.0, args.restart_grace_sec))
    deadline = time.monotonic() + max(1.0, args.restart_timeout_sec)
    last_error = ""
    while time.monotonic() < deadline:
        try:
            login_with_retry(client, max_wait_s=10.0)
            snapshot = collect_snapshot(client, args.tail_lines)
            compact = summarize_snapshot(snapshot)
            boot_count = compact.get("boot_count")
            uptime = compact.get("uptime_sec")
            restart_observed = False
            if isinstance(before_boot_count, int) and isinstance(boot_count, int):
                restart_observed = boot_count > before_boot_count
            if (
                not restart_observed
                and isinstance(before_uptime, int)
                and isinstance(uptime, int)
            ):
                restart_observed = uptime < before_uptime
            if restart_observed:
                return {"ok": True, "snapshot": snapshot, "compact": compact}
            last_error = (
                f"device online but restart not observed "
                f"(boot={boot_count}, uptime={uptime})"
            )
        except Exception as exc:  # noqa: BLE001 - expected while rebooting.
            last_error = str(exc)
        time.sleep(max(1.0, args.poll_interval_sec))
    return {"ok": False, "error": last_error or "timeout"}


def validate_cycle(
    cycle: int,
    compact: dict[str, Any],
    snapshot: dict[str, Any],
    baseline: dict[str, Any],
    task_baseline: int | None,
    args: argparse.Namespace,
) -> list[str]:
    failures: list[str] = []
    unexpected = compact.get("unexpected_restarts")
    baseline_unexpected = baseline.get("unexpected_restarts")
    task_count = compact.get("task_count")
    baseline_tasks = task_baseline

    if compact.get("last_boot_unexpected"):
        failures.append("lastBootUnexpected=true")
    if isinstance(unexpected, int) and isinstance(baseline_unexpected, int):
        if unexpected != baseline_unexpected:
            failures.append(f"unexpectedRestarts drifted {baseline_unexpected}->{unexpected}")
    if compact.get("last_shutdown_reason") != RESTART_COMMAND:
        failures.append(
            "lastShutdownReason is "
            f"{compact.get('last_shutdown_reason')} "
            f"({compact.get('last_shutdown_reason_name')}), expected restart_command"
        )
    if compact.get("current_reset_reason") != ESP_RST_SW:
        failures.append(
            "currentResetReason is "
            f"{compact.get('current_reset_reason')} "
            f"({compact.get('current_reset_reason_name')}), expected software"
        )
    if compact.get("watchdog_initialized") is not True:
        failures.append("watchdog not initialized")
    if isinstance(task_count, int) and isinstance(baseline_tasks, int):
        drift = abs(task_count - baseline_tasks)
        if drift > args.allowed_task_drift:
            failures.append(
                f"task count drift cycle={cycle}: baseline={baseline_tasks} current={task_count}"
            )

    bad_logs = scan_tail(snapshot)
    if bad_logs:
        failures.append("bad lifecycle log lines: " + " | ".join(bad_logs[-3:]))
    return failures


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    parser.add_argument("--cycles", type=int, default=10)
    parser.add_argument("--restart-timeout-sec", type=float, default=90.0)
    parser.add_argument("--restart-grace-sec", type=float, default=2.0)
    parser.add_argument("--poll-interval-sec", type=float, default=2.0)
    parser.add_argument("--tail-lines", type=int, default=128)
    parser.add_argument("--allowed-task-drift", type=int, default=0)
    parser.add_argument("--ws-timeout-sec", type=float, default=5.0)
    parser.add_argument("--require-websocket", action="store_true")
    args = parser.parse_args(argv)

    client = DeviceClient.from_args(args)
    try:
        login_with_retry(client)
        baseline_snapshot = collect_snapshot(client, args.tail_lines)
    except DeviceClientError as exc:
        print(f"ERROR: baseline collection failed: {exc}", file=sys.stderr)
        return 1

    baseline = summarize_snapshot(baseline_snapshot)
    print(
        "[BASELINE] "
        f"boot={baseline['boot_count']} "
        f"unexpected={baseline['unexpected_restarts']} "
        f"tasks={baseline['task_count']} "
        f"uptime={baseline['uptime_sec']}s",
        flush=True,
    )

    failures: list[str] = []
    cycle_results: list[dict[str, Any]] = []
    cycles = max(1, args.cycles)
    task_baseline: int | None = None

    for cycle in range(1, cycles + 1):
        print(f"[CYCLE {cycle}/{cycles}] requesting controlled restart", flush=True)
        before = summarize_snapshot(baseline_snapshot if cycle == 1 else cycle_results[-1]["snapshot"])
        request_restart(client)
        wait_result = wait_for_online(
            client,
            before.get("boot_count") if isinstance(before.get("boot_count"), int) else None,
            before.get("uptime_sec") if isinstance(before.get("uptime_sec"), int) else None,
            args,
        )
        if not wait_result.get("ok"):
            failures.append(f"cycle {cycle}: device did not return online: {wait_result.get('error')}")
            break

        snapshot = wait_result["snapshot"]
        compact = wait_result["compact"]
        ws_ok, ws_status = probe_ws_system(client, args.ws_timeout_sec, args.require_websocket)
        compact["websocket"] = ws_status
        cycle_failures = validate_cycle(cycle, compact, snapshot, baseline, task_baseline, args)
        if not ws_ok:
            cycle_failures.append(f"/ws/system reconnect failed: {ws_status}")

        if task_baseline is None and isinstance(compact.get("task_count"), int):
            task_baseline = compact["task_count"]

        print(
            f"[CYCLE {cycle}/{cycles}] "
            f"boot={compact['boot_count']} "
            f"uptime={compact['uptime_sec']}s "
            f"reset={compact['current_reset_reason_name']} "
            f"shutdown={compact['last_shutdown_reason_name']} "
            f"tasks={compact['task_count']} "
            f"ws={ws_status}",
            flush=True,
        )

        if cycle_failures:
            failures.extend(f"cycle {cycle}: {failure}" for failure in cycle_failures)

        cycle_results.append({"compact": compact, "snapshot": snapshot, "failures": cycle_failures})

    report = {
        "target": client.base_url,
        "cycles_requested": cycles,
        "cycles_completed": len(cycle_results),
        "baseline": baseline,
        "post_restart_task_baseline": task_baseline,
        "cycles": [item["compact"] for item in cycle_results],
        "failures": failures,
    }

    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))

    if failures:
        print("FAIL: controlled restart lifecycle smoke failed", file=sys.stderr)
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        return 1

    print("PASS: controlled restart lifecycle smoke succeeded")
    return 0


if __name__ == "__main__":
    sys.exit(main())
