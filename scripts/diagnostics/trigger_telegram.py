#!/usr/bin/env python3
"""Enable/disable Telegram notification flag and optionally trigger hygiene sleep."""

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
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--enable", action="store_true")
    mode.add_argument("--disable", action="store_true")
    parser.add_argument("--commands", action="store_true", help="Also enable Telegram commands.")
    parser.add_argument("--no-restart", action="store_true")
    args = parser.parse_args(argv)

    client = DeviceClient.from_args(args)
    payload = {"telegram_enabled": bool(args.enable)}
    if args.enable and args.commands:
        payload["commands_enabled"] = True
    try:
        result = client.json("POST", "/api/notifications/settings", json=payload)
        if not args.no_restart:
            request_hygiene_sleep(client)
    except DeviceClientError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    print_json({"notifications": result, "restart_requested": not args.no_restart})
    return 0


if __name__ == "__main__":
    sys.exit(main())
