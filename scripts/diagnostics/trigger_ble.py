#!/usr/bin/env python3
"""Set scanner-only BLE mode and optionally trigger hygiene sleep."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args, print_json  # noqa: E402


def request_hygiene_sleep(client: DeviceClient) -> None:
    try:
        client.post("/rest/power/hygieneSleep", timeout=2)
    except Exception:
        pass


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    parser.add_argument("mode", choices=("enabled", "disabled", "scanner", "advertising", "full"))
    parser.add_argument("--no-restart", action="store_true")
    args = parser.parse_args(argv)

    payload = {"enabled": args.mode != "disabled"}
    client = DeviceClient.from_args(args)
    try:
        result = client.json("POST", "/api/ble/settings", json=payload)
        if not args.no_restart:
            request_hygiene_sleep(client)
    except DeviceClientError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    print_json({"ble": result, "restart_requested": not args.no_restart})
    return 0


if __name__ == "__main__":
    sys.exit(main())
