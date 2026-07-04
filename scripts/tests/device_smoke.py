#!/usr/bin/env python3
"""Comprehensive HTTPS/JWT smoke suite for a live MatrixHub device."""

from __future__ import annotations

import argparse
import json
import sys
import time
from collections.abc import Callable
from copy import deepcopy
from datetime import datetime, timezone
from pathlib import Path
from typing import Any
from urllib.parse import urlsplit, urlunsplit

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parent))

try:
    from verify_system_websocket import RawWebSocket, WebSocketError, request_system_status_snapshot  # noqa: E402
except Exception:  # noqa: BLE001 - WSS checks are optional and reported when unavailable.
    RawWebSocket = None  # type: ignore[assignment]
    WebSocketError = RuntimeError  # type: ignore[assignment]
    request_system_status_snapshot = None  # type: ignore[assignment]


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT_DIR = REPO_ROOT / "artifacts" / "device-smoke"
DEFAULT_HEAP_DROP_LIMIT_BYTES = 65536

JsonValidator = Callable[[Any], list[str]]


def is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def require_object(name: str, value: Any) -> list[str]:
    return [] if isinstance(value, dict) else [f"{name} must be an object"]


def require_array(name: str, value: Any) -> list[str]:
    return [] if isinstance(value, list) else [f"{name} must be an array"]


def require_keys(name: str, value: Any, keys: tuple[str, ...]) -> list[str]:
    errors = require_object(name, value)
    if errors:
        return errors
    return [f"{name}.{key} is missing" for key in keys if key not in value]


def require_type(name: str, value: Any, key: str, expected: type | tuple[type, ...]) -> list[str]:
    errors = require_object(name, value)
    if errors:
        return errors
    item = value.get(key)
    if expected is bool:
        return [] if isinstance(item, bool) else [f"{name}.{key} must be bool"]
    if expected is int:
        return [] if isinstance(item, int) and not isinstance(item, bool) else [f"{name}.{key} must be int"]
    if expected is str:
        return [] if isinstance(item, str) else [f"{name}.{key} must be string"]
    if expected is list:
        return [] if isinstance(item, list) else [f"{name}.{key} must be array"]
    if expected is dict:
        return [] if isinstance(item, dict) else [f"{name}.{key} must be object"]
    return [] if isinstance(item, expected) else [f"{name}.{key} has unexpected type"]


def validate_system_info(data: Any) -> list[str]:
    errors = require_keys("system_info", data, ("firmware_version", "free_heap", "total_heap", "uptime"))
    if not errors:
        if not isinstance(data.get("firmware_version"), str):
            errors.append("system_info.firmware_version must be string")
        for key in ("free_heap", "total_heap", "uptime"):
            if not is_number(data.get(key)):
                errors.append(f"system_info.{key} must be number")
    return errors


def validate_tasks(data: Any) -> list[str]:
    errors = require_keys("tasks", data, ("watchdog", "taskCount", "memory"))
    if not errors:
        errors += require_type("tasks", data, "watchdog", dict)
        errors += require_type("tasks", data, "memory", dict)
        if not isinstance(data.get("taskCount"), int):
            errors.append("tasks.taskCount must be int")
    return errors


def validate_system_network(data: Any) -> list[str]:
    return require_keys("system_network", data, ("wifi", "ap", "http", "forwarding"))


def validate_config(data: Any) -> list[str]:
    return require_object("config", data)


def validate_power_status(data: Any) -> list[str]:
    errors = require_keys(
        "power_status",
        data,
        (
            "sleep_enabled",
            "uptime_ms",
            "wake_reason",
            "wake_timer_enabled",
            "wake_button_enabled",
            "wake_touch_enabled",
            "wake_interval_ms",
            "timer_wake_awake_ms",
            "button_wake_awake_ms",
            "wake_touch_gpio",
            "wake_touch_threshold",
            "wake_awake_window_ms",
            "wake_awake_eta_ms",
        ),
    )
    if not errors:
        errors += require_type("power_status", data, "sleep_enabled", bool)
        errors += require_type("power_status", data, "wake_reason", str)
        for key in ("wake_timer_enabled", "wake_button_enabled", "wake_touch_enabled"):
            errors += require_type("power_status", data, key, bool)
        for key in (
            "wake_interval_ms",
            "timer_wake_awake_ms",
            "button_wake_awake_ms",
            "wake_touch_gpio",
            "wake_touch_threshold",
            "wake_awake_window_ms",
            "wake_awake_eta_ms",
        ):
            if not is_number(data.get(key)):
                errors.append(f"power_status.{key} must be number")
        if not is_number(data.get("uptime_ms")):
            errors.append("power_status.uptime_ms must be number")
        if "thermal_state" in data:
            errors += require_type("power_status", data, "thermal_state", str)
        if "thermal_cpu_mhz" in data and not is_number(data.get("thermal_cpu_mhz")):
            errors.append("power_status.thermal_cpu_mhz must be number")
    return errors


def validate_power_config(data: Any) -> list[str]:
    errors = require_keys(
        "power_config",
        data,
        (
            "sleep_enabled",
            "inactivity_timeout_ms",
            "grace_after_boot_ms",
            "wake_timer_enabled",
            "wake_button_enabled",
            "wake_touch_enabled",
            "wake_interval_ms",
            "timer_wake_awake_ms",
            "button_wake_awake_ms",
            "wake_touch_gpio",
            "wake_touch_threshold",
        ),
    )
    if not errors:
        errors += require_type("power_config", data, "sleep_enabled", bool)
        for key in ("wake_timer_enabled", "wake_button_enabled", "wake_touch_enabled"):
            errors += require_type("power_config", data, key, bool)
    return errors


