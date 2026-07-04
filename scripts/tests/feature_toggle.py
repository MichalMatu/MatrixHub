#!/usr/bin/env python3
"""Toggle selected feature settings off/on through the real HTTPS API."""

from __future__ import annotations

import argparse
import copy
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args  # noqa: E402


FEATURES = {
    "ble": ("/api/ble/settings", "enabled"),
    "wifisensing": ("/api/wifisensing/config", "enabled"),
}


def toggle_feature(client: DeviceClient, name: str, delay: float) -> bool:
    endpoint, key = FEATURES[name]
    original = client.json("GET", endpoint)
    if key not in original:
        raise DeviceClientError(f"{endpoint} response has no {key!r} field")

    first = copy.deepcopy(original)
    first[key] = not bool(original[key])
    client.json("POST", endpoint, json=first)
    time.sleep(delay)

    restored = copy.deepcopy(original)
    client.json("POST", endpoint, json=restored)
    time.sleep(delay)

    final = client.json("GET", endpoint)
    if bool(final.get(key)) != bool(original[key]):
        raise DeviceClientError(f"{name} did not restore {key}: {final.get(key)} != {original[key]}")
    return True


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    parser.add_argument(
        "--feature",
        choices=tuple(FEATURES) + ("all",),
        default="all",
        help="Feature to toggle. Default: all.",
    )
    parser.add_argument("--delay", type=float, default=2.0, help="Delay after each update.")
    args = parser.parse_args(argv)

    client = DeviceClient.from_args(args)
    selected = list(FEATURES) if args.feature == "all" else [args.feature]
    failures = 0
    for feature in selected:
        try:
            print(f"[TOGGLE] {feature} via {client.base_url}")
            toggle_feature(client, feature, max(0.0, args.delay))
            print(f"[PASS] {feature}")
        except Exception as exc:
            failures += 1
            print(f"[FAIL] {feature}: {exc}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
