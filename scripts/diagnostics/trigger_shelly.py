#!/usr/bin/env python3
"""Add a diagnostic Shelly device and optionally trigger hygiene sleep."""

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
    parser.add_argument("--id", default="shelly-fake-01")
    parser.add_argument("--name", default="Diagnostics Shelly")
    parser.add_argument("--ip", default="192.168.0.20")
    parser.add_argument("--relay-index", type=int, default=0)
    parser.add_argument("--no-restart", action="store_true")
    args = parser.parse_args(argv)

    payload = {
        "id": args.id,
        "name": args.name,
        "ip": args.ip,
        "enabled": True,
        "relay_index": args.relay_index,
    }
    client = DeviceClient.from_args(args)
    try:
        result = client.json("POST", "/api/shelly/devices", json=payload)
        if not args.no_restart:
            request_hygiene_sleep(client)
    except DeviceClientError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    print_json({"shelly": result, "restart_requested": not args.no_restart})
    return 0


if __name__ == "__main__":
    sys.exit(main())
