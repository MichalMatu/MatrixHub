#!/usr/bin/env python3
"""Security smoke tests for HTTPS/JWT MatrixHub endpoints."""

from __future__ import annotations

import argparse
import io
import socket
import ssl
import sys
import time
import uuid
from pathlib import Path
from urllib.parse import urlsplit

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args  # noqa: E402


def login_with_retry(client: DeviceClient, label: str, max_wait_s: float = 75.0) -> str:
    deadline = time.monotonic() + max_wait_s
    while True:
        response = client.session.post(
            f"{client.base_url}/rest/signIn",
            json={"username": client.username, "password": client.password},
            timeout=client.timeout,
        )
        if response.status_code == 429 and time.monotonic() < deadline:
            print(f"[INFO] {label}: login rate limited, waiting 5s")
            time.sleep(5)
            continue
        if response.status_code != 200:
            raise DeviceClientError(
                f"{label}: signIn failed: HTTP {response.status_code} {response.text[:200]}"
            )
        try:
            data = response.json()
        except ValueError as exc:
            raise DeviceClientError(f"{label}: signIn returned non-JSON response") from exc
        token = data.get("access_token")
        if not isinstance(token, str) or not token.strip():
            raise DeviceClientError(f"{label}: signIn returned no access_token")
        client.set_token(token)
        return token


def absolute_url(client: DeviceClient, path: str) -> str:
    if path.startswith("http://") or path.startswith("https://"):
        return path
    return f"{client.base_url}{path}"


def raw_request(client: DeviceClient, method: str, path: str, **kwargs):
    return client.session.request(method, absolute_url(client, path), timeout=client.timeout, **kwargs)


def expect_status(label: str, response, expected: set[int]) -> bool:
    expected_text = "/".join(str(code) for code in sorted(expected))
    print(f"{label}: HTTP {response.status_code} (expected {expected_text})")
    if response.status_code in expected:
        print(f"[PASS] {label}")
        return True
    print(f"[FAIL] {label}: {response.text[:240]}")
    return False


def sanitize_users(settings: dict) -> list[dict]:
    users = settings.get("users")
    if not isinstance(users, list):
        return []
    sanitized: list[dict] = []
    for entry in users:
        if not isinstance(entry, dict):
            continue
        username = entry.get("username")
        if not isinstance(username, str) or not username:
            continue
        sanitized.append(
            {
                "username": username,
                "password": entry.get("password") if isinstance(entry.get("password"), str) else "",
                "admin": bool(entry.get("admin")),
            }
        )
    return sanitized


def save_security_users(client: DeviceClient, users: list[dict]) -> dict:
    return client.json(
        "POST",
        "/rest/securitySettings",
        json={"jwt_secret": "", "users": users},
        timeout=20,
    )


def test_auth_required(client: DeviceClient) -> bool:
    print("\n--- Auth required ---")
    public = DeviceClient(client.base_url, timeout=client.timeout, verify=client.verify)
    response = public.request("GET", "/api/diagnostics/summary", auth=False)
    return expect_status("missing token rejected on admin diagnostics", response, {401})


def test_invalid_token(client: DeviceClient) -> bool:
    print("\n--- Invalid token ---")
    invalid = DeviceClient(client.base_url, timeout=client.timeout, verify=client.verify)
    invalid.set_token("invalid.token.value")
    response = raw_request(invalid, "GET", "/api/diagnostics/summary")
    return expect_status("invalid bearer token rejected", response, {401})


def test_security_settings_secret_redacted(client: DeviceClient) -> bool:
    print("\n--- Security settings redaction ---")
    settings = client.json("GET", "/rest/securitySettings")
    leaked = settings.get("jwt_secret")
    configured = settings.get("jwt_secret_configured")
    print(f"jwt_secret present={leaked is not None} configured={configured}")
    if leaked:
        print("[FAIL] JWT signing secret leaked by /rest/securitySettings")
        return False
    print("[PASS] JWT signing secret is write-only over HTTP")
    return True