def validate_matrix_settings(data: Any) -> list[str]:
    return require_keys(
        "matrix_settings",
        data,
        ("brightness", "alarm_mode", "rotation", "effect_enabled", "effect_mode"),
    )


def validate_alarm_rules(data: Any) -> list[str]:
    errors = require_keys("alarm_rules", data, ("schema_version", "rules"))
    if not errors:
        errors += require_type("alarm_rules", data, "rules", list)
    return errors


def validate_ble_status(data: Any) -> list[str]:
    errors = require_keys("ble_status", data, ("enabled", "running", "devices"))
    if not errors:
        errors += require_type("ble_status", data, "enabled", bool)
        errors += require_type("ble_status", data, "running", bool)
        errors += require_type("ble_status", data, "devices", list)
    return errors


def validate_ble_settings(data: Any) -> list[str]:
    errors = require_keys("ble_settings", data, ("enabled", "sensors"))
    if not errors:
        errors += require_type("ble_settings", data, "enabled", bool)
        errors += require_type("ble_settings", data, "sensors", list)
    return errors


def validate_wifisensing_config(data: Any) -> list[str]:
    errors = require_keys("wifisensing_config", data, ("enabled", "sample_interval_ms", "variance_threshold"))
    if not errors:
        errors += require_type("wifisensing_config", data, "enabled", bool)
        if not is_number(data.get("sample_interval_ms")):
            errors.append("wifisensing_config.sample_interval_ms must be number")
        if not is_number(data.get("variance_threshold")):
            errors.append("wifisensing_config.variance_threshold must be number")
    return errors


def validate_shelly_devices(data: Any) -> list[str]:
    return require_array("shelly_devices", data)


def validate_heartbeat(data: Any) -> list[str]:
    errors = require_keys("heartbeat", data, ("interval_ms", "slots"))
    if not errors:
        if not isinstance(data.get("interval_ms"), int):
            errors.append("heartbeat.interval_ms must be int")
        errors += require_type("heartbeat", data, "slots", list)
    return errors


def validate_udp(data: Any) -> list[str]:
    errors = require_keys("udp", data, ("enabled", "host", "port", "format", "interval_ms"))
    if not errors:
        errors += require_type("udp", data, "enabled", bool)
        errors += require_type("udp", data, "host", str)
        errors += require_type("udp", data, "port", int)
        errors += require_type("udp", data, "format", str)
        errors += require_type("udp", data, "interval_ms", int)
    return errors


def validate_notifications(data: Any) -> list[str]:
    errors = require_keys(
        "notifications",
        data,
        ("telegram_enabled", "webhook_enabled", "pushover_enabled", "is_configured"),
    )
    if not errors:
        for key in ("telegram_enabled", "webhook_enabled", "pushover_enabled", "is_configured"):
            errors += require_type("notifications", data, key, bool)
    return errors


def validate_object(data: Any) -> list[str]:
    return require_object("response", data)


def validate_keyboard_config(data: Any) -> list[str]:
    errors = require_keys("keyboard_config", data, ("enabled",))
    if not errors:
        errors += require_type("keyboard_config", data, "enabled", bool)
    return errors


def validate_macro_status(data: Any) -> list[str]:
    return require_keys("macro_status", data, ("current_script", "status", "current_line", "uptime_ms"))


def validate_compensation(data: Any) -> list[str]:
    errors = require_keys(
        "compensation",
        data,
        ("enabled", "base_temp_offset", "reference_cpu_temp", "temp_offset_per_cpu_degree"),
    )
    if not errors:
        errors += require_type("compensation", data, "enabled", bool)
    return errors


def validate_logs(data: Any) -> list[str]:
    errors = require_keys("logs", data, ("months",))
    if not errors:
        errors += require_type("logs", data, "months", list)
    return errors


def validate_tail(data: Any) -> list[str]:
    errors = require_keys("tail", data, ("lines",))
    if not errors:
        errors += require_type("tail", data, "lines", list)
    return errors


def validate_heap(data: Any) -> list[str]:
    errors = require_keys("heap", data, ("schema", "regions"))
    if errors:
        return errors
    regions = data.get("regions")
    errors += require_keys("heap.regions", regions, ("default", "internal", "psram"))
    if not errors:
        for region_name in ("default", "internal", "psram"):
            region = regions.get(region_name)
            errors += require_keys(
                f"heap.regions.{region_name}",
                region,
                ("total", "free", "minimumFree", "largestBlock", "fragmentationPercent"),
            )
    return errors


