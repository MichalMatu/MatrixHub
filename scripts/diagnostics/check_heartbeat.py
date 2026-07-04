#!/usr/bin/env python3
"""Check heartbeat settings and safe test paths over HTTPS/JWT."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args, print_json  # noqa: E402


MAX_SLOTS = 4


def disabled_slots(count: int = MAX_SLOTS) -> list[dict[str, Any]]:
    return [
        {"enabled": False, "name": "", "url": "", "allow_insecure": False}
        for _ in range(count)
    ]


def require_mapping(name: str, value: Any) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise DeviceClientError(f"{name} returned non-object JSON")
    return value


def normalize_slot(slot: Any) -> dict[str, Any]:
    if not isinstance(slot, dict):
        return {"enabled": False, "name": "", "url": "", "allow_insecure": False}
    return {
        "enabled": bool(slot.get("enabled", False)),
        "name": str(slot.get("name", "")),
        "url": str(slot.get("url", "")),
        "allow_insecure": bool(slot.get("allow_insecure", False)),
    }


def heartbeat_payload_for_restore(settings: dict[str, Any]) -> dict[str, Any]:
    slots = [normalize_slot(slot) for slot in settings.get("slots", [])[:MAX_SLOTS]]
    while len(slots) < MAX_SLOTS:
        slots.append({"enabled": False, "name": "", "url": "", "allow_insecure": False})
    return {
        "interval_ms": int(settings.get("interval_ms", 300000) or 300000),
        "slots": slots,
    }


def active_slot_count(settings: dict[str, Any]) -> int:
    slots = settings.get("slots")
    if not isinstance(slots, list):
        return 0
    return sum(
        1
        for slot in slots
        if isinstance(slot, dict) and bool(slot.get("enabled")) and bool(str(slot.get("url", "")))
    )


def validate_settings(settings: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    if not isinstance(settings.get("interval_ms"), int) or int(settings.get("interval_ms", 0)) <= 0:
        errors.append("/api/heartbeat missing positive interval_ms")
    slots = settings.get("slots")
    if not isinstance(slots, list):
        errors.append("/api/heartbeat missing slots array")
        return errors
    if len(slots) != MAX_SLOTS:
        errors.append(f"/api/heartbeat expected {MAX_SLOTS} slots, got {len(slots)}")
    for index, slot in enumerate(slots[:MAX_SLOTS]):
        if not isinstance(slot, dict):
            errors.append(f"/api/heartbeat slot[{index}] is not an object")
            continue
        if not isinstance(slot.get("enabled"), bool):
            errors.append(f"/api/heartbeat slot[{index}] missing boolean enabled")
        if not isinstance(slot.get("name"), str):
            errors.append(f"/api/heartbeat slot[{index}] missing string name")
        if not isinstance(slot.get("url"), str):
            errors.append(f"/api/heartbeat slot[{index}] missing string url")
        if not isinstance(slot.get("allow_insecure"), bool):
            errors.append(f"/api/heartbeat slot[{index}] missing boolean allow_insecure")
        if slot.get("enabled") is True and not str(slot.get("url", "")).strip():
            errors.append(f"/api/heartbeat slot[{index}] enabled but url is empty")
    return errors


def post_heartbeat_settings(client: DeviceClient, payload: dict[str, Any]) -> dict[str, Any]:
    return require_mapping("heartbeat settings update", client.json("POST", "/api/heartbeat", json=payload))


def safe_disabled_test(client: DeviceClient, current: dict[str, Any]) -> dict[str, Any]:
    results: dict[str, Any] = {}
    restore_payload = heartbeat_payload_for_restore(current)
    try:
        disabled = post_heartbeat_settings(
            client,
            {"interval_ms": restore_payload["interval_ms"], "slots": disabled_slots()},
        )
        results["disable"] = {"ok": active_slot_count(disabled) == 0}

        response = client.post("/api/heartbeat/test")
        try:
            body: Any = response.json()
        except ValueError:
            body = response.text[:200]
        status = body.get("status") if isinstance(body, dict) else None
        results["test_no_slots"] = {
            "http_status": response.status_code,
            "ok": response.status_code == 400 and status in {None, "no_enabled_slots"},
            "body": body,
        }
    finally:
        try:
            restored = post_heartbeat_settings(client, restore_payload)
            results["restore"] = {
                "ok": heartbeat_payload_for_restore(restored) == restore_payload,
                "settings": restored,
            }
        except Exception as exc:
            results["restore"] = {"ok": False, "error": str(exc)}
    return results


def external_ping_test(client: DeviceClient, settings: dict[str, Any]) -> dict[str, Any]:
    response = client.post("/api/heartbeat/test")
    try:
        body: Any = response.json()
    except ValueError:
        body = response.text[:200]
    return {
        "http_status": response.status_code,
        "ok": response.status_code == 200,
        "active_slots": active_slot_count(settings),
        "body": body,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--enable", action="store_true", help="Enable a single diagnostic slot.")
    mode.add_argument("--disable", action="store_true", help="Disable all heartbeat slots.")
    parser.add_argument("--url", default="https://example.com/", help="URL for --enable.")
    parser.add_argument("--interval-ms", type=int, default=60000)
    parser.add_argument(
        "--safe-disabled-test",
        action="store_true",
        help="Temporarily disable all slots, verify /api/heartbeat/test reports no slots, then restore.",
    )
    parser.add_argument(
        "--allow-external-test",
        action="store_true",
        help="Call /api/heartbeat/test against the currently enabled URLs.",
    )
    args = parser.parse_args(argv)

    client = DeviceClient.from_args(args)
    try:
        current = require_mapping("heartbeat settings", client.json("GET", "/api/heartbeat"))
        errors = validate_settings(current)
        exercise: dict[str, Any] = {}

        if args.enable or args.disable:
            payload = {
                "interval_ms": max(1000, args.interval_ms),
                "slots": disabled_slots(),
            }
            if args.enable:
                payload["slots"][0] = {
                    "enabled": True,
                    "name": "Diagnostics",
                    "url": args.url,
                    "allow_insecure": False,
                }
            current = post_heartbeat_settings(client, payload)
            errors = validate_settings(current)

        if args.safe_disabled_test:
            exercise["safe_disabled_test"] = safe_disabled_test(client, current)
            current = require_mapping("heartbeat settings", client.json("GET", "/api/heartbeat"))
            errors = validate_settings(current)

        if args.allow_external_test:
            exercise["external_ping_test"] = external_ping_test(client, current)
    except DeviceClientError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    exercise_ok = all(
        isinstance(item, dict)
        and all(not isinstance(value, dict) or value.get("ok", True) for value in item.values())
        for item in exercise.values()
    )
    report = {
        "ok": not errors and exercise_ok,
        "errors": errors,
        "exercise": exercise,
        "settings": current,
    }
    if not args.json:
        print(f"Target: {client.base_url}")
    print_json(report)
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