def test_non_admin_and_revoked_token(client: DeviceClient) -> bool:
    print("\n--- Non-admin authorization and token revocation ---")
    username = f"security_probe_{uuid.uuid4().hex[:8]}"
    old_password = f"old-{uuid.uuid4().hex[:8]}"
    new_password = f"new-{uuid.uuid4().hex[:8]}"
    failures = 0

    original = client.json("GET", "/rest/securitySettings")
    users = [entry for entry in sanitize_users(original) if entry["username"] != username]
    users.append({"username": username, "password": old_password, "admin": False})

    try:
        save_security_users(client, users)
        probe = DeviceClient(client.base_url, username, old_password, timeout=client.timeout, verify=client.verify)
        login_with_retry(probe, "temporary non-admin user")

        forbidden = raw_request(probe, "GET", "/api/diagnostics/summary")
        if not expect_status("non-admin rejected from admin diagnostics", forbidden, {403}):
            failures += 1

        changed_users = []
        for entry in users:
            if entry["username"] == username:
                changed_users.append({"username": username, "password": new_password, "admin": False})
            else:
                changed_users.append(entry)
        save_security_users(client, changed_users)

        revoked = raw_request(probe, "GET", "/rest/verifyAuthorization")
        if not expect_status("old token rejected after password change", revoked, {401}):
            failures += 1

        updated_probe = DeviceClient(
            client.base_url,
            username,
            new_password,
            timeout=client.timeout,
            verify=client.verify,
        )
        login_with_retry(updated_probe, "temporary non-admin user after password change")
        verified = raw_request(updated_probe, "GET", "/rest/verifyAuthorization")
        if not expect_status("new token accepted after password change", verified, {200}):
            failures += 1
    finally:
        try:
            latest = client.json("GET", "/rest/securitySettings")
            cleaned_users = [
                entry for entry in sanitize_users(latest) if entry["username"] != username
            ]
            save_security_users(client, cleaned_users)
            print("[INFO] temporary security probe user removed")
        except Exception as exc:  # pragma: no cover - best-effort cleanup on device
            print(f"[WARN] failed to clean temporary user {username}: {exc}")

    return failures == 0


def test_content_length(client: DeviceClient) -> bool:
    print("\n--- Content-Length protection ---")
    big_data = "A" * 20000
    response = client.post(
        "/api/config",
        data=big_data,
        headers={"Content-Type": "text/plain"},
        timeout=8,
    )
    print(f"Response: HTTP {response.status_code}")
    if response.status_code == 413:
        print("[PASS] oversized payload rejected")
        return True
    lower_body = response.text.lower()
    if response.status_code == 400 and (
        "payload" in lower_body or "less than" in lower_body or "too large" in lower_body
    ):
        print("[PASS] oversized payload rejected with legacy HTTP 400")
        return True
    print(f"[FAIL] expected payload rejection, got {response.status_code}: {response.text[:200]}")
    return False


def test_invalid_json(client: DeviceClient) -> bool:
    print("\n--- Invalid JSON protection ---")
    response = client.post(
        "/api/config",
        data="{",
        headers={"Content-Type": "application/json"},
        timeout=8,
    )
    return expect_status("invalid JSON rejected", response, {400})


def test_path_traversal(client: DeviceClient) -> bool:
    print("\n--- File manager path traversal protection ---")
    list_response = client.get("/rest/fs/list?dir=/littlefs/../config", timeout=8)
    download_response = client.get(
        "/rest/fs/download?path=/littlefs/../config/securitySettings.json",
        timeout=8,
    )
    return (
        expect_status("list traversal rejected", list_response, {400, 404})
        and expect_status("download traversal rejected", download_response, {400, 404})
    )


