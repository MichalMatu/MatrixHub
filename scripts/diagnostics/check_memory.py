#!/usr/bin/env python3
"""Print runtime memory summary from /api/system/info."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args, print_json  # noqa: E402


MEMORY_KEYS = (
    "uptime",
    "free_heap",
    "min_free_heap",
    "max_alloc_heap",
    "total_heap",
    "used_heap",
    "free_psram",
    "used_psram",
    "psram_size",
    "core_temp",
)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    args = parser.parse_args(argv)

    client = DeviceClient.from_args(args)
    try:
        info = client.json("GET", "/api/system/info")
    except DeviceClientError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    summary = {key: info.get(key) for key in MEMORY_KEYS if key in info}
    if args.json:
        print_json(summary)
    else:
        print(f"Target: {client.base_url}")
        for key, value in summary.items():
            print(f"{key}: {value}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