READ_ONLY_ENDPOINTS: tuple[dict[str, Any], ...] = (
    {"name": "system.info", "method": "GET", "path": "/api/system/info", "validator": validate_system_info},
    {"name": "system.tasks", "method": "GET", "path": "/api/system/tasks?details=1", "validator": validate_tasks},
    {"name": "system.network", "method": "GET", "path": "/api/system/network", "validator": validate_system_network},
    {"name": "config", "method": "GET", "path": "/api/config", "validator": validate_config},
    {"name": "power.status", "method": "GET", "path": "/rest/power/status", "validator": validate_power_status},
    {"name": "matrix.settings", "method": "GET", "path": "/api/matrix/settings", "validator": validate_matrix_settings},
    {"name": "alarms.rules", "method": "GET", "path": "/api/alarms/rules?includeStatus=1", "validator": validate_alarm_rules},
    {"name": "ble.status", "method": "GET", "path": "/api/ble/status", "validator": validate_ble_status},
    {"name": "wifisensing.config", "method": "GET", "path": "/api/wifisensing/config", "validator": validate_wifisensing_config},
    {"name": "shelly.devices", "method": "GET", "path": "/api/shelly/devices", "validator": validate_shelly_devices},
    {"name": "heartbeat", "method": "GET", "path": "/api/heartbeat", "validator": validate_heartbeat},
    {"name": "udp", "method": "GET", "path": "/api/udp", "validator": validate_udp},
    {"name": "notifications.settings", "method": "GET", "path": "/api/notifications/settings", "validator": validate_notifications},
    {"name": "airmouse.status", "method": "GET", "path": "/api/airmouse/status", "validator": validate_object},
    {"name": "keyboard.config", "method": "GET", "path": "/api/keyboard/config", "validator": validate_keyboard_config},
    {"name": "macros.status", "method": "GET", "path": "/api/macros/status", "validator": validate_macro_status},
    {"name": "compensation", "method": "GET", "path": "/api/compensation", "validator": validate_compensation},
    {"name": "usbterminal.config", "method": "GET", "path": "/api/usbterminal/config", "validator": validate_object},
    {"name": "logs", "method": "GET", "path": "/api/logs", "validator": validate_logs},
    {"name": "logs.tail", "method": "GET", "path": "/rest/logs/tail?lines=50", "validator": validate_tail},
)


def now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def safe_slug(value: str) -> str:
    return "".join(ch if ch.isalnum() or ch in {"-", "_"} else "-" for ch in value).strip("-")


def json_equal(left: Any, right: Any) -> bool:
    return json.dumps(left, sort_keys=True, separators=(",", ":")) == json.dumps(
        right,
        sort_keys=True,
        separators=(",", ":"),
    )


def extract_heap(data: Any) -> dict[str, Any]:
    if not isinstance(data, dict):
        return {}
    regions = data.get("regions")
    if not isinstance(regions, dict):
        return {}
    compact: dict[str, Any] = {}
    for name in ("default", "internal", "psram"):
        region = regions.get(name)
        if isinstance(region, dict):
            compact[name] = {
                "total": region.get("total"),
                "free": region.get("free"),
                "minimumFree": region.get("minimumFree"),
                "largestBlock": region.get("largestBlock"),
                "fragmentationPercent": region.get("fragmentationPercent"),
            }
    return compact


def heap_delta(before: dict[str, Any], after: dict[str, Any]) -> dict[str, Any]:
    delta: dict[str, Any] = {}
    for region_name, before_region in before.items():
        after_region = after.get(region_name)
        if not isinstance(before_region, dict) or not isinstance(after_region, dict):
            continue
        delta[region_name] = {}
        for key in ("free", "minimumFree", "largestBlock"):
            if isinstance(before_region.get(key), int) and isinstance(after_region.get(key), int):
                delta[region_name][key] = after_region[key] - before_region[key]
    return delta


def new_result(name: str, method: str, path: str, started: float) -> dict[str, Any]:
    return {
        "name": name,
        "method": method,
        "path": path,
        "ok": False,
        "latency_ms": round((time.perf_counter() - started) * 1000, 2),
    }


def http_json_check(
    client: DeviceClient,
    name: str,
    method: str,
    path: str,
    *,
    json_body: Any = None,
    expected: tuple[int, ...] = (200,),
    validator: JsonValidator | None = None,
    timeout: float | None = None,
) -> tuple[dict[str, Any], Any]:
    started = time.perf_counter()
    body: Any = None
    result = new_result(name, method, path, started)
    try:
        kwargs: dict[str, Any] = {}
        if json_body is not None:
            kwargs["json"] = json_body
        if timeout is not None:
            kwargs["timeout"] = timeout
        response = client.request(method, path, **kwargs)
        result["latency_ms"] = round((time.perf_counter() - started) * 1000, 2)
        result["status"] = response.status_code
        result["bytes"] = len(response.content)
        if response.status_code not in expected:
            result["error"] = f"HTTP {response.status_code}: {response.text[:240]}"
            return result, body
        try:
            body = response.json()
        except ValueError as exc:
            result["error"] = f"non-JSON response: {exc}"
            return result, body
        schema_errors = validator(body) if validator else []
        if schema_errors:
            result["schema_errors"] = schema_errors
            result["error"] = "; ".join(schema_errors)
            return result, body
        result["ok"] = True
        return result, body
    except Exception as exc:  # noqa: BLE001 - smoke reports transport failures as results.
        result["latency_ms"] = round((time.perf_counter() - started) * 1000, 2)
        result["error"] = str(exc)
        return result, body


def raw_http_check(
    client: DeviceClient,
    name: str,
    method: str,
    path: str,
    *,
    expected: tuple[int, ...] = (200, 204),
    timeout: float | None = None,
) -> dict[str, Any]:
    started = time.perf_counter()
    result = new_result(name, method, path, started)
    try:
        kwargs: dict[str, Any] = {}
        if timeout is not None:
            kwargs["timeout"] = timeout
        response = client.request(method, path, **kwargs)
        result["latency_ms"] = round((time.perf_counter() - started) * 1000, 2)
        result["status"] = response.status_code
        result["bytes"] = len(response.content)
        result["ok"] = response.status_code in expected
        if not result["ok"]:
            result["error"] = f"HTTP {response.status_code}: {response.text[:240]}"
    except Exception as exc:  # noqa: BLE001
        result["latency_ms"] = round((time.perf_counter() - started) * 1000, 2)
        result["error"] = str(exc)
    return result


