#!/usr/bin/env python3
"""Print FreeRTOS task diagnostics from /api/system/tasks."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args, print_json  # noqa: E402


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    parser.add_argument("--details", action="store_true", default=True, help="Request detailed task fields.")
    args = parser.parse_args(argv)

    path = "/api/system/tasks?details=1" if args.details else "/api/system/tasks"
    client = DeviceClient.from_args(args)
    try:
        tasks = client.json("GET", path)
    except DeviceClientError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    if args.json:
        print_json(tasks)
    else:
        print(f"Target: {client.base_url}")
        print_json(tasks)
    return 0


if __name__ == "__main__":
    sys.exit(main())
