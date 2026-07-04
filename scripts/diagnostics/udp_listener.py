#!/usr/bin/env python3
"""Receive MatrixHub UDP packets with a bounded timeout."""

from __future__ import annotations

import argparse
import json
import socket
import sys
import time
from dataclasses import dataclass
from typing import Any, Callable


@dataclass
class UdpPacket:
    source: str
    source_port: int
    payload: str
    bytes: int


def receive_udp_packets(
    *,
    host: str = "0.0.0.0",
    port: int = 8094,
    timeout_s: float = 60.0,
    count: int = 1,
    buffer_size: int = 4096,
    ready_callback: Callable[[], None] | None = None,
) -> list[UdpPacket]:
    packets: list[UdpPacket] = []
    deadline = time.monotonic() + timeout_s

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind((host, port))
        if ready_callback:
            ready_callback()

        while len(packets) < count:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            sock.settimeout(remaining)
            try:
                data, addr = sock.recvfrom(buffer_size)
            except TimeoutError:
                break
            except socket.timeout:
                break

            try:
                payload = data.decode("utf-8")
            except UnicodeDecodeError:
                payload = data.hex()

            packets.append(
                UdpPacket(
                    source=addr[0],
                    source_port=addr[1],
                    payload=payload,
                    bytes=len(data),
                )
            )

    return packets


def packet_to_dict(packet: UdpPacket) -> dict[str, Any]:
    return {
        "source": packet.source,
        "source_port": packet.source_port,
        "payload": packet.payload,
        "bytes": packet.bytes,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="0.0.0.0", help="Local bind address. Default: 0.0.0.0.")
    parser.add_argument("--port", type=int, default=8094, help="Local UDP port. Default: 8094.")
    parser.add_argument("--timeout", type=float, default=60.0, help="Receive timeout in seconds.")
    parser.add_argument("--count", type=int, default=1, help="Packets required before success.")
    parser.add_argument("--json", action="store_true", help="Emit JSON summary.")
    args = parser.parse_args(argv)

    if args.count <= 0:
        print("ERROR: --count must be positive", file=sys.stderr)
        return 2
    if not (1 <= args.port <= 65535):
        print("ERROR: --port must be in range 1-65535", file=sys.stderr)
        return 2

    try:
        packets = receive_udp_packets(
            host=args.host,
            port=args.port,
            timeout_s=args.timeout,
            count=args.count,
        )
    except OSError as exc:
        print(f"ERROR: listener failed: {exc}", file=sys.stderr)
        return 1

    report = {
        "ok": len(packets) >= args.count,
        "received": len(packets),
        "required": args.count,
        "host": args.host,
        "port": args.port,
        "timeout_s": args.timeout,
        "packets": [packet_to_dict(packet) for packet in packets],
    }

    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print(f"Listening result: {len(packets)}/{args.count} packet(s) on {args.host}:{args.port}")
        for packet in packets:
            print(f"[{packet.source}:{packet.source_port}] {packet.payload}")

    return 0 if report["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
