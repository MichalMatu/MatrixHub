#!/usr/bin/env python3
"""Verify /ws/system snapshots, cleanup, and runtime diagnostics over HTTPS/JWT."""

from __future__ import annotations

import argparse
import base64
import json
import os
import socket
import ssl
import struct
import sys
import time
from pathlib import Path
from typing import Any, Callable
from urllib.parse import urlsplit, urlunsplit

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args  # noqa: E402


HTTP_COUNTER_FIELDS = (
    "wsActiveClients",
    "wsPeakClients",
    "wsOpens",
    "wsCloses",
    "wsForcedRemovals",
    "wsQueueDrops",
)


class WebSocketError(RuntimeError):
    """Raised when the raw WebSocket probe cannot complete."""


def nested(data: dict[str, Any], path: tuple[str, ...], default: Any = None) -> Any:
    current: Any = data
    for key in path:
        if not isinstance(current, dict):
            return default
        current = current.get(key)
    return current if current is not None else default


def http_summary(client: DeviceClient) -> dict[str, int]:
    summary = client.json("GET", "/api/diagnostics/summary")
    http = summary.get("http")
    if not isinstance(http, dict):
        raise DeviceClientError("/api/diagnostics/summary missing http object")
    missing = [field for field in HTTP_COUNTER_FIELDS if field not in http]
    if missing:
        raise DeviceClientError(
            "/api/diagnostics/summary missing WS diagnostic fields: " + ", ".join(missing)
        )
    result: dict[str, int] = {}
    for field in HTTP_COUNTER_FIELDS:
        value = http.get(field)
        if not isinstance(value, int):
            raise DeviceClientError(f"http.{field} is {type(value).__name__}, expected int")
        result[field] = value
    return result


def recv_exact(sock: socket.socket | ssl.SSLSocket, length: int) -> bytes:
    chunks: list[bytes] = []
    remaining = length
    while remaining > 0:
        chunk = sock.recv(remaining)
        if not chunk:
            raise WebSocketError("socket closed while reading frame")
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


class RawWebSocket:
    def __init__(self, client: DeviceClient, path: str = "/ws/system", timeout: float = 5.0):
        self.client = client
        self.path = path
        self.timeout = timeout
        self.raw: socket.socket | None = None
        self.sock: socket.socket | ssl.SSLSocket | None = None

    def __enter__(self) -> "RawWebSocket":
        self.connect()
        return self

    def __exit__(self, exc_type: object, exc: object, tb: object) -> None:
        self.close()

    def connect(self) -> None:
        uri = self.client.ws_url(self.path)
        parts = urlsplit(uri)
        host = parts.hostname
        if not host:
            raise WebSocketError(f"invalid websocket URL: {uri}")
        port = parts.port or (443 if parts.scheme == "wss" else 80)
        path = urlunsplit(("", "", parts.path or "/", parts.query, ""))
        key = base64.b64encode(os.urandom(16)).decode("ascii")
        headers = [
            f"GET {path} HTTP/1.1",
            f"Host: {parts.netloc}",
            "Upgrade: websocket",
            "Connection: Upgrade",
            "Sec-WebSocket-Version: 13",
            f"Sec-WebSocket-Key: {key}",
            f"Cookie: {self.client.ws_cookie_header()['Cookie']}",
            "\r\n",
        ]

        self.raw = socket.create_connection((host, port), timeout=self.timeout)
        self.sock = self.raw
        if parts.scheme == "wss":
            context = self.client.ws_ssl_context()
            if context is None:
                context = ssl.create_default_context()
            self.sock = context.wrap_socket(self.raw, server_hostname=host)
        self.sock.settimeout(self.timeout)
        self.sock.sendall(("\r\n".join(headers)).encode("ascii"))

        response = b""
        while b"\r\n\r\n" not in response and len(response) < 4096:
            chunk = self.sock.recv(512)
            if not chunk:
                break
            response += chunk
        header = response.decode("iso-8859-1", errors="replace")
        status_line = header.splitlines()[0] if header else ""
        if " 101 " not in status_line:
            raise WebSocketError(f"handshake failed: {status_line or 'no response'}")
        if "upgrade: websocket" not in header.lower():
            raise WebSocketError("handshake missing websocket upgrade header")

    def send_text(self, payload: str) -> None:
        self.send_frame(0x1, payload.encode("utf-8"))

    def send_frame(self, opcode: int, payload: bytes) -> None:
        if not self.sock:
            raise WebSocketError("websocket is not connected")
        first = 0x80 | (opcode & 0x0F)
        length = len(payload)
        if length <= 125:
            header = struct.pack("!BB", first, 0x80 | length)
        elif length <= 0xFFFF:
            header = struct.pack("!BBH", first, 0x80 | 126, length)
        else:
            header = struct.pack("!BBQ", first, 0x80 | 127, length)
        mask = os.urandom(4)
        masked = bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))
        self.sock.sendall(header + mask + masked)

    def recv_frame(self, timeout: float | None = None) -> tuple[int, bytes]:
        if not self.sock:
            raise WebSocketError("websocket is not connected")
        previous_timeout = self.sock.gettimeout()
        if timeout is not None:
            self.sock.settimeout(timeout)
        try:
            while True:
                header = recv_exact(self.sock, 2)
                first, second = header
                opcode = first & 0x0F
                masked = (second & 0x80) != 0
                length = second & 0x7F
                if length == 126:
                    length = struct.unpack("!H", recv_exact(self.sock, 2))[0]
                elif length == 127:
                    length = struct.unpack("!Q", recv_exact(self.sock, 8))[0]
                mask = recv_exact(self.sock, 4) if masked else b""
                payload = recv_exact(self.sock, length) if length else b""
                if masked:
                    payload = bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))
                if opcode == 0x9:
                    self.send_frame(0xA, payload)
                    continue
                return opcode, payload
        finally:
            if timeout is not None:
                self.sock.settimeout(previous_timeout)

    def recv_text_json(
        self,
        predicate: Callable[[dict[str, Any]], bool],
        timeout: float,
    ) -> dict[str, Any]:
        deadline = time.monotonic() + timeout
        last_error = "no text frame"
        while time.monotonic() < deadline:
            remaining = max(0.1, deadline - time.monotonic())
            try:
                opcode, payload = self.recv_frame(timeout=remaining)
            except socket.timeout:
                break
            if opcode == 0x8:
                raise WebSocketError("server closed websocket before expected text frame")
            if opcode != 0x1:
                continue
            try:
                data = json.loads(payload.decode("utf-8"))
            except Exception as exc:  # noqa: BLE001 - keep the last parse failure for diagnostics.
                last_error = f"invalid JSON text frame: {exc}"
                continue
            if isinstance(data, dict) and predicate(data):
                return data
            last_error = f"unexpected text frame: {data}"
        raise WebSocketError(last_error)

    def close(self) -> None:
        if self.sock:
            try:
                self.send_frame(0x8, b"")
            except OSError:
                pass
            try:
                self.sock.close()
            except OSError:
                pass
        if self.raw and self.raw is not self.sock:
            try:
                self.raw.close()
            except OSError:
                pass
        self.sock = None
        self.raw = None


