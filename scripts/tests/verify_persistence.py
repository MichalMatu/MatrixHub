#!/usr/bin/env python3
"""Verify BLE scanner settings survive a controlled restart."""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args  # noqa: E402


def normalize_settings(settings: dict) -> dict:
    return {
        "enabled": bool(settings.get("enabled", False)),
        "sensors": [
            {"mac": sensor.get("mac", ""), "alias": sensor.get("alias", "")}
            for sensor in settings.get("sensors", [])
        ],
    }


def wait_for_online(client: DeviceClient, timeout: float) -> bool:
    start = time.monotonic()
    while time.monotonic() - start < timeout:
        try:
            client.login(force=True)
            response = client.get("/api/ble/settings", timeout=3)
            if response.status_code == 200:
                return True
        except Exception:
            pass
        time.sleep(5)
    return False


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    parser.add_argument("--restart-timeout", type=float, default=90.0)
    args = parser.parse_args(argv)

    client = DeviceClient.from_args(args)
    try:
        initial = client.json("GET", "/api/ble/settings")
        baseline = normalize_settings(initial)
        print(f"[BASELINE] {json.dumps(baseline, sort_keys=True)}")

        client.json("POST", "/api/ble/settings", json=baseline)
        print("[RESTART] requesting /rest/restart")
        try:
            client.post("/rest/restart", timeout=2)
        except Exception:
            pass

        if not wait_for_online(client, args.restart_timeout):
            print("ERROR: device did not return online in time", file=sys.stderr)
            return 1

        persisted = normalize_settings(client.json("GET", "/api/ble/settings"))
    except DeviceClientError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    if persisted != baseline:
        print("FAILED: BLE scanner settings drifted", file=sys.stderr)
        print(f"expected={json.dumps(baseline, sort_keys=True)}", file=sys.stderr)
        print(f"actual={json.dumps(persisted, sort_keys=True)}", file=sys.stderr)
        return 1
    print("PASS: BLE scanner settings persisted")
    return 0


if __name__ == "__main__":
    sys.exit(main())
