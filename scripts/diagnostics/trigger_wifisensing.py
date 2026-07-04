#!/usr/bin/env python3
"""Enable/disable WiFi sensing and optionally trigger hygiene sleep."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args, print_json  # noqa: E402


def get_status(client: DeviceClient) -> dict | None:
    try:
        response = client.get("/api/wifisensing/status")
    except Exception as exc:
        return {"available": False, "error": str(exc)}

    if response.status_code != 200:
        return {"available": False, "http_status": response.status_code}
    try:
        status = response.json()
    except ValueError:
        return {"available": False, "http_status": response.status_code, "error": "non_json"}
    if isinstance(status, dict):
        status["available"] = True
        return status
    return {"available": False, "http_status": response.status_code, "error": "unexpected_json"}


def request_hygiene_sleep(client: DeviceClient) -> None:
    try:
        client.post("/rest/power/hygieneSleep", timeout=2)
    except Exception:
        pass


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--enable", action="store_true")
    mode.add_argument("--disable", action="store_true")
    parser.add_argument("--no-restart", action="store_true")
    args = parser.parse_args(argv)

    client = DeviceClient.from_args(args)
    try:
        status_before = get_status(client)
        current = client.json("GET", "/api/wifisensing/config")
        current["enabled"] = bool(args.enable)
        result = client.json("POST", "/api/wifisensing/config", json=current)
        status_after = get_status(client)
        if not args.no_restart:
            request_hygiene_sleep(client)
    except DeviceClientError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    print_json({
        "wifisensing": result,
        "status_before": status_before,
        "status_after": status_after,
        "restart_requested": not args.no_restart,
    })
    return 0


if __name__ == "__main__":
    sys.exit(main())
