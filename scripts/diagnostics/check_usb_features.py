#!/usr/bin/env python3
"""Smoke-check USB HID and USB terminal feature state over HTTPS/JWT."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args, print_json  # noqa: E402


def require_mapping(name: str, value: Any) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise DeviceClientError(f"{name} returned non-object JSON")
    return value


def fetch_state(client: DeviceClient) -> dict[str, Any]:
    keyboard = require_mapping("keyboard config", client.json("GET", "/api/keyboard/config"))
    airmouse = require_mapping("airmouse status", client.json("GET", "/api/airmouse/status"))
    usb_terminal = require_mapping("usb terminal config", client.json("GET", "/api/usbterminal/config"))

    diagnostics_features: dict[str, Any] | None = None
    response = client.get("/api/diagnostics/features")
    if response.status_code == 200:
        try:
            payload = response.json()
        except ValueError:
            payload = None
        if isinstance(payload, dict):
            diagnostics_features = payload

    return {
        "keyboard": keyboard,
        "airmouse": airmouse,
        "usb_terminal": usb_terminal,
        "diagnostics_features": diagnostics_features,
    }


def validate_state(state: dict[str, Any]) -> list[str]:
    errors: list[str] = []

    def section(name: str) -> dict[str, Any]:
        value = state.get(name)
        if isinstance(value, dict):
            return value
        errors.append(f"state missing object {name}")
        return {}

    keyboard = section("keyboard")
    if "enabled" not in keyboard or not isinstance(keyboard["enabled"], bool):
        errors.append("/api/keyboard/config missing boolean enabled")

    airmouse = section("airmouse")
    for field in ("movement_enabled", "click_enabled", "running", "calibrating", "jiggler"):
        if field not in airmouse:
            errors.append(f"/api/airmouse/status missing {field}")
    if "jiggler" in airmouse and not isinstance(airmouse["jiggler"], dict):
        errors.append("/api/airmouse/status jiggler is not an object")

    usb_terminal = section("usb_terminal")
    if "enabled" not in usb_terminal or not isinstance(usb_terminal["enabled"], bool):
        errors.append("/api/usbterminal/config missing boolean enabled")
    if "idle_timeout_ms" not in usb_terminal or not isinstance(usb_terminal["idle_timeout_ms"], int):
        errors.append("/api/usbterminal/config missing integer idle_timeout_ms")
    if "target_port" not in usb_terminal or not isinstance(usb_terminal["target_port"], str):
        errors.append("/api/usbterminal/config missing string target_port")

    return errors


def exercise_safe_actions(client: DeviceClient, state: dict[str, Any]) -> dict[str, Any]:
    """Run opt-in actions only when the target feature is already available."""
    results: dict[str, Any] = {}

    keyboard_enabled = bool(state["keyboard"].get("enabled"))
    if keyboard_enabled:
        response = client.post("/api/keyboard/press", json={"key": 240})
        results["keyboard_press_f13"] = {
            "http_status": response.status_code,
            "ok": response.status_code == 200,
        }
    else:
        results["keyboard_press_f13"] = {"skipped": "keyboard_disabled"}

    airmouse_running = bool(state["airmouse"].get("running"))
    if airmouse_running:
        response = client.post("/api/airmouse/calibrate")
        results["airmouse_calibrate"] = {
            "http_status": response.status_code,
            "ok": response.status_code in {200, 429},
        }
    else:
        results["airmouse_calibrate"] = {"skipped": "airmouse_not_running"}

    return results


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    parser.add_argument(
        "--exercise-hid",
        action="store_true",
        help="Opt in to safe HID actions when keyboard/AirMouse are already enabled.",
    )
    args = parser.parse_args(argv)

    client = DeviceClient.from_args(args)
    try:
        state = fetch_state(client)
        errors = validate_state(state)
        exercise = exercise_safe_actions(client, state) if args.exercise_hid else {}
    except DeviceClientError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    report = {
        "ok": not errors and all(item.get("ok", True) for item in exercise.values()),
        "errors": errors,
        "exercise": exercise,
        "state": state,
    }
    print_json(report)
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
