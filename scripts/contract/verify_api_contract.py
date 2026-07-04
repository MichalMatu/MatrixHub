#!/usr/bin/env python3
"""Verify that local API path literals are represented in the API contract."""

from __future__ import annotations

import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
CONTRACT_PATH = REPO_ROOT / "docs" / "engineering" / "api-contract.json"

SOURCE_GROUPS = {
    "firmware": [
        "src/api",
        "src/ble/settings",
        "src/config/Network.h",
        "src/config/json/ConfigKeys.h",
        "lib/framework",
    ],
    "frontend": [
        "interface/src/lib/services/api",
        "interface/src/lib/services/fileManager",
        "interface/src/lib/stores",
        "interface/src/lib/features",
        "interface/src/lib/bootstrap",
    ],
    "sdk": ["packages/device-sdk/src"],
    "scripts": ["scripts", "tools"],
    "docs_user": ["docs/user-guide"],
}

SOURCE_SUFFIXES = {".cpp", ".h", ".ts", ".svelte", ".py", ".sh", ".md"}
PATH_RE = re.compile(r"(?<![A-Za-z0-9_.-])/(?:api|rest|ws)/[A-Za-z0-9_][A-Za-z0-9_./{}$?=&%+-]*")
ENV_PREFIX_RE = re.compile(r"\$\{?[A-Za-z_][A-Za-z0-9_]*\}?")

IGNORED_PATHS = {
    "/api/sensors": "removed endpoint retained only in diagnostic log text",
}

REQUIRED_FIELDS = {
    "method",
    "path",
    "auth",
    "request",
    "response",
    "restartRequired",
    "firmware",
    "frontend",
    "sdk",
    "tests",
}


@dataclass(frozen=True)
class Occurrence:
    group: str
    file: Path


def strip_comments(text: str, *, strip_hash_comments: bool) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    cleaned_lines: list[str] = []
    for line in text.splitlines():
        if strip_hash_comments and line.lstrip().startswith("#"):
            cleaned_lines.append("")
            continue
        line = re.sub(r"//.*", "", line)
        cleaned_lines.append(line)
    return "\n".join(cleaned_lines)


def normalize_path(raw_path: str) -> str:
    path = raw_path.split("?", 1)[0]
    path = path.rstrip(".,);]")
    while "//" in path:
        path = path.replace("//", "/")
    return path.rstrip("/") if path != "/" else path


def iter_files(path: Path):
    if path.is_file():
        if path.suffix in SOURCE_SUFFIXES:
            yield path
        return

    if not path.exists():
        return

    for file in path.rglob("*"):
        if not file.is_file() or file.suffix not in SOURCE_SUFFIXES:
            continue
        parts = set(file.parts)
        if "__pycache__" in parts or "node_modules" in parts:
            continue
        if file.name.endswith((".test.ts", ".spec.ts", ".test.svelte.ts", ".spec.svelte.ts")):
            continue
        yield file


def extract_paths(file: Path) -> set[str]:
    text = file.read_text(encoding="utf-8", errors="ignore")
    text = ENV_PREFIX_RE.sub(
        "",
        strip_comments(text, strip_hash_comments=file.suffix in {".py", ".sh"}),
    )
    return {
        normalize_path(match.group(0))
        for match in PATH_RE.finditer(text)
        if normalize_path(match.group(0)) not in IGNORED_PATHS
    }


def scan_sources() -> dict[str, list[Occurrence]]:
    discovered: dict[str, list[Occurrence]] = {}
    for group, entries in SOURCE_GROUPS.items():
        for entry in entries:
            for file in iter_files(REPO_ROOT / entry):
                for path in extract_paths(file):
                    discovered.setdefault(path, []).append(
                        Occurrence(group=group, file=file.relative_to(REPO_ROOT))
                    )
    return discovered


def load_contract() -> list[dict]:
    try:
        data = json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))
    except FileNotFoundError:
        raise SystemExit(f"missing contract: {CONTRACT_PATH.relative_to(REPO_ROOT)}")
    except json.JSONDecodeError as exc:
        raise SystemExit(f"invalid contract JSON: {exc}") from exc

    endpoints = data.get("endpoints")
    if not isinstance(endpoints, list):
        raise SystemExit("contract must contain an endpoints array")
    return endpoints


def validate_contract_entries(endpoints: list[dict]) -> list[str]:
    errors: list[str] = []
    seen_ids: set[str] = set()
    for index, endpoint in enumerate(endpoints):
        missing = sorted(REQUIRED_FIELDS - set(endpoint))
        label = endpoint.get("id") or f"entry[{index}]"
        if missing:
            errors.append(f"{label}: missing required fields: {', '.join(missing)}")

        entry_id = endpoint.get("id")
        if isinstance(entry_id, str):
            if entry_id in seen_ids:
                errors.append(f"{label}: duplicate id")
            seen_ids.add(entry_id)

        path = endpoint.get("path")
        if not isinstance(path, str) or not path.startswith(("/api/", "/rest/", "/ws/")):
            errors.append(f"{label}: invalid local path {path!r}")

        if not isinstance(endpoint.get("restartRequired"), bool):
            errors.append(f"{label}: restartRequired must be a boolean")

        for field in ("firmware", "frontend", "sdk", "tests"):
            if field in endpoint and not isinstance(endpoint[field], list):
                errors.append(f"{label}: {field} must be an array")

        firmware = endpoint.get("firmware")
        if not isinstance(firmware, list) or not firmware:
            errors.append(f"{label}: firmware implementation list cannot be empty")
    return errors


def main() -> int:
    endpoints = load_contract()
    contract_errors = validate_contract_entries(endpoints)
    contract_paths = {normalize_path(entry["path"]) for entry in endpoints if isinstance(entry.get("path"), str)}
    discovered = scan_sources()

    source_errors: list[str] = []
    for path, occurrences in sorted(discovered.items()):
        if path not in contract_paths:
            sample = ", ".join(str(item.file) for item in occurrences[:4])
            source_errors.append(f"{path}: missing from contract (seen in {sample})")

    firmware_paths = {
        path for path, occurrences in discovered.items() if any(item.group == "firmware" for item in occurrences)
    }
    warnings = sorted(path for path in contract_paths if path not in firmware_paths)

    if contract_errors or source_errors:
        print("API contract verification FAILED", file=sys.stderr)
        for error in contract_errors + source_errors:
            print(f"  - {error}", file=sys.stderr)
        if warnings:
            print("Warnings:", file=sys.stderr)
            for path in warnings:
                print(f"  - {path}: not observed in firmware scan", file=sys.stderr)
        return 1

    print(
        "API contract verification OK: "
        f"{len(contract_paths)} paths, {sum(len(v) for v in discovered.values())} source occurrence(s)."
    )
    if warnings:
        print("Warnings:")
        for path in warnings:
            print(f"  - {path}: not observed in firmware scan")
    return 0


if __name__ == "__main__":
    sys.exit(main())
