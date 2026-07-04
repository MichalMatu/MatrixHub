#!/usr/bin/env python3
"""CSI WebSocket client for MatrixHub over HTTPS/WSS."""

from __future__ import annotations

import argparse
import asyncio
import struct
import sys
import time
from pathlib import Path

import websockets

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from device_client import DeviceClient, add_common_device_args  # noqa: E402

CSI_HEADER_BYTES = 13


def fetch_status(client: DeviceClient) -> dict | None:
    response = client.get("/api/wifisensing/status")
    if response.status_code != 200:
        return None
    try:
        status = response.json()
    except ValueError:
        return None
    return status if isinstance(status, dict) else None


def print_csi_status(label: str, status: dict | None) -> None:
    csi = status.get("csi") if isinstance(status, dict) else None
    if not isinstance(csi, dict):
        return
    print(
        f"{label}: clients={csi.get('ws_client_count')} queue={csi.get('queue_depth')}/"
        f"{csi.get('queue_capacity')} drops={csi.get('queue_drops_total')} "
        f"pps={csi.get('packets_per_sec')} bps={csi.get('batches_per_sec')} "
        f"calibration={csi.get('calibration_state')}:{csi.get('calibration_count')}/"
        f"{csi.get('calibration_target')}"
    )


def connect_ws(uri: str, headers: dict[str, str], ssl_context):
    try:
        return websockets.connect(uri, additional_headers=headers, ssl=ssl_context)
    except TypeError:
        return websockets.connect(uri, extra_headers=headers, ssl=ssl_context)


def iter_csi_records(data: bytes):
    offset = 0
    while offset + CSI_HEADER_BYTES <= len(data):
        ts, rssi, data_len, gain_raw, motion_score, is_motion = struct.unpack_from(
            "<IbHBfB", data, offset
        )
        payload_start = offset + CSI_HEADER_BYTES
        payload_end = payload_start + data_len
        if data_len == 0 or payload_end > len(data):
            break
        yield ts, rssi, data_len, gain_raw / 10.0, motion_score, bool(is_motion), data[payload_start:payload_end]
        offset = payload_end


async def csi_client(client: DeviceClient, duration: float | None) -> int:
    uri = client.ws_url("/ws/csi")
    headers = client.ws_cookie_header()
    print_csi_status("Status before", fetch_status(client))
    print(f"Connecting to {uri}...")
    start_time = time.time()
    message_count = 0
    packet_count = 0
    async with connect_ws(uri, headers, client.ws_ssl_context()) as websocket:
        print("Connected. Waiting for CSI frames...")
        while duration is None or time.time() - start_time < duration:
            receive_timeout = 1.0
            if duration is not None:
                remaining = duration - (time.time() - start_time)
                if remaining <= 0:
                    break
                receive_timeout = min(receive_timeout, remaining)

            try:
                data = await asyncio.wait_for(websocket.recv(), timeout=receive_timeout)
            except asyncio.TimeoutError:
                continue
            message_count += 1
            if not isinstance(data, bytes):
                print(f"Invalid frame size: {len(data) if hasattr(data, '__len__') else 'n/a'}")
                continue

            for ts, rssi, data_len, gain, motion_score, is_motion, payload in iter_csi_records(data):
                packet_count += 1
                if packet_count % 10 != 0:
                    continue
                fps = packet_count / max(0.001, time.time() - start_time)
                print(
                    f"Packet #{packet_count} | Messages: {message_count} | FPS: {fps:.1f} | TS: {ts} | "
                    f"RSSI: {rssi}dBm | Gain: {gain:.1f} | Len: {data_len} | "
                    f"Payload: {len(payload)} bytes | Motion: {is_motion} | Score: {motion_score:.3f}"
                )
    print(f"Received {packet_count} CSI packet(s) in {message_count} WebSocket message(s)")
    print_csi_status("Status after", fetch_status(client))
    return 0 if packet_count > 0 else 1


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    parser.add_argument("--duration", type=float, default=None, help="Seconds to run. Default: until Ctrl+C.")
    args = parser.parse_args(argv)
    client = DeviceClient.from_args(args)
    try:
        return asyncio.run(csi_client(client, args.duration))
    except KeyboardInterrupt:
        print("Exiting...")
        return 0
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