def wait_for_ws_active(
    client: DeviceClient,
    expected_max: int,
    timeout: float,
    poll_interval: float,
) -> dict[str, int]:
    deadline = time.monotonic() + timeout
    last = http_summary(client)
    while time.monotonic() < deadline:
        last = http_summary(client)
        if last["wsActiveClients"] <= expected_max:
            return last
        time.sleep(max(0.2, poll_interval))
    return last


def validate_system_status_snapshot(frame: dict[str, Any]) -> None:
    if frame.get("type") != "snapshot" or frame.get("channel") != "system_status":
        raise WebSocketError(f"unexpected snapshot envelope: {frame}")
    data = frame.get("data")
    if not isinstance(data, dict):
        raise WebSocketError("system_status snapshot data is not an object")
    if not isinstance(data.get("system_info"), dict):
        raise WebSocketError("system_status snapshot missing system_info")
    diagnostics = data.get("diagnostics")
    if not isinstance(diagnostics, dict) or not isinstance(diagnostics.get("http"), dict):
        raise WebSocketError("system_status snapshot missing diagnostics.http")
    missing = [field for field in HTTP_COUNTER_FIELDS if field not in diagnostics["http"]]
    if missing:
        raise WebSocketError("system_status diagnostics.http missing: " + ", ".join(missing))


def request_system_status_snapshot(ws: RawWebSocket, timeout: float) -> dict[str, Any]:
    ws.send_text(json.dumps({"subscribe": "system_status"}, separators=(",", ":")))
    ws.send_text(json.dumps({"snapshot": "system_status"}, separators=(",", ":")))
    frame = ws.recv_text_json(
        lambda data: data.get("type") == "snapshot" and data.get("channel") == "system_status",
        timeout,
    )
    validate_system_status_snapshot(frame)
    ws.send_text(json.dumps({"unsubscribe": "system_status"}, separators=(",", ":")))
    return frame


def run_single_snapshot(client: DeviceClient, timeout: float) -> dict[str, Any]:
    with RawWebSocket(client, timeout=timeout) as ws:
        return request_system_status_snapshot(ws, timeout)


def run_multi_client_snapshot(client: DeviceClient, clients: int, timeout: float) -> None:
    sockets: list[RawWebSocket] = []
    try:
        for _index in range(clients):
            ws = RawWebSocket(client, timeout=timeout)
            ws.connect()
            sockets.append(ws)
        for ws in sockets:
            ws.send_text(json.dumps({"subscribe": "system_status"}, separators=(",", ":")))
            ws.send_text(json.dumps({"snapshot": "system_status"}, separators=(",", ":")))
        for ws in sockets:
            frame = ws.recv_text_json(
                lambda data: data.get("type") == "snapshot" and data.get("channel") == "system_status",
                timeout,
            )
            validate_system_status_snapshot(frame)
            ws.send_text(json.dumps({"unsubscribe": "system_status"}, separators=(",", ":")))
    finally:
        for ws in sockets:
            ws.close()


