#!/usr/bin/env python3
"""Shared HTTPS/JWT client for MatrixHub diagnostic scripts."""

from __future__ import annotations

import argparse
import json
import os
import ssl
import sys
from dataclasses import dataclass
from typing import Any
from urllib.parse import urlsplit, urlunsplit

import requests
from requests.adapters import HTTPAdapter
from urllib3.exceptions import InsecureRequestWarning
from urllib3.util.retry import Retry

DEFAULT_DEVICE_URL = "https://192.168.0.18"
DEFAULT_ALIAS_URL = "https://matrixhub.local"
DEFAULT_USER = "admin"
DEFAULT_PASSWORD = "admin"


class DeviceClientError(RuntimeError):
    """Raised when a device diagnostic request cannot complete."""


def _truthy(value: str | None) -> bool:
    return (value or "").strip().lower() in {"1", "true", "yes", "on"}


def normalize_base_url(value: str | None) -> str:
    url = (value or os.environ.get("DEVICE_URL") or DEFAULT_DEVICE_URL).strip()
    if not url:
        url = DEFAULT_DEVICE_URL
    if "://" not in url:
        url = f"https://{url}"
    return url.rstrip("/")


def websocket_url(base_url: str, path: str) -> str:
    parts = urlsplit(normalize_base_url(base_url))
    scheme = "wss" if parts.scheme == "https" else "ws"
    clean_path = path if path.startswith("/") else f"/{path}"
    return urlunsplit((scheme, parts.netloc, clean_path, "", ""))


def websocket_ssl_context(base_url: str, verify: bool) -> ssl.SSLContext | None:
    if urlsplit(normalize_base_url(base_url)).scheme != "https":
        return None
    if verify:
        return ssl.create_default_context()
    return ssl._create_unverified_context()


def add_common_device_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--device-url",
        "--host",
        "--base-url",
        dest="device_url",
        default=os.environ.get("DEVICE_URL", DEFAULT_DEVICE_URL),
        help="Device base URL. Default: DEVICE_URL or https://192.168.0.18.",
    )
    parser.add_argument(
        "--user",
        "--username",
        default=os.environ.get("DEVICE_USER", DEFAULT_USER),
        help="JWT username. Default: DEVICE_USER or admin.",
    )
    parser.add_argument(
        "--password",
        default=os.environ.get("DEVICE_PASSWORD", DEFAULT_PASSWORD),
        help="JWT password. Default: DEVICE_PASSWORD or admin.",
    )
    parser.add_argument(
        "--token",
        default=os.environ.get("DEVICE_TOKEN"),
        help="Existing JWT access token. Default: DEVICE_TOKEN.",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=float(os.environ.get("DEVICE_TIMEOUT", "10")),
        help="HTTP timeout in seconds. Default: DEVICE_TIMEOUT or 10.",
    )
    parser.add_argument(
        "--tls-verify",
        action="store_true",
        default=_truthy(os.environ.get("DEVICE_TLS_VERIFY")),
        help="Verify TLS certificates. Default is disabled for self-signed device certs.",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Emit machine-readable JSON where the script supports it.",
    )


@dataclass
class DeviceClient:
    base_url: str
    username: str = DEFAULT_USER
    password: str = DEFAULT_PASSWORD
    timeout: float = 10.0
    verify: bool = False
    retries: int = 3

    def __post_init__(self) -> None:
        self.base_url = normalize_base_url(self.base_url)
        self.token: str | None = None
        self.session = requests.Session()
        self.session.verify = self.verify
        self.session.headers.update({"User-Agent": "MatrixHubDiagnostics/1.0"})
        if not self.verify:
            requests.packages.urllib3.disable_warnings(category=InsecureRequestWarning)

        retry = Retry(
            total=self.retries,
            backoff_factor=0.4,
            status_forcelist=(500, 502, 503, 504),
            allowed_methods=frozenset({"GET", "POST", "DELETE"}),
        )
        adapter = HTTPAdapter(max_retries=retry)
        self.session.mount("http://", adapter)
        self.session.mount("https://", adapter)

    @classmethod
    def from_args(cls, args: argparse.Namespace) -> "DeviceClient":
        client = cls(
            base_url=args.device_url,
            username=args.user,
            password=args.password,
            timeout=args.timeout,
            verify=args.tls_verify,
        )
        if args.token:
            client.set_token(args.token)
        return client

    def set_token(self, token: str) -> None:
        clean = token.strip()
        if clean:
            self.token = clean
            self.session.headers.update({"Authorization": f"Bearer {self.token}"})

    def login(self, force: bool = False) -> str:
        if self.token and not force:
            return self.token
        response = self.session.post(
            f"{self.base_url}/rest/signIn",
            json={"username": self.username, "password": self.password},
            timeout=self.timeout,
        )
        if response.status_code != 200:
            raise DeviceClientError(
                f"signIn failed: HTTP {response.status_code} {response.text[:200]}"
            )
        try:
            data = response.json()
        except ValueError as exc:
            raise DeviceClientError("signIn returned non-JSON response") from exc
        token = data.get("access_token")
        if not isinstance(token, str) or not token.strip():
            raise DeviceClientError("signIn succeeded but no access_token was returned")
        self.token = token.strip()
        self.session.headers.update({"Authorization": f"Bearer {self.token}"})
        return self.token

    def request(self, method: str, path: str, *, auth: bool = True, **kwargs: Any) -> requests.Response:
        if auth:
            self.login()
        timeout = kwargs.pop("timeout", self.timeout)
        url = path if path.startswith("http://") or path.startswith("https://") else f"{self.base_url}{path}"
        response = self.session.request(method, url, timeout=timeout, **kwargs)
        if auth and response.status_code == 401:
            self.login(force=True)
            response = self.session.request(method, url, timeout=timeout, **kwargs)
        return response

    def get(self, path: str, **kwargs: Any) -> requests.Response:
        return self.request("GET", path, **kwargs)

    def post(self, path: str, **kwargs: Any) -> requests.Response:
        return self.request("POST", path, **kwargs)

    def delete(self, path: str, **kwargs: Any) -> requests.Response:
        return self.request("DELETE", path, **kwargs)

    def json(self, method: str, path: str, *, expected: tuple[int, ...] = (200,), **kwargs: Any) -> Any:
        response = self.request(method, path, **kwargs)
        if response.status_code not in expected:
            raise DeviceClientError(
                f"{method} {path} failed: HTTP {response.status_code} {response.text[:300]}"
            )
        try:
            return response.json()
        except ValueError as exc:
            raise DeviceClientError(f"{method} {path} returned non-JSON response") from exc

    def ws_cookie_header(self) -> dict[str, str]:
        token = self.login()
        return {"Cookie": f"access_token={token}"}

    def ws_url(self, path: str) -> str:
        return websocket_url(self.base_url, path)

    def ws_ssl_context(self) -> ssl.SSLContext | None:
        return websocket_ssl_context(self.base_url, self.verify)


def print_json(data: Any) -> None:
    print(json.dumps(data, indent=2, sort_keys=True))


def fail(message: str, code: int = 1) -> int:
    print(f"ERROR: {message}", file=sys.stderr)
    return code