def append_verify_result(
    results: list[dict[str, Any]],
    name: str,
    ok: bool,
    details: dict[str, Any] | None = None,
    error: str | None = None,
) -> None:
    result: dict[str, Any] = {
        "name": name,
        "method": "VERIFY",
        "path": "",
        "ok": ok,
        "latency_ms": 0.0,
    }
    if details:
        result["details"] = details
    if error:
        result["error"] = error
    results.append(result)


def run_read_only(client: DeviceClient, timeout: float) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    results: list[dict[str, Any]] = []
    bodies: dict[str, Any] = {}
    for endpoint in READ_ONLY_ENDPOINTS:
        result, body = http_json_check(
            client,
            endpoint["name"],
            endpoint["method"],
            endpoint["path"],
            validator=endpoint["validator"],
            timeout=timeout,
        )
        results.append(result)
        bodies[endpoint["name"]] = body
    return results, bodies


def run_reauth_probe(client: DeviceClient, timeout: float) -> dict[str, Any]:
    started = time.perf_counter()
    result = new_result("auth.401_reauth", "GET", "/api/system/info", started)
    original_token = client.token or client.login()
    invalid_token = "invalid-token-for-device-smoke-reauth"
    body: Any = None
    try:
        client.set_token(invalid_token)
        first = client.session.get(f"{client.base_url}/api/system/info", timeout=timeout)
        client.set_token(original_token)
        second = client.session.get(f"{client.base_url}/api/system/info", timeout=timeout)
        result["latency_ms"] = round((time.perf_counter() - started) * 1000, 2)
        result["status"] = second.status_code
        result["bytes"] = len(second.content)
        result["details"] = {
            "purpose": "verify protected endpoints return 401 for an invalid JWT and recover with the current JWT",
            "invalid_token_status": first.status_code,
            "recovered_status": second.status_code,
        }
        if first.status_code != 401:
            result["error"] = f"invalid JWT returned HTTP {first.status_code}, expected 401"
            return result
        if second.status_code != 200:
            result["error"] = f"valid JWT recovery returned HTTP {second.status_code}: {second.text[:240]}"
            return result
        try:
            body = second.json()
        except ValueError as exc:
            result["error"] = f"recovery returned non-JSON response: {exc}"
            return result
        schema_errors = validate_system_info(body)
        if schema_errors:
            result["schema_errors"] = schema_errors
            result["error"] = "; ".join(schema_errors)
            return result
        result["ok"] = True
    except Exception as exc:  # noqa: BLE001
        result["latency_ms"] = round((time.perf_counter() - started) * 1000, 2)
        result["error"] = str(exc)
    finally:
        if original_token:
            client.set_token(original_token)
    return result


def get_required_json(
    client: DeviceClient,
    results: list[dict[str, Any]],
    name: str,
    path: str,
    validator: JsonValidator | None = None,
    timeout: float | None = None,
) -> Any | None:
    result, body = http_json_check(client, name, "GET", path, validator=validator, timeout=timeout)
    results.append(result)
    return body if result["ok"] else None


def safe_post_json(
    client: DeviceClient,
    results: list[dict[str, Any]],
    name: str,
    path: str,
    payload: Any,
    validator: JsonValidator | None = None,
    timeout: float | None = None,
) -> Any | None:
    result, body = http_json_check(
        client,
        name,
        "POST",
        path,
        json_body=payload,
        validator=validator,
        timeout=timeout,
    )
    results.append(result)
    return body if result["ok"] else None


def safe_noop_write(
    client: DeviceClient,
    results: list[dict[str, Any]],
    *,
    name: str,
    read_path: str,
    write_path: str,
    validator: JsonValidator,
    timeout: float,
) -> None:
    backup = get_required_json(client, results, f"{name}.backup", read_path, validator, timeout)
    if backup is None:
        append_verify_result(results, f"{name}.skipped", False, error="backup read failed")
        return

    try:
        safe_post_json(client, results, f"{name}.noop_save", write_path, backup, validator, timeout)
        after = get_required_json(client, results, f"{name}.verify_read", read_path, validator, timeout)
        append_verify_result(
            results,
            f"{name}.noop_verify",
            after is not None,
            details={"read_after_write": after is not None},
            error=None if after is not None else "read after write failed",
        )
    finally:
        restored = safe_post_json(client, results, f"{name}.restore", write_path, backup, validator, timeout)
        append_verify_result(
            results,
            f"{name}.restore_verify",
            restored is not None,
            error=None if restored is not None else "restore POST failed",
        )


def run_config_log_noop(client: DeviceClient, results: list[dict[str, Any]], timeout: float) -> None:
    backup = get_required_json(client, results, "config.backup", "/api/config", validate_config, timeout)
    if backup is None:
        append_verify_result(results, "config.log_level_noop.skipped", False, error="config backup failed")
        return
    logging = backup.get("logging") if isinstance(backup, dict) else None
    level = logging.get("level") if isinstance(logging, dict) else None
    if not isinstance(level, str) or not level:
        append_verify_result(results, "config.log_level_noop.skipped", False, error="config.logging.level missing")
        return
    payload = {"logging": {"level": level}}
    try:
        safe_post_json(client, results, "config.log_level_noop_save", "/api/config", payload, validate_config, timeout)
        after = get_required_json(client, results, "config.log_level_noop_verify_read", "/api/config", validate_config, timeout)
        after_level = after.get("logging", {}).get("level") if isinstance(after, dict) else None
        append_verify_result(
            results,
            "config.log_level_noop_verify",
            after_level == level,
            details={"before": level, "after": after_level},
            error=None if after_level == level else "logging level changed after no-op save",
        )
    finally:
        safe_post_json(client, results, "config.log_level_restore", "/api/config", payload, validate_config, timeout)