def run_oversize_probe(client: DeviceClient, payload_bytes: int, timeout: float) -> None:
    ws = RawWebSocket(client, timeout=timeout)
    try:
        ws.connect()
        ws.send_text(json.dumps({"subscribe": "telemetry"}, separators=(",", ":")))
        ws.send_text("x" * payload_bytes)
        deadline = time.monotonic() + min(4.0, timeout)
        last_opcode: int | None = None
        while time.monotonic() < deadline:
            try:
                opcode, _payload = ws.recv_frame(timeout=max(0.1, deadline - time.monotonic()))
            except socket.timeout:
                break
            except WebSocketError as exc:
                if "socket closed" in str(exc):
                    return
                raise
            if opcode == 0x8:
                return
            # A queued binary heartbeat or event can arrive before the HTTPD
            # session close is observed on the client side.
            last_opcode = opcode
        raise WebSocketError(f"oversize frame did not close session, last_opcode={last_opcode}")
    finally:
        ws.close()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    parser.add_argument("--clients", type=int, default=3, help="Parallel /ws/system clients to open.")
    parser.add_argument("--ws-timeout-sec", type=float, default=8.0)
    parser.add_argument("--settle-timeout-sec", type=float, default=12.0)
    parser.add_argument("--poll-interval-sec", type=float, default=1.0)
    parser.add_argument("--oversize-bytes", type=int, default=384)
    parser.add_argument("--skip-oversize", action="store_true")
    args = parser.parse_args(argv)

    client = DeviceClient.from_args(args)
    try:
        client.login()
        before = http_summary(client)
        baseline_active = before["wsActiveClients"]
        baseline_drops = before["wsQueueDrops"]
        baseline_forced = before["wsForcedRemovals"]
        baseline_opens = before["wsOpens"]
        baseline_closes = before["wsCloses"]

        print(
            "[BASELINE] "
            f"wsActive={baseline_active} wsOpens={baseline_opens} "
            f"wsCloses={baseline_closes} wsDrops={baseline_drops}",
            flush=True,
        )

        run_single_snapshot(client, args.ws_timeout_sec)
        after_single = wait_for_ws_active(
            client,
            baseline_active,
            args.settle_timeout_sec,
            args.poll_interval_sec,
        )
        if after_single["wsActiveClients"] > baseline_active:
            raise WebSocketError(
                f"single snapshot client leaked: active {after_single['wsActiveClients']} > {baseline_active}"
            )

        clients = max(1, args.clients)
        run_multi_client_snapshot(client, clients, args.ws_timeout_sec)
        after_multi = wait_for_ws_active(
            client,
            baseline_active,
            args.settle_timeout_sec,
            args.poll_interval_sec,
        )
        if after_multi["wsActiveClients"] > baseline_active:
            raise WebSocketError(
                f"multi-client snapshot leaked: active {after_multi['wsActiveClients']} > {baseline_active}"
            )

        expected_sessions = 1 + clients
        if args.skip_oversize:
            after_oversize = after_multi
        else:
            oversize_bytes = max(257, args.oversize_bytes)
            run_oversize_probe(client, oversize_bytes, args.ws_timeout_sec)
            after_oversize = wait_for_ws_active(
                client,
                baseline_active,
                args.settle_timeout_sec,
                args.poll_interval_sec,
            )
            expected_sessions += 1
            if after_oversize["wsForcedRemovals"] < baseline_forced + 1:
                raise WebSocketError(
                    "oversize frame did not increment wsForcedRemovals "
                    f"({baseline_forced}->{after_oversize['wsForcedRemovals']})"
                )

        final = after_oversize
        if final["wsQueueDrops"] != baseline_drops:
            raise WebSocketError(
                f"wsQueueDrops changed during normal smoke: {baseline_drops}->{final['wsQueueDrops']}"
            )
        if final["wsActiveClients"] > baseline_active:
            raise WebSocketError(
                f"websocket clients still active after cleanup: {final['wsActiveClients']} > {baseline_active}"
            )
        if final["wsOpens"] < baseline_opens + expected_sessions:
            raise WebSocketError(
                f"wsOpens did not record all sessions: {baseline_opens}->{final['wsOpens']}"
            )
        if final["wsCloses"] < baseline_closes + expected_sessions:
            raise WebSocketError(
                f"wsCloses did not record all sessions: {baseline_closes}->{final['wsCloses']}"
            )

        report = {
            "target": client.base_url,
            "baseline": before,
            "final": final,
            "clients": clients,
            "oversize": not args.skip_oversize,
        }
        if args.json:
            print(json.dumps(report, indent=2, sort_keys=True))
        print(
            "PASS: /ws/system smoke succeeded "
            f"(sessions={expected_sessions}, active={final['wsActiveClients']}, "
            f"drops={final['wsQueueDrops']})"
        )
        return 0
    except (DeviceClientError, OSError, WebSocketError) as exc:
        print(f"FAIL: /ws/system smoke failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
