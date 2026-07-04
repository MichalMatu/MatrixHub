#!/usr/bin/env python3
"""HTTPS/JWT endpoint stress and fault-injection smoke for MatrixHub."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import sys
import time
from collections import Counter
from pathlib import Path
from typing import Any
from urllib.parse import urlsplit

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parent))
try:
    from verify_system_websocket import RawWebSocket, WebSocketError, request_system_status_snapshot  # noqa: E402
except Exception:  # noqa: BLE001 - WSS scenarios are reported as unavailable.
    RawWebSocket = None  # type: ignore[assignment]
    WebSocketError = RuntimeError  # type: ignore[assignment]
    request_system_status_snapshot = None  # type: ignore[assignment]


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT_DIR = REPO_ROOT / "artifacts" / "stress"

READ_ENDPOINTS: tuple[tuple[str, str], ...] = (
    ("GET", "/api/system/info"),
    ("GET", "/api/system/tasks?details=1"),
    ("GET", "/api/system/network"),
    ("GET", "/api/diagnostics/summary"),
    ("GET", "/api/diagnostics/heap"),
    ("GET", "/api/diagnostics/tasks?details=0"),
    ("GET", "/api/diagnostics/mutexes"),
    ("GET", "/api/diagnostics/endpoints"),
    ("GET", "/api/diagnostics/features"),
    ("GET", "/api/config"),
    ("GET", "/rest/power/status"),
    ("GET", "/api/matrix/settings"),
    ("GET", "/api/alarms/rules?includeStatus=1"),
    ("GET", "/api/ble/status"),
    ("GET", "/api/ble/settings"),
    ("GET", "/api/wifisensing/config"),
    ("GET", "/api/shelly/devices"),
    ("GET", "/api/heartbeat"),
    ("GET", "/api/udp"),
    ("GET", "/api/notifications/settings"),
    ("GET", "/api/airmouse/status"),
    ("GET", "/api/keyboard/config"),
    ("GET", "/api/macros/status"),
    ("GET", "/api/compensation"),
    ("GET", "/api/usbterminal/config"),
    ("GET", "/api/logs"),
    ("GET", "/rest/logs/tail?lines=50"),
)

INVALID_JSON_ENDPOINTS: tuple[str, ...] = (
    "/api/config",
    "/api/matrix/settings",
    "/api/ble/settings",
    "/api/wifisensing/config",
    "/api/heartbeat",
    "/api/udp",
    "/api/alarms/rules",
)

BAD_LOG_PATTERNS = (
    "guru meditation",
    "panic",
    "abort() was called",
    "assert failed",
    "task watchdog got triggered",
    "watchdog timeout",
    "wdt reset",
    "brownout",
)


def now_iso() -> str:
    return dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def safe_slug(value: str) -> str:
    return "".join(ch if ch.isalnum() or ch in {"-", "_"} else "-" for ch in value).strip("-")


def nested(data: Any, path: tuple[str, ...], default: Any = None) -> Any:
    current: Any = data
    for key in path:
        if not isinstance(current, dict):
            return default
        current = current.get(key)
    return current if current is not None else default


def enforce_https(base_url: str) -> None:
    scheme = urlsplit(base_url).scheme
    if scheme != "https":
        raise DeviceClientError(f"stress_test requires HTTPS, got {scheme or 'missing scheme'}")


def login_with_retry(client: DeviceClient, max_wait_s: float = 75.0) -> str:
    if client.token:
        return client.login()

    deadline = time.monotonic() + max(0.0, max_wait_s)
    while True:
        response = client.session.post(
            f"{client.base_url}/rest/signIn",
            json={"username": client.username, "password": client.password},
            timeout=client.timeout,
        )
        if response.status_code == 429 and time.monotonic() < deadline:
            print("[AUTH] login rate limited; waiting 5s", file=sys.stderr, flush=True)
            time.sleep(min(5.0, max(0.1, deadline - time.monotonic())))
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


def percentile(values: list[float], pct: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, int(round((len(ordered) - 1) * pct))))
    return round(ordered[index], 2)


def request_result(
    client: DeviceClient,
    scenario: str,
    method: str,
    path: str,
    *,
    expected: tuple[int, ...] = (200,),
    **kwargs: Any,
) -> dict[str, Any]:
    started = time.perf_counter()
    result: dict[str, Any] = {
        "scenario": scenario,
        "method": method,
        "path": path,
        "ok": False,
        "latency_ms": 0.0,
    }
    try:
        response = client.request(method, path, **kwargs)
        result["latency_ms"] = round((time.perf_counter() - started) * 1000, 2)
        result["status"] = response.status_code
        result["bytes"] = len(response.content)
        result["ok"] = response.status_code in expected
        if not result["ok"]:
            result["error"] = f"HTTP {response.status_code}: {response.text[:240]}"
    except Exception as exc:  # noqa: BLE001 - stress should keep running and report failures.
        result["latency_ms"] = round((time.perf_counter() - started) * 1000, 2)
        result["status"] = None
        result["bytes"] = 0
        result["error"] = str(exc)
    return result


def synthetic_result(
    scenario: str,
    name: str,
    ok: bool,
    *,
    latency_ms: float = 0.0,
    skipped: bool = False,
    error: str | None = None,
    details: dict[str, Any] | None = None,
) -> dict[str, Any]:
    result: dict[str, Any] = {
        "scenario": scenario,
        "method": "SCENARIO",
        "path": name,
        "ok": ok,
        "latency_ms": round(latency_ms, 2),
    }
    if skipped:
        result["skipped"] = True
    if error:
        result["error"] = error
    if details:
        result["details"] = details
    return result


def json_or_empty(client: DeviceClient, path: str) -> dict[str, Any]:
    try:
        data = client.json("GET", path)
        return data if isinstance(data, dict) else {}
    except Exception:
        return {}


def diagnostics_feature(client: DeviceClient, key: str) -> dict[str, Any] | None:
    features = json_or_empty(client, "/api/diagnostics/features").get("features")
    if not isinstance(features, list):
        return None
    for feature in features:
        if isinstance(feature, dict) and feature.get("key") == key:
            return feature
    return None


def collect_snapshot(client: DeviceClient, label: str) -> dict[str, Any]:
    summary = json_or_empty(client, "/api/diagnostics/summary")
    heap = json_or_empty(client, "/api/diagnostics/heap")
    tasks = json_or_empty(client, "/api/system/tasks?details=1")
    mutexes = json_or_empty(client, "/api/diagnostics/mutexes")
    tail = json_or_empty(client, "/rest/logs/tail?lines=120")
    regions = heap.get("regions") if isinstance(heap.get("regions"), dict) else {}
    internal = regions.get("internal") if isinstance(regions.get("internal"), dict) else {}
    psram = regions.get("psram") if isinstance(regions.get("psram"), dict) else {}
    default = regions.get("default") if isinstance(regions.get("default"), dict) else {}
    stack = tasks.get("stack") if isinstance(tasks.get("stack"), dict) else {}
    return {
        "label": label,
        "timestamp": now_iso(),
        "summary": summary,
        "heap": heap,
        "tasks": tasks,
        "mutexes": mutexes,
        "tail": tail,
        "boot_count": nested(summary, ("boot", "bootCount")),
        "unexpected_restarts": nested(summary, ("boot", "unexpectedRestarts")),
        "default_free": default.get("free"),
        "internal_free": internal.get("free"),
        "psram_free": psram.get("free"),
        "internal_largest_block": internal.get("largestBlock"),
        "psram_largest_block": psram.get("largestBlock"),
        "min_stack_watermark": stack.get("worstHighWaterMark"),
        "min_stack_task": stack.get("worstTask"),
        "ws_queue_drops": nested(summary, ("http", "wsQueueDrops")),
        "ws_active_clients": nested(summary, ("http", "wsActiveClients")),
        "lock_standard_timeouts": nested(mutexes, ("runtime", "standard", "timeouts")),
        "lock_recursive_timeouts": nested(mutexes, ("runtime", "recursive", "timeouts")),
        "lock_standard_slow_acquires": nested(mutexes, ("runtime", "standard", "slowAcquires")),
        "lock_recursive_slow_acquires": nested(mutexes, ("runtime", "recursive", "slowAcquires")),
    }


def numeric_delta(before: dict[str, Any], after: dict[str, Any], key: str) -> int | None:
    left = before.get(key)
    right = after.get(key)
    if isinstance(left, int) and isinstance(right, int):
        return right - left
    return None


def run_endpoint_stress(
    client: DeviceClient,
    cycles: int,
    delay: float,
    verbose: bool,
) -> list[dict[str, Any]]:
    results: list[dict[str, Any]] = []
    total = cycles * len(READ_ENDPOINTS)
    request_no = 0
    for _cycle in range(cycles):
        for method, path in READ_ENDPOINTS:
            request_no += 1
            result = request_result(client, "endpoint_stress", method, path)
            results.append(result)
            if verbose:
                label = "PASS" if result["ok"] else "FAIL"
                print(
                    f"[{label}] {method} {path} status={result.get('status')} "
                    f"{result['latency_ms']:.2f}ms bytes={result.get('bytes')}"
                )
            elif request_no % 25 == 0:
                print(f"  endpoint stress: {request_no}/{total} requests", end="\r", flush=True)
            if delay > 0:
                time.sleep(delay)
    if not verbose:
        print()
    return results


def run_invalid_payload_flood(client: DeviceClient, attempts: int) -> list[dict[str, Any]]:
    results: list[dict[str, Any]] = []
    if attempts <= 0:
        return results
    payload = "{ invalid json"
    headers = {"Content-Type": "application/json"}
    for index in range(attempts):
        path = INVALID_JSON_ENDPOINTS[index % len(INVALID_JSON_ENDPOINTS)]
        results.append(
            request_result(
                client,
                "invalid_payload_flood",
                "POST",
                path,
                expected=(400, 413, 422, 429),
                data=payload,
                headers=headers,
            )
        )
    return results


def run_wss_reconnect_loop(client: DeviceClient, loops: int, timeout: float) -> list[dict[str, Any]]:
    results: list[dict[str, Any]] = []
    if loops <= 0:
        return results
    if RawWebSocket is None or request_system_status_snapshot is None:
        return [synthetic_result("wss_reconnect", "/ws/system", False, error="WSS helper unavailable")]
    for _index in range(loops):
        started = time.perf_counter()
        try:
            with RawWebSocket(client, timeout=timeout) as ws:
                frame = request_system_status_snapshot(ws, timeout)
            results.append(
                synthetic_result(
                    "wss_reconnect",
                    "/ws/system",
                    True,
                    latency_ms=(time.perf_counter() - started) * 1000,
                    details={
                        "type": frame.get("type") if isinstance(frame, dict) else None,
                        "channel": frame.get("channel") if isinstance(frame, dict) else None,
                    },
                )
            )
        except Exception as exc:  # noqa: BLE001
            results.append(
                synthetic_result(
                    "wss_reconnect",
                    "/ws/system",
                    False,
                    latency_ms=(time.perf_counter() - started) * 1000,
                    error=str(exc),
                )
            )
    return results


def run_wss_multi_client(client: DeviceClient, clients: int, timeout: float) -> list[dict[str, Any]]:
    if clients <= 0:
        return []
    if RawWebSocket is None or request_system_status_snapshot is None:
        return [synthetic_result("wss_multi_client", "/ws/system", False, error="WSS helper unavailable")]

    sockets: list[Any] = []
    started = time.perf_counter()
    try:
        for _index in range(clients):
            ws = RawWebSocket(client, timeout=timeout)
            ws.connect()
            sockets.append(ws)
        for ws in sockets:
            request_system_status_snapshot(ws, timeout)
        return [
            synthetic_result(
                "wss_multi_client",
                "/ws/system",
                True,
                latency_ms=(time.perf_counter() - started) * 1000,
                details={"clients": clients},
            )
        ]
    except Exception as exc:  # noqa: BLE001
        return [
            synthetic_result(
                "wss_multi_client",
                "/ws/system",
                False,
                latency_ms=(time.perf_counter() - started) * 1000,
                error=str(exc),
                details={"clients": clients},
            )
        ]
    finally:
        for ws in sockets:
            ws.close()


def run_csi_connect_loop(client: DeviceClient, loops: int, timeout: float) -> list[dict[str, Any]]:
    results: list[dict[str, Any]] = []
    if loops <= 0:
        return results
    if RawWebSocket is None:
        return [synthetic_result("csi_connect_loop", "/ws/csi", False, error="WSS helper unavailable")]
    csi_feature = diagnostics_feature(client, "csi")
    if csi_feature and csi_feature.get("configuredEnabled") is not True:
        return [
            synthetic_result(
                "csi_connect_loop",
                "/ws/csi",
                True,
                skipped=True,
                details={"reason": "CSI disabled in current runtime config", "feature": csi_feature},
            )
        ]
    for _index in range(loops):
        started = time.perf_counter()
        try:
            with RawWebSocket(client, path="/ws/csi", timeout=timeout):
                time.sleep(0.05)
            results.append(
                synthetic_result(
                    "csi_connect_loop",
                    "/ws/csi",
                    True,
                    latency_ms=(time.perf_counter() - started) * 1000,
                )
            )
        except Exception as exc:  # noqa: BLE001
            results.append(
                synthetic_result(
                    "csi_connect_loop",
                    "/ws/csi",
                    False,
                    latency_ms=(time.perf_counter() - started) * 1000,
                    error=str(exc),
                )
            )
    return results


def run_ble_scan_loop(client: DeviceClient, loops: int) -> list[dict[str, Any]]:
    if loops <= 0:
        return []
    status = json_or_empty(client, "/api/ble/status")
    if status.get("enabled") is not True:
        return [
            synthetic_result(
                "ble_scan_loop",
                "/api/ble/scan",
                True,
                skipped=True,
                details={"reason": "BLE disabled in current runtime config"},
            )
        ]

    results: list[dict[str, Any]] = []
    for _index in range(loops):
        results.append(
            request_result(
                client,
                "ble_scan_loop",
                "POST",
                "/api/ble/scan?timeout=1000",
                expected=(200,),
            )
        )
        time.sleep(0.05)
        results.append(
            request_result(
                client,
                "ble_scan_loop",
                "DELETE",
                "/api/ble/scan",
                expected=(200,),
            )
        )
    return results


def run_file_loop(client: DeviceClient, loops: int) -> list[dict[str, Any]]:
    results: list[dict[str, Any]] = []
    if loops <= 0:
        return results
    stamp = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%S")
    for index in range(loops):
        filename = f"matrixhub_stress_{stamp}_{index}.txt"
        path = f"/{filename}"
        payload = f"MatrixHub stress fixture {stamp} #{index}\n".encode("utf-8")
        upload_started = time.perf_counter()
        try:
            response = client.session.post(
                f"{client.base_url}/rest/fs/upload",
                params={"path": "/"},
                files={"file": (filename, payload, "text/plain")},
                timeout=client.timeout,
            )
            results.append(
                {
                    "scenario": "file_upload_download_loop",
                    "method": "POST",
                    "path": "/rest/fs/upload",
                    "ok": response.status_code == 200,
                    "status": response.status_code,
                    "bytes": len(response.content),
                    "latency_ms": round((time.perf_counter() - upload_started) * 1000, 2),
                    **({} if response.status_code == 200 else {"error": response.text[:240]}),
                }
            )

            download = request_result(
                client,
                "file_upload_download_loop",
                "GET",
                "/rest/fs/download",
                params={"path": path},
            )
            if download.get("ok"):
                downloaded = client.session.get(
                    f"{client.base_url}/rest/fs/download",
                    params={"path": path},
                    timeout=client.timeout,
                )
                download["ok"] = downloaded.status_code == 200 and downloaded.content == payload
                download["status"] = downloaded.status_code
                download["bytes"] = len(downloaded.content)
                if not download["ok"]:
                    download["error"] = "downloaded content did not match uploaded fixture"
            results.append(download)
        except Exception as exc:  # noqa: BLE001
            results.append(
                synthetic_result(
                    "file_upload_download_loop",
                    path,
                    False,
                    latency_ms=(time.perf_counter() - upload_started) * 1000,
                    error=str(exc),
                )
            )
        finally:
            results.append(
                request_result(
                    client,
                    "file_upload_download_loop",
                    "DELETE",
                    "/rest/fs/remove",
                    expected=(200, 404),
                    params={"path": path},
                )
            )
    return results


def run_wifi_recovery_probe(client: DeviceClient) -> list[dict[str, Any]]:
    result = request_result(
        client,
        "wifi_recovery_probe",
        "POST",
        "/api/system/wifi/recover",
        expected=(200,),
    )
    if result.get("ok"):
        try:
            body = client.session.get(f"{client.base_url}/api/system/network", timeout=client.timeout).json()
            result["details"] = {"network_ok": isinstance(body, dict)}
        except Exception as exc:  # noqa: BLE001
            result["ok"] = False
            result["error"] = f"post-recovery network read failed: {exc}"
    return [result]


def run_macro_loop(client: DeviceClient, loops: int) -> list[dict[str, Any]]:
    if loops <= 0:
        return []
    settings = json_or_empty(client, "/api/macros/settings")
    if settings.get("enabled") is not True:
        return [
            synthetic_result(
                "macro_run_stop_loop",
                "/api/macros/run",
                True,
                skipped=True,
                details={"reason": "macros disabled in current runtime config"},
            )
        ]

    filename = "matrixhub_stress_noop.txt"
    results: list[dict[str, Any]] = []
    results.append(
        request_result(
            client,
            "macro_run_stop_loop",
            "POST",
            "/api/macros",
            json={"filename": filename, "content": "REM MatrixHub stress no-op\nDELAY 1\n"},
        )
    )
    for _index in range(loops):
        results.append(
            request_result(
                client,
                "macro_run_stop_loop",
                "POST",
                "/api/macros/run",
                json={"name": filename},
                expected=(200, 500),
            )
        )
        time.sleep(0.05)
        results.append(request_result(client, "macro_run_stop_loop", "POST", "/api/macros/stop"))
    results.append(
        request_result(
            client,
            "macro_run_stop_loop",
            "POST",
            "/api/macros/delete",
            expected=(200, 404),
            json={"name": filename},
        )
    )
    for result in results:
        if result["scenario"] == "macro_run_stop_loop" and result.get("status") == 500:
            result["ok"] = False
            result["error"] = result.get("error") or "macro run failed"
    return results


def scan_bad_logs(snapshot: dict[str, Any]) -> list[str]:
    lines = nested(snapshot, ("tail", "lines"), []) or []
    matches: list[str] = []
    for line in lines:
        text = str(line)
        lower = text.lower()
        if any(pattern in lower for pattern in BAD_LOG_PATTERNS):
            matches.append(text)
    return matches


def summarize_results(
    target: str,
    started_at: str,
    finished_at: str,
    before: dict[str, Any],
    after: dict[str, Any],
    results: list[dict[str, Any]],
) -> dict[str, Any]:
    failures = [result for result in results if not result.get("ok")]
    latencies = [float(result.get("latency_ms", 0.0)) for result in results if result.get("latency_ms")]
    scenario_counts = Counter(str(result.get("scenario", "unknown")) for result in results)
    before_bad_logs = set(scan_bad_logs(before))
    bad_logs = [line for line in scan_bad_logs(after) if line not in before_bad_logs]

    runtime = {
        "heap_drift": {
            "default_free": numeric_delta(before, after, "default_free"),
            "internal_free": numeric_delta(before, after, "internal_free"),
            "psram_free": numeric_delta(before, after, "psram_free"),
            "internal_largest_block": numeric_delta(before, after, "internal_largest_block"),
            "psram_largest_block": numeric_delta(before, after, "psram_largest_block"),
        },
        "min_stack_watermark": after.get("min_stack_watermark"),
        "min_stack_task": after.get("min_stack_task"),
        "lock_contention_delta": {
            "standard_timeouts": numeric_delta(before, after, "lock_standard_timeouts"),
            "recursive_timeouts": numeric_delta(before, after, "lock_recursive_timeouts"),
            "standard_slow_acquires": numeric_delta(before, after, "lock_standard_slow_acquires"),
            "recursive_slow_acquires": numeric_delta(before, after, "lock_recursive_slow_acquires"),
        },
        "ws_queue_drops_delta": numeric_delta(before, after, "ws_queue_drops"),
        "reset_detection": {
            "boot_count_delta": numeric_delta(before, after, "boot_count"),
            "unexpected_restarts_delta": numeric_delta(before, after, "unexpected_restarts"),
        },
        "bad_log_matches": bad_logs,
        "pre_existing_bad_log_matches": len(before_bad_logs),
    }

    hard_failures = list(failures)
    if runtime["reset_detection"]["boot_count_delta"] not in (None, 0):
        hard_failures.append({"scenario": "runtime", "error": "boot_count changed during stress"})
    if runtime["reset_detection"]["unexpected_restarts_delta"] not in (None, 0):
        hard_failures.append({"scenario": "runtime", "error": "unexpected_restarts changed during stress"})
    if runtime["ws_queue_drops_delta"] not in (None, 0):
        hard_failures.append({"scenario": "runtime", "error": "ws_queue_drops changed during stress"})
    if bad_logs:
        hard_failures.append({"scenario": "runtime", "error": "bad log pattern detected"})

    return {
        "ok": len(hard_failures) == 0,
        "target": target,
        "started_at": started_at,
        "finished_at": finished_at,
        "summary": {
            "checks": len(results),
            "passed": len(results) - len(failures),
            "failed": len(failures),
            "hard_failures": len(hard_failures),
            "avg_latency_ms": round(sum(latencies) / len(latencies), 2) if latencies else 0.0,
            "p95_latency_ms": percentile(latencies, 0.95),
            "max_latency_ms": round(max(latencies), 2) if latencies else 0.0,
            "scenario_counts": dict(sorted(scenario_counts.items())),
        },
        "runtime": runtime,
        "before": before,
        "after": after,
        "results": results,
    }


def render_markdown(report: dict[str, Any]) -> str:
    summary = report["summary"]
    runtime = report["runtime"]
    lines = [
        "# MatrixHub Stress Report",
        "",
        f"- Target: `{report['target']}`",
        f"- Started: `{report['started_at']}`",
        f"- Finished: `{report['finished_at']}`",
        f"- Result: `{'PASS' if report['ok'] else 'FAIL'}`",
        f"- Requests/checks: `{summary['checks']}`",
        f"- Failures: `{summary['failed']}`",
        f"- Average latency: `{summary['avg_latency_ms']} ms`",
        f"- P95 latency: `{summary['p95_latency_ms']} ms`",
        f"- Max latency: `{summary['max_latency_ms']} ms`",
        "",
        "## Runtime",
        "",
        f"- Heap drift: `{runtime['heap_drift']}`",
        f"- Min stack watermark: `{runtime['min_stack_watermark']}` (`{runtime['min_stack_task']}`)",
        f"- Lock contention delta: `{runtime['lock_contention_delta']}`",
        f"- WS queue drops delta: `{runtime['ws_queue_drops_delta']}`",
        f"- Reset detection: `{runtime['reset_detection']}`",
        f"- Bad log matches: `{len(runtime['bad_log_matches'])}`",
        f"- Pre-existing bad log matches: `{runtime['pre_existing_bad_log_matches']}`",
        "",
        "## Scenarios",
        "",
    ]
    for name, count in summary["scenario_counts"].items():
        lines.append(f"- `{name}`: {count}")
    lines += [
        "",
        "## Failures",
        "",
    ]
    failures = [result for result in report["results"] if not result.get("ok")]
    if not failures:
        lines.append("- none")
    else:
        for result in failures[:50]:
            lines.append(
                f"- `{result.get('scenario')}` `{result.get('method')}` "
                f"`{result.get('path')}`: {result.get('error')}"
            )
    return "\n".join(lines) + "\n"


def write_reports(report: dict[str, Any], report_dir: Path) -> dict[str, str]:
    report_dir.mkdir(parents=True, exist_ok=True)
    stamp = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    target_slug = safe_slug(urlsplit(report["target"]).netloc or "device")
    base = f"stress-{target_slug}-{stamp}"
    json_path = report_dir / f"{base}.json"
    md_path = report_dir / f"{base}.md"
    paths = {"json": str(json_path), "markdown": str(md_path)}
    with_paths = dict(report)
    with_paths["reports"] = paths
    json_path.write_text(json.dumps(with_paths, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    md_path.write_text(render_markdown(with_paths), encoding="utf-8")
    return paths


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    parser.add_argument("--cycles", type=int, default=20, help="Read-only endpoint stress cycles. Default: 20.")
    parser.add_argument("--delay", type=float, default=0.05, help="Delay between endpoint requests. Default: 0.05.")
    parser.add_argument("--quiet", action="store_true", help="Only print summary.")
    parser.add_argument("--login-timeout-sec", type=float, default=75.0)
    parser.add_argument("--skip-faults", action="store_true", help="Skip safe fault-injection scenarios.")
    parser.add_argument("--invalid-payloads", type=int, default=10)
    parser.add_argument("--wss-reconnects", type=int, default=5)
    parser.add_argument("--frontend-clients", type=int, default=3)
    parser.add_argument("--csi-loops", type=int, default=3)
    parser.add_argument("--ble-loops", type=int, default=3)
    parser.add_argument("--file-loops", type=int, default=3)
    parser.add_argument("--macro-loop", action="store_true", help="Run an inert macro run/stop loop if macros are enabled.")
    parser.add_argument("--macro-loops", type=int, default=2)
    parser.add_argument("--wss-timeout-sec", type=float, default=8.0)
    parser.add_argument("--report-dir", default=str(DEFAULT_REPORT_DIR))
    parser.add_argument("--no-report-files", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)

    client = DeviceClient.from_args(args)
    started_at = now_iso()
    report_paths: dict[str, str] = {}
    try:
        enforce_https(client.base_url)
        login_with_retry(client, args.login_timeout_sec)
    except DeviceClientError as exc:
        print(f"[AUTH] {exc}", file=sys.stderr)
        return 1

    print(f"Target: {client.base_url}")
    before = collect_snapshot(client, "before")
    results = run_endpoint_stress(
        client,
        max(1, args.cycles),
        max(0.0, args.delay),
        not args.quiet,
    )

    if not args.skip_faults:
        print("running safe fault scenarios...")
        results.extend(run_invalid_payload_flood(client, max(0, args.invalid_payloads)))
        results.extend(run_wss_reconnect_loop(client, max(0, args.wss_reconnects), args.wss_timeout_sec))
        results.extend(run_wss_multi_client(client, max(0, args.frontend_clients), args.wss_timeout_sec))
        results.extend(run_csi_connect_loop(client, max(0, args.csi_loops), args.wss_timeout_sec))
        results.extend(run_ble_scan_loop(client, max(0, args.ble_loops)))
        results.extend(run_file_loop(client, max(0, args.file_loops)))
        results.extend(run_wifi_recovery_probe(client))
        if args.macro_loop:
            results.extend(run_macro_loop(client, max(1, args.macro_loops)))
        else:
            results.append(
                synthetic_result(
                    "macro_run_stop_loop",
                    "/api/macros/run",
                    True,
                    skipped=True,
                    details={"reason": "macro_loop flag not set; avoids unintended HID activity"},
                )
            )

    after = collect_snapshot(client, "after")
    report = summarize_results(client.base_url, started_at, now_iso(), before, after, results)

    if not args.no_report_files:
        report_paths = write_reports(report, Path(args.report_dir))
        report["reports"] = report_paths

    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        summary = report["summary"]
        label = "PASS" if report["ok"] else "FAIL"
        print(
            f"[STRESS] {label} requests={summary['checks']} "
            f"success={summary['passed']} failed={summary['failed']} "
            f"avg={summary['avg_latency_ms']}ms p95={summary['p95_latency_ms']}ms"
        )
        if report_paths:
            print(f"JSON report: {report_paths['json']}")
            print(f"Markdown report: {report_paths['markdown']}")
        for result in report["results"]:
            if not result.get("ok"):
                print(
                    f"FAIL {result.get('scenario')} {result.get('method')} "
                    f"{result.get('path')}: {result.get('error')}",
                    file=sys.stderr,
                )
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