def heartbeat_disabled_payload(settings: dict[str, Any]) -> dict[str, Any]:
    slots = settings.get("slots")
    slot_count = len(slots) if isinstance(slots, list) and slots else 4
    return {
        "interval_ms": int(settings.get("interval_ms", 300000) or 300000),
        "slots": [
            {"enabled": False, "name": "", "url": "", "allow_insecure": False}
            for _ in range(slot_count)
        ],
    }


def run_udp_disabled_write(client: DeviceClient, results: list[dict[str, Any]], timeout: float) -> None:
    backup = get_required_json(client, results, "udp.backup", "/api/udp", validate_udp, timeout)
    if not isinstance(backup, dict):
        append_verify_result(results, "udp.disabled.skipped", False, error="UDP backup failed")
        return
    disabled = deepcopy(backup)
    disabled["enabled"] = False
    try:
        safe_post_json(client, results, "udp.disabled_save", "/api/udp", disabled, validate_udp, timeout)
        after = get_required_json(client, results, "udp.disabled_verify_read", "/api/udp", validate_udp, timeout)
        ok = isinstance(after, dict) and after.get("enabled") is False
        append_verify_result(
            results,
            "udp.disabled_verify",
            ok,
            details={"enabled_after": after.get("enabled") if isinstance(after, dict) else None},
            error=None if ok else "UDP was not disabled by safe disabled config",
        )
    finally:
        restored = safe_post_json(client, results, "udp.restore", "/api/udp", backup, validate_udp, timeout)
        append_verify_result(
            results,
            "udp.restore_verify",
            isinstance(restored, dict) and restored.get("enabled") == backup.get("enabled"),
            error="UDP restore did not return expected enabled state"
            if not (isinstance(restored, dict) and restored.get("enabled") == backup.get("enabled"))
            else None,
        )


def run_heartbeat_disabled_write(client: DeviceClient, results: list[dict[str, Any]], timeout: float) -> None:
    backup = get_required_json(client, results, "heartbeat.backup", "/api/heartbeat", validate_heartbeat, timeout)
    if not isinstance(backup, dict):
        append_verify_result(results, "heartbeat.disabled.skipped", False, error="heartbeat backup failed")
        return
    disabled = heartbeat_disabled_payload(backup)
    try:
        safe_post_json(client, results, "heartbeat.disabled_save", "/api/heartbeat", disabled, validate_heartbeat, timeout)
        after = get_required_json(client, results, "heartbeat.disabled_verify_read", "/api/heartbeat", validate_heartbeat, timeout)
        slots = after.get("slots") if isinstance(after, dict) else None
        active_slots = sum(
            1
            for slot in slots or []
            if isinstance(slot, dict) and slot.get("enabled") is True and bool(str(slot.get("url", "")))
        )
        append_verify_result(
            results,
            "heartbeat.disabled_verify",
            active_slots == 0,
            details={"active_slots_after": active_slots},
            error=None if active_slots == 0 else "heartbeat still has enabled slots",
        )
    finally:
        restored = safe_post_json(client, results, "heartbeat.restore", "/api/heartbeat", backup, validate_heartbeat, timeout)
        append_verify_result(
            results,
            "heartbeat.restore_verify",
            isinstance(restored, dict),
            error=None if isinstance(restored, dict) else "heartbeat restore failed",
        )


def run_safe_writes(client: DeviceClient, timeout: float) -> list[dict[str, Any]]:
    results: list[dict[str, Any]] = []
    run_config_log_noop(client, results, timeout)
    safe_noop_write(
        client,
        results,
        name="matrix.settings",
        read_path="/api/matrix/settings",
        write_path="/api/matrix/settings",
        validator=validate_matrix_settings,
        timeout=timeout,
    )
    safe_noop_write(
        client,
        results,
        name="ble.settings",
        read_path="/api/ble/settings",
        write_path="/api/ble/settings",
        validator=validate_ble_settings,
        timeout=timeout,
    )
    safe_noop_write(
        client,
        results,
        name="wifisensing.config",
        read_path="/api/wifisensing/config",
        write_path="/api/wifisensing/config",
        validator=validate_wifisensing_config,
        timeout=timeout,
    )
    safe_noop_write(
        client,
        results,
        name="alarms.rules",
        read_path="/api/alarms/rules",
        write_path="/api/alarms/rules",
        validator=validate_alarm_rules,
        timeout=timeout,
    )
    run_udp_disabled_write(client, results, timeout)
    run_heartbeat_disabled_write(client, results, timeout)
    return results


def run_wss_check(client: DeviceClient, timeout: float) -> dict[str, Any]:
    started = time.perf_counter()
    result = new_result("wss.system_snapshot", "WS", "/ws/system", started)
    if RawWebSocket is None or request_system_status_snapshot is None:
        result["error"] = "verify_system_websocket helpers are unavailable"
        return result
    try:
        with RawWebSocket(client, timeout=timeout) as ws:
            frame = request_system_status_snapshot(ws, timeout)
        result["latency_ms"] = round((time.perf_counter() - started) * 1000, 2)
        result["ok"] = True
        result["details"] = {
            "type": frame.get("type") if isinstance(frame, dict) else None,
            "channel": frame.get("channel") if isinstance(frame, dict) else None,
        }
    except Exception as exc:  # noqa: BLE001
        result["latency_ms"] = round((time.perf_counter() - started) * 1000, 2)
        result["error"] = str(exc)
    return result