def test_log_upload_protection(client: DeviceClient) -> bool:
    print("\n--- Log storage upload protection ---")
    files = {"file": ("security-probe.txt", io.BytesIO(b"probe"), "text/plain")}
    response = client.post("/rest/fs/upload?path=/data", files=files, timeout=15)
    if response.status_code == 403:
        print("[PASS] upload into log storage rejected")
        return True
    if response.status_code == 404 and "fs/invalid_path" in response.text:
        print("[PASS] upload into log storage rejected as invalid path")
        return True
    print(f"[FAIL] expected protected log upload rejection, got {response.status_code}: {response.text[:240]}")
    return False


def test_slowloris(client: DeviceClient) -> bool:
    print("\n--- Slowloris protection ---")
    parts = urlsplit(client.base_url)
    host = parts.hostname
    port = parts.port or (443 if parts.scheme == "https" else 80)
    if not host:
        print("[FAIL] cannot parse device host")
        return False

    raw = socket.create_connection((host, port), timeout=5)
    raw.settimeout(5)
    conn: socket.socket | ssl.SSLSocket
    if parts.scheme == "https":
        context = ssl._create_unverified_context() if not client.verify else ssl.create_default_context()
        conn = context.wrap_socket(raw, server_hostname=host)
    else:
        conn = raw

    request = (
        "POST /api/config HTTP/1.1\r\n"
        f"Host: {host}\r\n"
        "Content-Length: 100\r\n"
        "Content-Type: application/json\r\n"
        "\r\n"
    )
    try:
        conn.sendall(request.encode("ascii"))
        for idx in range(10):
            time.sleep(2)
            try:
                conn.sendall(b"A")
            except OSError:
                print("[PASS] server closed slow body connection")
                return True
            conn.setblocking(False)
            try:
                data = conn.recv(1024)
                if not data:
                    print("[PASS] server closed slow body connection")
                    return True
            except BlockingIOError:
                pass
            finally:
                conn.setblocking(True)
            print(f"sent slow byte {idx + 1}/10")
    except Exception as exc:
        print(f"[PASS] connection closed/failed during slow body: {exc}")
        return True
    finally:
        try:
            conn.close()
        except Exception:
            pass

    print("[FAIL] server kept slow body connection open too long")
    return False


def test_rate_limiter(client: DeviceClient) -> bool:
    print("\n--- Login rate limiter ---")
    triggered = False
    for idx in range(5):
        response = client.request(
            "POST",
            "/rest/signIn",
            auth=False,
            json={"username": "rate-limit-probe", "password": "bad"},
            timeout=3,
        )
        print(f"Attempt {idx + 1}: HTTP {response.status_code}")
        if response.status_code == 429:
            triggered = True
            break
    if triggered:
        print("[PASS] login rate limiter returned 429")
        return True
    print("[FAIL] login rate limiter did not return 429")
    return False


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_device_args(parser)
    parser.add_argument("--skip-slowloris", action="store_true")
    parser.add_argument("--skip-rate-limit", action="store_true")
    args = parser.parse_args(argv)

    client = DeviceClient.from_args(args)
    failures = 0
    try:
        login_with_retry(client, "admin preflight")
    except DeviceClientError as exc:
        print(f"ERROR: login failed before security tests: {exc}", file=sys.stderr)
        return 1

    for check in (
        test_security_settings_secret_redacted,
        test_auth_required,
        test_invalid_token,
        test_non_admin_and_revoked_token,
        test_invalid_json,
        test_path_traversal,
        test_log_upload_protection,
    ):
        if not check(client):
            failures += 1

    if not test_content_length(client):
        failures += 1
    if not args.skip_slowloris and not test_slowloris(client):
        failures += 1
    if not args.skip_rate_limit and not test_rate_limiter(client):
        failures += 1

    if failures:
        print(f"\nFAIL: {failures} security check(s) failed")
        return 1
    print("\nPASS: security smoke checks succeeded")
    return 0


if __name__ == "__main__":
    sys.exit(main())
