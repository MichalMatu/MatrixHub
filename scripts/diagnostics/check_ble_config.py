#!/usr/bin/env python3
"""Print BLE settings and runtime status."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args, print_json  # noqa: E402


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    args = parser.parse_args(argv)

    client = DeviceClient.from_args(args)
    try:
        data = {
            "settings": client.json("GET", "/api/ble/settings"),
            "status": client.json("GET", "/api/ble/status"),
        }
    except DeviceClientError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    if not args.json:
        print(f"Target: {client.base_url}")
    print_json(data)
    return 0


if __name__ == "__main__":
    sys.exit(main())
