#!/usr/bin/env python3
"""Smoke-check MatrixHub macro settings, repository, and runtime over HTTPS/JWT."""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args, print_json  # noqa: E402

TEST_SCRIPT_NAME = "codex-macro-smoke.txt"
TEST_SCRIPT_CONTENT = "REM MatrixHub macro smoke\nDELAY 1\n"


def require_mapping(name: str, value: Any) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise DeviceClientError(f"{name} returned non-object JSON")
    return value


def fetch_state(client: DeviceClient) -> dict[str, Any]:
    settings = require_mapping("macro settings", client.json("GET", "/api/macros/settings"))
    status = require_mapping("macro status", client.json("GET", "/api/macros/status"))
    scripts = client.json("GET", "/api/macros")
    if not isinstance(scripts, list):
        raise DeviceClientError("macro list returned non-array JSON")
    return {
        "settings": settings,
        "status": status,
        "scripts": scripts,
    }


def validate_state(state: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    settings = state.get("settings")
    if not isinstance(settings, dict):
        errors.append("state missing object settings")
        settings = {}
    if "enabled" not in settings or not isinstance(settings["enabled"], bool):
        errors.append("/api/macros/settings missing boolean enabled")
    if "boot_script" not in settings or not isinstance(settings["boot_script"], str):
        errors.append("/api/macros/settings missing string boot_script")
    if "boot_delay" not in settings or not isinstance(settings["boot_delay"], int):
        errors.append("/api/macros/settings missing integer boot_delay")

    status = state.get("status")
    if not isinstance(status, dict):
        errors.append("state missing object status")
        status = {}
    for field in ("current_script", "status", "current_line", "uptime_ms", "last_error"):
        if field not in status:
            errors.append(f"/api/macros/status missing {field}")
    if "status" in status and status["status"] not in {"IDLE", "RUNNING", "PAUSED", "ERROR", "COMPLETED"}:
        errors.append(f"/api/macros/status invalid status {status['status']!r}")

    scripts = state.get("scripts")
    if not isinstance(scripts, list):
        errors.append("state missing array scripts")
    else:
        for index, script in enumerate(scripts):
            if not isinstance(script, dict) or not isinstance(script.get("name"), str):
                errors.append(f"/api/macros script[{index}] missing string name")

    return errors


def _content_or_none(client: DeviceClient, name: str) -> str | None:
    response = client.get(f"/api/macros/content?name={name}")
    if response.status_code == 404:
        return None
    if response.status_code != 200:
        raise DeviceClientError(f"GET macro content failed: HTTP {response.status_code} {response.text[:200]}")
    return response.text


def script_list_contains(state: dict[str, Any], name: str) -> bool:
    scripts = state.get("scripts")
    if not isinstance(scripts, list):
        return False
    return any(isinstance(item, dict) and item.get("name") == name for item in scripts)


def exercise_safe_writes(client: DeviceClient, state: dict[str, Any]) -> dict[str, Any]:
    results: dict[str, Any] = {}
    had_previous = script_list_contains(state, TEST_SCRIPT_NAME)
    previous_content = _content_or_none(client, TEST_SCRIPT_NAME) if had_previous else None

    try:
        upload = client.post(
            "/api/macros",
            json={"filename": TEST_SCRIPT_NAME, "content": TEST_SCRIPT_CONTENT},
        )
        results["upload"] = {"http_status": upload.status_code, "ok": upload.status_code == 200}
        if upload.status_code != 200:
            return results

        scripts = client.json("GET", "/api/macros")
        results["listed_after_upload"] = {
            "ok": any(isinstance(item, dict) and item.get("name") == TEST_SCRIPT_NAME for item in scripts),
        }

        if bool(state.get("settings", {}).get("enabled")):
            run = client.post("/api/macros/run", json={"name": TEST_SCRIPT_NAME})
            results["run"] = {"http_status": run.status_code, "ok": run.status_code == 200}
            time.sleep(0.2)
            stop = client.post("/api/macros/stop")
            results["stop"] = {"http_status": stop.status_code, "ok": stop.status_code == 200}
        else:
            results["run"] = {"skipped": "macros_disabled"}
            results["stop"] = {"skipped": "macros_disabled"}
    finally:
        if had_previous:
            restore = client.post(
                "/api/macros",
                json={"filename": TEST_SCRIPT_NAME, "content": previous_content},
            )
            results["restore"] = {"http_status": restore.status_code, "ok": restore.status_code == 200}
        else:
            delete = client.post("/api/macros/delete", json={"name": TEST_SCRIPT_NAME})
            results["delete"] = {"http_status": delete.status_code, "ok": delete.status_code in {200, 404}}

    return results


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    parser.add_argument(
        "--safe-writes",
        action="store_true",
        help="Create/run/stop/delete a short REM+DELAY macro; restores pre-existing test file content.",
    )
    args = parser.parse_args(argv)

    client = DeviceClient.from_args(args)
    try:
        state = fetch_state(client)
        errors = validate_state(state)
        exercise = exercise_safe_writes(client, state) if args.safe_writes else {}
    except DeviceClientError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    exercise_ok = all(item.get("ok", True) for item in exercise.values())
    report = {
        "ok": not errors and exercise_ok,
        "errors": errors,
        "exercise": exercise,
        "state": state,
    }
    print_json(report)
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
