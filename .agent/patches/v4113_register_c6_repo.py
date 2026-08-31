from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path

home = Path.home()
local_agent = home / "local-agent"
registry = home / "Library" / "Application Support" / "local-agent" / "repositories.json"
new_entry = {
    "id": "esp32-c6-zigbee",
    "repository": "MichalMatu/esp32_c6_zigbee",
    "control_branch": "agent-control",
    "default_branch": "main",
}

original = registry.read_bytes()
payload = json.loads(original.decode("utf-8"))
if payload.get("version") != 1 or not isinstance(payload.get("repositories"), list):
    raise RuntimeError("unexpected repository registry schema")

entries = payload["repositories"]
by_id = [item for item in entries if str(item.get("id", "")).casefold() == new_entry["id"].casefold()]
by_repo = [item for item in entries if str(item.get("repository", "")).casefold() == new_entry["repository"].casefold()]
if by_id or by_repo:
    if len(by_id) != 1 or len(by_repo) != 1 or by_id[0] is not by_repo[0]:
        raise RuntimeError("repository id/remote already exists with conflicting registry identity")
    existing = by_id[0]
    for key, value in new_entry.items():
        if existing.get(key, value) != value:
            raise RuntimeError(f"existing registry entry differs for {key}: {existing.get(key)!r}")
    changed = False
else:
    entries.append(new_entry)
    changed = True

encoded = (json.dumps(payload, indent=2, ensure_ascii=True) + "\n").encode("utf-8")


def atomic_write(data: bytes) -> None:
    registry.parent.mkdir(parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(prefix="repositories.", suffix=".tmp", dir=registry.parent)
    try:
        with os.fdopen(fd, "wb") as handle:
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(name, registry)
    finally:
        try:
            os.unlink(name)
        except FileNotFoundError:
            pass


if changed:
    atomic_write(encoded)

try:
    subprocess.run(
        [str(local_agent / ".venv" / "bin" / "python"), str(local_agent / "agent_repo_admin.py"), "--registry", str(registry), "list"],
        cwd=local_agent,
        check=True,
        text=True,
    )
    subprocess.run(
        [str(local_agent / ".venv" / "bin" / "python"), str(local_agent / "agent_repo_admin.py"), "--registry", str(registry), "provision", "--repository-id", "esp32-c6-zigbee"],
        cwd=local_agent,
        check=True,
        text=True,
    )
    subprocess.run(
        [str(local_agent / ".venv" / "bin" / "python"), str(local_agent / "agent_repo_admin.py"), "--registry", str(registry), "validate"],
        cwd=local_agent,
        check=True,
        text=True,
    )
except Exception:
    if changed:
        atomic_write(original)
    raise

print(f"REGISTRY_CHANGED={str(changed).lower()}")
print("REGISTERED_REPOSITORY_ID=esp32-c6-zigbee")
print("REGISTERED_REPOSITORY=MichalMatu/esp32_c6_zigbee")
