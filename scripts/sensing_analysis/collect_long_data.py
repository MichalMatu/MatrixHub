import argparse
import asyncio
import csv
import struct
import time
from pathlib import Path
import sys

import websockets

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, add_common_device_args  # noqa: E402

FIELDNAMES = ["timestamp", "rssi", "variance", "motion"]
DEFAULT_OUTPUT_FILE = Path(__file__).with_name("raw_capture.csv")
CSI_HEADER_BYTES = 13


def parse_args():
    parser = argparse.ArgumentParser(
        description="Collect WiFi CSI WebSocket samples and save them as CSV."
    )
    add_common_device_args(parser)
    parser.add_argument(
        "--duration",
        type=float,
        default=120.0,
        help="Collection duration in seconds.",
    )
    parser.add_argument(
        "--poll-interval",
        type=float,
        default=2.0,
        help=argparse.SUPPRESS,
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT_FILE,
        help="CSV output path.",
    )
    return parser.parse_args()


def connect_ws(uri: str, headers: dict[str, str], ssl_context):
    try:
        return websockets.connect(uri, additional_headers=headers, ssl=ssl_context)
    except TypeError:
        return websockets.connect(uri, extra_headers=headers, ssl=ssl_context)


def iter_new_samples(payload: bytes, seen_timestamps: set[int]):
    offset = 0
    while offset + CSI_HEADER_BYTES <= len(payload):
        timestamp, rssi, data_len, _gain_scaled, motion_score, is_motion = struct.unpack_from(
            "<IbHBfB", payload, offset
        )
        payload_start = offset + CSI_HEADER_BYTES
        payload_end = payload_start + data_len
        if data_len == 0 or payload_end > len(payload):
            break
        offset = payload_end

        if timestamp in seen_timestamps:
            continue

        seen_timestamps.add(timestamp)
        yield {
            "timestamp": timestamp,
            "rssi": rssi,
            "variance": motion_score,
            "motion": is_motion,
        }


def initialize_output(output_file: Path):
    output_file.parent.mkdir(parents=True, exist_ok=True)
    with output_file.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDNAMES)
        writer.writeheader()


def append_rows(output_file: Path, rows):
    with output_file.open("a", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDNAMES)
        writer.writerows(rows)


async def collect_samples(args):
    client = DeviceClient.from_args(args)
    output_file = args.output
    uri = client.ws_url("/ws/csi")

    initialize_output(output_file)
    seen_timestamps = set()
    start_time = time.time()

    async with connect_ws(uri, client.ws_cookie_header(), client.ws_ssl_context()) as websocket:
        print(f"[+] Connected to {uri}; collecting for {args.duration:.1f}s")
        while (time.time() - start_time) < args.duration:
            try:
                message = await asyncio.wait_for(websocket.recv(), timeout=1.0)
            except asyncio.TimeoutError:
                continue
            if not isinstance(message, bytes):
                continue

            new_rows = list(iter_new_samples(message, seen_timestamps))
            if new_rows:
                append_rows(output_file, new_rows)
            elapsed = int(time.time() - start_time)
            print(f"[{elapsed}s] Saved {len(new_rows)} new samples.")

    print(f"[+] Finished. Saved {len(seen_timestamps)} unique samples to {output_file}.")


if __name__ == "__main__":
    try:
        asyncio.run(collect_samples(parse_args()))
    except Exception as exc:
        raise SystemExit(f"Error: {exc}")