def build_http_redirect_probe_url(base_url: str, path: str) -> tuple[str, str]:
    parts = urlsplit(base_url)
    host = parts.hostname or parts.netloc
    if ":" in host and not host.startswith("["):
        host = f"[{host}]"
    clean_path = path if path.startswith("/") else f"/{path}"
    http_url = urlunsplit(("http", host, clean_path, "", ""))
    expected_location = urlunsplit(("https", host, clean_path, "", ""))
    return http_url, expected_location


def run_http_redirect_check(client: DeviceClient, timeout: float) -> dict[str, Any]:
    probe_path = "/system/power?smoke=http_redirect"
    http_url, expected_location = build_http_redirect_probe_url(client.base_url, probe_path)
    started = time.perf_counter()
    result = new_result("http.redirect_to_https", "GET", http_url, started)
    try:
        response = client.session.get(http_url, timeout=timeout, allow_redirects=False)
        location = response.headers.get("Location", "")
        result["latency_ms"] = round((time.perf_counter() - started) * 1000, 2)
        result["status"] = response.status_code
        result["bytes"] = len(response.content)
        result["details"] = {
            "location": location,
            "expected_location": expected_location,
        }
        result["ok"] = response.status_code == 301 and location == expected_location
        if not result["ok"]:
            result["error"] = f"expected 301 Location {expected_location}, got {response.status_code} {location}"
    except Exception as exc:  # noqa: BLE001 - smoke reports transport failures as results.
        result["latency_ms"] = round((time.perf_counter() - started) * 1000, 2)
        result["error"] = str(exc)
    return result


def run_restart_check(client: DeviceClient, timeout: float) -> dict[str, Any]:
    result = raw_http_check(client, "restart.optional", "POST", "/rest/restart", timeout=timeout)
    if not result["ok"]:
        return result

    deadline = time.monotonic() + timeout
    time.sleep(2.0)
    probe_client = DeviceClient(
        base_url=client.base_url,
        username=client.username,
        password=client.password,
        timeout=min(5.0, client.timeout),
        verify=client.verify,
    )
    last_error = ""
    while time.monotonic() < deadline:
        try:
            login_with_retry(probe_client, max_wait_s=10.0)
            response = probe_client.get("/api/system/info")
            if response.status_code == 200:
                result["details"] = {"device_recovered": True}
                return result
            last_error = f"HTTP {response.status_code}"
        except Exception as exc:  # noqa: BLE001
            last_error = str(exc)
        time.sleep(2.0)
    result["ok"] = False
    result["error"] = f"restart request sent but device did not recover before timeout: {last_error}"
    return result


def run_power_sleep_smoke(
    client: DeviceClient,
    results: list[dict[str, Any]],
    *,
    sleep_duration_ms: int,
    recover_timeout_s: float,
    timeout: float,
) -> None:
    backup = get_required_json(
        client,
        results,
        "power.sleep_smoke.backup",
        "/rest/power/config",
        validate_power_config,
        timeout,
    )
    if not isinstance(backup, dict):
        append_verify_result(results, "power.sleep_smoke.skipped", False, error="power config backup failed")
        return

    requested_sleep_ms = max(30000, int(sleep_duration_ms))
    payload = deepcopy(backup)
    payload.update(
        {
            "sleep_enabled": False,
            "wake_timer_enabled": True,
            "wake_button_enabled": True,
            "wake_interval_ms": requested_sleep_ms,
            "timer_wake_awake_ms": max(60000, int(payload.get("timer_wake_awake_ms", 60000) or 60000)),
            "button_wake_awake_ms": max(120000, int(payload.get("button_wake_awake_ms", 120000) or 120000)),
        }
    )

    try:
        saved = safe_post_json(
            client,
            results,
            "power.sleep_smoke.configure",
            "/rest/power/config",
            payload,
            validate_power_config,
            timeout,
        )
        if not isinstance(saved, dict):
            append_verify_result(results, "power.sleep_smoke.skipped", False, error="power config update failed")
            return

        results.append(raw_http_check(client, "power.sleep_smoke.request", "POST", "/rest/sleep", timeout=timeout))
        started = time.perf_counter()
        recover = new_result("power.sleep_smoke.recover", "VERIFY", "/rest/power/status", started)
        probe_client = DeviceClient(
            base_url=client.base_url,
            username=client.username,
            password=client.password,
            timeout=min(5.0, client.timeout),
            verify=client.verify,
        )
        deadline = time.monotonic() + max(recover_timeout_s, requested_sleep_ms / 1000.0 + 30.0)
        last_error = ""
        time.sleep(2.0)
        while time.monotonic() < deadline:
            try:
                login_with_retry(probe_client, max_wait_s=5.0)
                response = probe_client.get("/rest/power/status", timeout=min(5.0, timeout))
                recover["latency_ms"] = round((time.perf_counter() - started) * 1000, 2)
                recover["status"] = response.status_code
                recover["bytes"] = len(response.content)
                if response.status_code == 200:
                    status = response.json()
                    schema_errors = validate_power_status(status)
                    if schema_errors:
                        recover["schema_errors"] = schema_errors
                        recover["error"] = "; ".join(schema_errors)
                        results.append(recover)
                        return
                    recover["ok"] = status.get("wake_reason") == "timer"
                    recover["details"] = {
                        "wake_reason": status.get("wake_reason"),
                        "wake_interval_ms": status.get("wake_interval_ms"),
                        "requested_sleep_ms": requested_sleep_ms,
                    }
                    if not recover["ok"]:
                        recover["error"] = f"expected timer wake, got {status.get('wake_reason')}"
                    results.append(recover)
                    client.set_token(probe_client.token or "")
                    return
                last_error = f"HTTP {response.status_code}"
            except Exception as exc:  # noqa: BLE001
                last_error = str(exc)
            time.sleep(2.0)
        recover["latency_ms"] = round((time.perf_counter() - started) * 1000, 2)
        recover["error"] = f"device did not recover from sleep before timeout: {last_error}"
        results.append(recover)
    finally:
        restored = safe_post_json(
            client,
            results,
            "power.sleep_smoke.restore",
            "/rest/power/config",
            backup,
            validate_power_config,
            timeout,
        )
        append_verify_result(
            results,
            "power.sleep_smoke.restore_verify",
            isinstance(restored, dict) and restored.get("sleep_enabled") == backup.get("sleep_enabled"),
            error=None if isinstance(restored, dict) else "power config restore failed",
        )


