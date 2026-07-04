#!/usr/bin/env python3
"""Check UDP push settings and optionally exercise a real UDP packet over HTTPS/JWT."""

from __future__ import annotations

import argparse
import socket
import sys
import threading
from pathlib import Path
from typing import Any
from urllib.parse import urlsplit

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args, print_json  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parent))
from udp_listener import UdpPacket, packet_to_dict, receive_udp_packets  # noqa: E402


VALID_FORMATS = {"line", "json", "csv"}


def require_mapping(name: str, value: Any) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise DeviceClientError(f"{name} returned non-object JSON")
    return value


def validate_settings(settings: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    if not isinstance(settings.get("enabled"), bool):
        errors.append("/api/udp missing boolean enabled")
    if not isinstance(settings.get("host"), str):
        errors.append("/api/udp missing string host")
    if not isinstance(settings.get("port"), int) or not (1 <= int(settings.get("port", 0)) <= 65535):
        errors.append("/api/udp missing valid integer port")
    if settings.get("format") not in VALID_FORMATS:
        errors.append("/api/udp missing valid format")
    if not isinstance(settings.get("interval_ms"), int) or int(settings.get("interval_ms", 0)) <= 0:
        errors.append("/api/udp missing positive interval_ms")
    if settings.get("enabled") is True and not str(settings.get("host", "")).strip():
        errors.append("/api/udp enabled but host is empty")
    return errors


def udp_payload_for_restore(settings: dict[str, Any]) -> dict[str, Any]:
    return {
        "enabled": bool(settings.get("enabled", False)),
        "host": str(settings.get("host", "")),
        "port": int(settings.get("port", 8094) or 8094),
        "format": settings.get("format") if settings.get("format") in VALID_FORMATS else "line",
        "interval_ms": int(settings.get("interval_ms", 60000) or 60000),
    }


def infer_local_ip_for_device(device_url: str) -> str:
    host = urlsplit(device_url).hostname or "192.168.0.30"
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.connect((host, 9))
        return sock.getsockname()[0]


def find_free_udp_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(("0.0.0.0", 0))
        return int(sock.getsockname()[1])


def post_udp_settings(client: DeviceClient, payload: dict[str, Any]) -> dict[str, Any]:
    return require_mapping("UDP settings update", client.json("POST", "/api/udp", json=payload))


def exercise_safe_udp_test(
    client: DeviceClient,
    current: dict[str, Any],
    *,
    device_url: str,
    target_host: str | None,
    target_port: int,
    timeout_s: float,
) -> dict[str, Any]:
    results: dict[str, Any] = {}
    restore_payload = udp_payload_for_restore(current)
    listener_port = target_port if target_port > 0 else find_free_udp_port()
    resolved_host = target_host or infer_local_ip_for_device(device_url)

    listener_packets: list[UdpPacket] = []
    listener_error: list[str] = []
    listener_ready = threading.Event()

    def listen() -> None:
        try:
            listener_packets.extend(
                receive_udp_packets(
                    host="0.0.0.0",
                    port=listener_port,
                    timeout_s=timeout_s,
                    count=1,
                    ready_callback=listener_ready.set,
                )
            )
        except OSError as exc:
            listener_error.append(str(exc))
            listener_ready.set()

    thread = threading.Thread(target=listen, name="udp-listener", daemon=True)
    thread.start()
    if not listener_ready.wait(timeout=2.0):
        listener_error.append("listener did not become ready")

    try:
        disabled_payload = dict(restore_payload)
        disabled_payload["enabled"] = False
        disabled = post_udp_settings(client, disabled_payload)
        results["disable"] = {"ok": disabled.get("enabled") is False}

        disabled_test = client.post("/api/udp/test")
        results["push_while_disabled"] = {
            "http_status": disabled_test.status_code,
            "ok": disabled_test.status_code == 400,
            "body": disabled_test.text[:200],
        }

        test_payload = {
            "enabled": True,
            "host": resolved_host,
            "port": listener_port,
            "format": "json",
            "interval_ms": max(10000, int(restore_payload.get("interval_ms", 60000))),
        }
        configured = post_udp_settings(client, test_payload)
        results["configure_listener"] = {
            "ok": configured.get("enabled") is True
            and configured.get("host") == resolved_host
            and configured.get("port") == listener_port,
            "host": resolved_host,
            "port": listener_port,
        }

        push = client.post("/api/udp/test")
        try:
            push_body: Any = push.json()
        except ValueError:
            push_body = push.text[:200]
        results["push"] = {
            "http_status": push.status_code,
            "ok": push.status_code == 200,
            "body": push_body,
        }

        thread.join(timeout_s + 1.0)
        results["listener"] = {
            "ok": len(listener_packets) > 0 and not listener_error,
            "errors": listener_error,
            "packets": [packet_to_dict(packet) for packet in listener_packets],
        }
        if not results["listener"]["ok"] and push.status_code == 200:
            results["listener"]["hint"] = (
                "UDP test was accepted by the device but no packet reached this listener. "
                "Check host firewall rules for incoming UDP on the selected port."
            )
    finally:
        try:
            restored = post_udp_settings(client, restore_payload)
            results["restore"] = {
                "ok": udp_payload_for_restore(restored) == restore_payload,
                "settings": restored,
            }
        except Exception as exc:
            results["restore"] = {"ok": False, "error": str(exc)}

    return results


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--enable", action="store_true", help="Enable UDP push with the supplied target.")
    mode.add_argument("--disable", action="store_true", help="Disable UDP push.")
    parser.add_argument("--safe-test", action="store_true", help="Temporarily configure a local listener, test push, and restore settings.")
    parser.add_argument("--target-host", default=None, help="Receiver host for --enable/--safe-test. Default: auto-detect local IP.")
    parser.add_argument("--target-port", type=int, default=0, help="Receiver port. Default: random free port for --safe-test, 8094 for --enable.")
    parser.add_argument("--format", choices=tuple(sorted(VALID_FORMATS)), default="line")
    parser.add_argument("--interval-ms", type=int, default=60000)
    parser.add_argument("--listener-timeout", type=float, default=15.0)
    args = parser.parse_args(argv)

    client = DeviceClient.from_args(args)
    try:
        current = require_mapping("UDP settings", client.json("GET", "/api/udp"))
        errors = validate_settings(current)
        exercise: dict[str, Any] = {}

        if args.enable or args.disable:
            port = args.target_port if args.target_port > 0 else 8094
            payload = {
                "enabled": bool(args.enable),
                "host": args.target_host if args.enable else current.get("host", ""),
                "port": port if args.enable else current.get("port", port),
                "format": args.format if args.enable else current.get("format", args.format),
                "interval_ms": max(1000, args.interval_ms if args.enable else current.get("interval_ms", args.interval_ms)),
            }
            current = post_udp_settings(client, payload)
            errors = validate_settings(current)

        if args.safe_test:
            exercise = exercise_safe_udp_test(
                client,
                current,
                device_url=client.base_url,
                target_host=args.target_host,
                target_port=args.target_port,
                timeout_s=args.listener_timeout,
            )
            current = require_mapping("UDP settings", client.json("GET", "/api/udp"))
            errors = validate_settings(current)
    except DeviceClientError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    exercise_ok = all(isinstance(item, dict) and item.get("ok", True) for item in exercise.values())
    report = {
        "ok": not errors and exercise_ok,
        "errors": errors,
        "exercise": exercise,
        "settings": current,
    }
    if not args.json:
        print(f"Target: {client.base_url}")
    print_json(report)
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
