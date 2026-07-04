#!/usr/bin/env python3
"""Collect a short CSI stream sample and print wire-format statistics."""

from __future__ import annotations

import argparse
import asyncio
import struct
import sys
import time
from pathlib import Path

try:
    import websockets
except ImportError:
    print("Install dependency: pip install websockets", file=sys.stderr)
    sys.exit(1)

sys.path.insert(0, str(Path(__file__).resolve().parent))
from device_client import DeviceClient, add_common_device_args  # noqa: E402


def connect_ws(uri: str, headers: dict[str, str], ssl_context):
    try:
        return websockets.connect(uri, additional_headers=headers, ssl=ssl_context)
    except TypeError:
        return websockets.connect(uri, extra_headers=headers, ssl=ssl_context)


async def analyze_csi(client: DeviceClient, duration: float) -> int:
    uri = client.ws_url("/ws/csi")
    packet_count = 0
    total_bytes = 0
    min_amp = float("inf")
    max_amp = float("-inf")
    avg_amp_sum = 0.0
    avg_amp_count = 0
    last_motion_score = 0.0
    last_is_motion = 0

    async with connect_ws(uri, client.ws_cookie_header(), client.ws_ssl_context()) as websocket:
        print(f"Connected to {uri}; collecting for {duration:.1f}s")
        start_time = time.time()
        while time.time() - start_time < duration:
            try:
                message = await asyncio.wait_for(websocket.recv(), timeout=1.0)
            except asyncio.TimeoutError:
                continue
            if not isinstance(message, bytes):
                continue
            total_bytes += len(message)
            packet_count += 1
            offset = 0
            header_size = 13
            while offset + header_size <= len(message):
                ts, rssi, data_len, gain_scaled, motion_score, is_motion = struct.unpack(
                    "<IbHBfB", message[offset : offset + header_size]
                )
                (ts, rssi, gain_scaled)  # keep unpack names visible for future debugging
                payload_start = offset + header_size
                payload_end = payload_start + data_len
                if payload_end > len(message):
                    break
                payload = message[payload_start:payload_end]
                if payload:
                    values = struct.unpack(f"{len(payload)}b", payload)
                    min_amp = min(min_amp, min(values))
                    max_amp = max(max_amp, max(values))
                    avg_amp_sum += sum(map(abs, values)) / len(values)
                    avg_amp_count += 1
                last_motion_score = motion_score
                last_is_motion = is_motion
                offset = payload_end

    elapsed = max(0.001, duration)
    print("-" * 40)
    print(f"Packets received: {packet_count}")
    print(f"Rate: {packet_count / elapsed:.2f} FPS")
    print(f"Bandwidth: {(total_bytes * 8) / 1000 / elapsed:.2f} kbps")
    if packet_count:
        print(f"Avg packet size: {total_bytes / packet_count:.1f} bytes")
    if avg_amp_count:
        print(
            f"Raw amplitudes: min={min_amp}, max={max_amp}, "
            f"avgMagnitude={avg_amp_sum / avg_amp_count:.2f}"
        )
    print(f"Motion score: {last_motion_score:.4f} is_motion={last_is_motion}")
    print("-" * 40)
    return 0 if packet_count else 1


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    parser.add_argument("--duration", type=float, default=5.0)
    args = parser.parse_args(argv)
    client = DeviceClient.from_args(args)
    try:
        return asyncio.run(analyze_csi(client, max(0.1, args.duration)))
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
