#!/usr/bin/env python3
"""Short CSI monitor over WSS with JWT cookie auth."""

from __future__ import annotations

import argparse
import asyncio
import struct
import sys
import time
from pathlib import Path

import websockets

sys.path.insert(0, str(Path(__file__).resolve().parent))
from device_client import DeviceClient, add_common_device_args  # noqa: E402

CSI_HEADER_BYTES = 13


def connect_ws(uri: str, headers: dict[str, str], ssl_context):
    try:
        return websockets.connect(uri, additional_headers=headers, ssl=ssl_context)
    except TypeError:
        return websockets.connect(uri, extra_headers=headers, ssl=ssl_context)


def iter_csi_records(data: bytes):
    offset = 0
    while offset + CSI_HEADER_BYTES <= len(data):
        timestamp, rssi, data_len, gain_raw, motion_score, is_motion = struct.unpack_from(
            "<IbHBfB", data, offset
        )
        payload_start = offset + CSI_HEADER_BYTES
        payload_end = payload_start + data_len
        if data_len == 0 or payload_end > len(data):
            break
        yield timestamp, rssi, data_len, gain_raw / 10.0, motion_score, bool(is_motion)
        offset = payload_end


async def monitor(client: DeviceClient, duration: float) -> int:
    uri = client.ws_url("/ws/csi")
    count = 0
    async with connect_ws(uri, client.ws_cookie_header(), client.ws_ssl_context()) as ws:
        print(f"Connected to {uri}")
        print(f"{'TS':<10} {'RSSI':<6} {'GAIN':<6} {'LEN':<6} {'MOTION':<7} {'SCORE':<8}")
        print("-" * 54)
        start = time.time()
        while time.time() - start < duration:
            try:
                data = await asyncio.wait_for(ws.recv(), timeout=1.0)
            except asyncio.TimeoutError:
                continue
            if not isinstance(data, bytes):
                continue
            for ts, rssi, dlen, gain, score, is_motion in iter_csi_records(data):
                print(f"{ts:<10} {rssi:<6} {gain:<6.1f} {dlen:<6} {str(is_motion):<7} {score:<8.3f}")
                count += 1
    return 0 if count else 1


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    parser.add_argument("--duration", type=float, default=5.0)
    args = parser.parse_args(argv)
    try:
        return asyncio.run(monitor(DeviceClient.from_args(args), max(0.1, args.duration)))
    except KeyboardInterrupt:
        print("\nStopped.")
        return 0
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
