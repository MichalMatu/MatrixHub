#!/usr/bin/env python3
"""Smoke-check MatrixHub runtime diagnostics endpoints over HTTPS/JWT."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]
sys.path.insert(0, str(REPO_ROOT / "scripts"))

from device_client import DeviceClient, DeviceClientError, add_common_device_args, fail, print_json


ENDPOINTS = (
    "/api/diagnostics/summary",
    "/api/diagnostics/heap",
    "/api/diagnostics/tasks?details=0",
    "/api/diagnostics/mutexes",
    "/api/diagnostics/endpoints",
    "/api/diagnostics/features",
)


def require_object(path: str, payload: Any) -> None:
    if not isinstance(payload, dict):
        raise DeviceClientError(f"{path} returned {type(payload).__name__}, expected JSON object")


def validate(path: str, payload: dict[str, Any]) -> None:
    if path.startswith("/api/diagnostics/summary"):
        for key in ("schema", "firmware", "boot", "watchdog", "heap", "http", "features"):
            if key not in payload:
                raise DeviceClientError(f"{path} missing {key}")
        http = payload.get("http")
        if not isinstance(http, dict):
            raise DeviceClientError(f"{path} http is not an object")
        for key in (
            "wsActiveClients",
            "wsPeakClients",
            "wsOpens",
            "wsCloses",
            "wsForcedRemovals",
            "wsQueueDrops",
        ):
            if key not in http:
                raise DeviceClientError(f"{path} missing http.{key}")
    elif path.startswith("/api/diagnostics/heap"):
        regions = payload.get("regions")
        if not isinstance(regions, dict) or "internal" not in regions or "psram" not in regions:
            raise DeviceClientError(f"{path} missing heap regions")
    elif path.startswith("/api/diagnostics/tasks"):
        if "watchdog" not in payload or "memory" not in payload:
            raise DeviceClientError(f"{path} missing watchdog/memory")
        memory = payload.get("memory")
        if not isinstance(memory, dict):
            raise DeviceClientError(f"{path} memory is not an object")
        for region_name in ("default", "internal", "psram"):
            region = memory.get(region_name)
            if not isinstance(region, dict):
                raise DeviceClientError(f"{path} missing memory.{region_name}")
            for key in ("free", "minimumFree", "largestBlock", "fragmentationPercent"):
                if key not in region:
                    raise DeviceClientError(f"{path} missing memory.{region_name}.{key}")
        stack = payload.get("stack")
        if not isinstance(stack, dict) or "detailsAvailable" not in stack:
            raise DeviceClientError(f"{path} missing stack summary")
    elif path.startswith("/api/diagnostics/mutexes"):
        if "instrumented" not in payload or "criticalLocks" not in payload:
            raise DeviceClientError(f"{path} missing mutex coverage fields")
    elif path.startswith("/api/diagnostics/endpoints"):
        if "diagnostics" not in payload or "metrics" not in payload:
            raise DeviceClientError(f"{path} missing endpoint catalog")
    elif path.startswith("/api/diagnostics/features"):
        if "features" not in payload or "configRead" not in payload:
            raise DeviceClientError(f"{path} missing feature snapshot")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    args = parser.parse_args()

    client = DeviceClient.from_args(args)
    results: dict[str, Any] = {}

    try:
        for path in ENDPOINTS:
            payload = client.json("GET", path)
            require_object(path, payload)
            validate(path, payload)
            results[path] = payload
    except DeviceClientError as exc:
        return fail(str(exc))

    if args.json:
        print_json(results)
    else:
        print(f"OK runtime diagnostics: {len(results)} endpoint(s) healthy at {client.base_url}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