def summarize_results(results: list[dict[str, Any]]) -> dict[str, Any]:
    failures = [result for result in results if not result.get("ok")]
    latencies = [float(result.get("latency_ms", 0.0)) for result in results if result.get("latency_ms")]
    return {
        "checks": len(results),
        "passed": len(results) - len(failures),
        "failed": len(failures),
        "max_latency_ms": round(max(latencies), 2) if latencies else 0.0,
        "avg_latency_ms": round(sum(latencies) / len(latencies), 2) if latencies else 0.0,
    }


def markdown_escape(value: Any) -> str:
    return str(value).replace("|", "\\|").replace("\n", " ")


def render_markdown(report: dict[str, Any]) -> str:
    summary = report["summary"]
    lines = [
        "# MatrixHub Device Smoke Report",
        "",
        f"- Target: `{report['target']}`",
        f"- Started: `{report['started_at']}`",
        f"- Finished: `{report['finished_at']}`",
        f"- Result: `{'PASS' if report['ok'] else 'FAIL'}`",
        f"- Checks: `{summary.get('passed', 0)}/{summary.get('checks', 0)}` passed",
        f"- Average latency: `{summary.get('avg_latency_ms', 0.0)} ms`",
        f"- Max latency: `{summary.get('max_latency_ms', 0.0)} ms`",
        "",
        "## Memory",
        "",
        "| Region | Before free | After free | Delta free | Before largest | After largest | Delta largest |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    before = report.get("memory", {}).get("before", {})
    after = report.get("memory", {}).get("after", {})
    delta = report.get("memory", {}).get("delta", {})
    for region in ("default", "internal", "psram"):
        lines.append(
            "| "
            + " | ".join(
                [
                    region,
                    str(before.get(region, {}).get("free", "")),
                    str(after.get(region, {}).get("free", "")),
                    str(delta.get(region, {}).get("free", "")),
                    str(before.get(region, {}).get("largestBlock", "")),
                    str(after.get(region, {}).get("largestBlock", "")),
                    str(delta.get(region, {}).get("largestBlock", "")),
                ]
            )
            + " |"
        )
    lines += [
        "",
        "## Checks",
        "",
        "| Result | Name | Method | Path | Status | Latency ms | Error |",
        "| --- | --- | --- | --- | ---: | ---: | --- |",
    ]
    for result in report["results"]:
        lines.append(
            "| "
            + " | ".join(
                [
                    "PASS" if result.get("ok") else "FAIL",
                    markdown_escape(result.get("name", "")),
                    markdown_escape(result.get("method", "")),
                    f"`{markdown_escape(result.get('path', ''))}`",
                    markdown_escape(result.get("status", "")),
                    markdown_escape(result.get("latency_ms", "")),
                    markdown_escape(result.get("error", "")),
                ]
            )
            + " |"
        )
    return "\n".join(lines) + "\n"


def write_reports(report: dict[str, Any], report_dir: Path, prefix: str) -> dict[str, str]:
    report_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    target_slug = safe_slug(urlsplit(report["target"]).netloc or "device")
    base = f"{safe_slug(prefix)}-{target_slug}-{stamp}"
    json_path = report_dir / f"{base}.json"
    md_path = report_dir / f"{base}.md"
    paths = {"json": str(json_path), "markdown": str(md_path)}
    report_with_paths = dict(report)
    report_with_paths["reports"] = paths
    json_path.write_text(json.dumps(report_with_paths, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    md_path.write_text(render_markdown(report_with_paths), encoding="utf-8")
    return paths


def enforce_https(base_url: str) -> None:
    scheme = urlsplit(base_url).scheme
    if scheme != "https":
        raise DeviceClientError(f"device smoke requires HTTPS, got {scheme or 'missing scheme'}")


def login_with_retry(client: DeviceClient, max_wait_s: float) -> str:
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


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    parser.add_argument("--read-only", action="store_true", help="Run read-only endpoint contract checks.")
    parser.add_argument("--safe-writes", action="store_true", help="Run safe POST checks with backup/restore.")
    parser.add_argument("--wss", action="store_true", help="Also run a /ws/system WSS snapshot check.")
    parser.add_argument("--http-redirect", action="store_true", help="Verify plain HTTP redirects to HTTPS.")
    parser.add_argument("--restart", action="store_true", help="Optionally POST /rest/restart and wait for recovery.")
    parser.add_argument("--power-sleep-smoke", action="store_true", help="POST /rest/sleep, wait for timer wake, and restore power config.")
    parser.add_argument("--power-sleep-duration-ms", type=int, default=30000)
    parser.add_argument("--power-sleep-recover-timeout-sec", type=float, default=120.0)
    parser.add_argument("--wss-timeout-sec", type=float, default=8.0)
    parser.add_argument("--restart-timeout-sec", type=float, default=75.0)
    parser.add_argument("--login-timeout-sec", type=float, default=75.0)
    parser.add_argument("--max-heap-drop-bytes", type=int, default=DEFAULT_HEAP_DROP_LIMIT_BYTES)
    parser.add_argument("--report-dir", default=str(DEFAULT_REPORT_DIR), help="Directory for JSON/Markdown reports.")
    parser.add_argument("--report-prefix", default="device-smoke")
    parser.add_argument("--no-report-files", action="store_true", help="Do not write JSON/Markdown report files.")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)
    if (
        not args.read_only
        and not args.safe_writes
        and not args.wss
        and not args.http_redirect
        and not args.restart
        and not args.power_sleep_smoke
    ):
        args.read_only = True

    client = DeviceClient.from_args(args)
    started_at = now_iso()
    all_results: list[dict[str, Any]] = []
    report_paths: dict[str, str] = {}
    memory_before: dict[str, Any] = {}
    memory_after: dict[str, Any] = {}
    memory_delta: dict[str, Any] = {}

    try:
        enforce_https(client.base_url)
        login_with_retry(client, args.login_timeout_sec)

        auth_result = run_reauth_probe(client, args.timeout)
        all_results.append(auth_result)

        before_result, before_body = http_json_check(
            client,
            "memory.before",
            "GET",
            "/api/diagnostics/heap",
            validator=validate_heap,
            timeout=args.timeout,
        )
        all_results.append(before_result)
        memory_before = extract_heap(before_body) if before_result["ok"] else {}

        if args.read_only:
            read_results, _bodies = run_read_only(client, args.timeout)
            all_results.extend(read_results)

        if args.safe_writes:
            all_results.extend(run_safe_writes(client, args.timeout))

        if args.wss:
            all_results.append(run_wss_check(client, args.wss_timeout_sec))

        if args.http_redirect:
            all_results.append(run_http_redirect_check(client, args.timeout))

        if args.power_sleep_smoke:
            run_power_sleep_smoke(
                client,
                all_results,
                sleep_duration_ms=args.power_sleep_duration_ms,
                recover_timeout_s=args.power_sleep_recover_timeout_sec,
                timeout=args.timeout,
            )

        if args.restart:
            all_results.append(run_restart_check(client, args.restart_timeout_sec))

        after_result, after_body = http_json_check(
            client,
            "memory.after",
            "GET",
            "/api/diagnostics/heap",
            validator=validate_heap,
            timeout=args.timeout,
        )
        all_results.append(after_result)
        memory_after = extract_heap(after_body) if after_result["ok"] else {}
        memory_delta = heap_delta(memory_before, memory_after)

        for region, values in memory_delta.items():
            free_delta = values.get("free")
            if isinstance(free_delta, int) and free_delta < -abs(args.max_heap_drop_bytes):
                append_verify_result(
                    all_results,
                    f"memory.{region}.free_drop_limit",
                    False,
                    details={"delta_free": free_delta, "limit": -abs(args.max_heap_drop_bytes)},
                    error="heap free dropped beyond smoke threshold",
                )

        summary = summarize_results(all_results)
        report = {
            "ok": summary["failed"] == 0,
            "target": client.base_url,
            "https_only": True,
            "jwt_login": True,
            "retries": client.retries,
            "started_at": started_at,
            "finished_at": now_iso(),
            "modes": {
                "read_only": bool(args.read_only),
                "safe_writes": bool(args.safe_writes),
                "wss": bool(args.wss),
                "http_redirect": bool(args.http_redirect),
                "power_sleep_smoke": bool(args.power_sleep_smoke),
                "restart": bool(args.restart),
            },
            "summary": summary,
            "memory": {
                "before": memory_before,
                "after": memory_after,
                "delta": memory_delta,
            },
            "results": all_results,
        }
    except Exception as exc:  # noqa: BLE001
        report = {
            "ok": False,
            "target": client.base_url,
            "https_only": urlsplit(client.base_url).scheme == "https",
            "jwt_login": False,
            "retries": client.retries,
            "started_at": started_at,
            "finished_at": now_iso(),
            "summary": {"checks": len(all_results), "passed": 0, "failed": 1},
            "memory": {
                "before": memory_before,
                "after": memory_after,
                "delta": memory_delta,
            },
            "results": all_results
            + [
                {
                    "name": "device_smoke.fatal",
                    "method": "ERROR",
                    "path": "",
                    "ok": False,
                    "latency_ms": 0.0,
                    "error": str(exc),
                }
            ],
        }

    if not args.no_report_files:
        report_paths = write_reports(report, Path(args.report_dir), args.report_prefix)
        report["reports"] = report_paths

    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        label = "PASS" if report["ok"] else "FAIL"
        summary = report.get("summary", {})
        print(
            f"{label}: device smoke target={report['target']} "
            f"passed={summary.get('passed', 0)}/{summary.get('checks', 0)} "
            f"failed={summary.get('failed', 0)}"
        )
        if report_paths:
            print(f"JSON report: {report_paths['json']}")
            print(f"Markdown report: {report_paths['markdown']}")
        for result in report.get("results", []):
            if not result.get("ok"):
                print(
                    f"FAIL {result.get('name')} {result.get('method')} "
                    f"{result.get('path')}: {result.get('error')}",
                    file=sys.stderr,
                )
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
