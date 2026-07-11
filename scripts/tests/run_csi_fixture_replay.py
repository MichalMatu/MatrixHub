#!/usr/bin/env python3
"""Fail-closed preflight and native replay for reviewed real CSI fixtures."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT))

from scripts.sensing_analysis.csi_capture import (  # noqa: E402
    CAPTURE_FILENAME,
    SCENARIO_FILENAME,
    verify_capture,
)


DEFAULT_FIXTURE_ROOT = REPO_ROOT / "test" / "fixtures" / "csi"
ALLOWED_ROOT_FILES = {"README.md", ".gitkeep"}


class CorpusError(RuntimeError):
    """Raised when the release corpus cannot be trusted or is empty."""


def discover_fixture_directories(root: Path) -> list[Path]:
    root = root.expanduser().resolve()
    if not root.is_dir():
        raise CorpusError(f"CSI fixture root is not a directory: {root}")

    fixtures: list[Path] = []
    for entry in sorted(root.iterdir(), key=lambda item: item.name):
        if entry.is_symlink():
            raise CorpusError(f"symbolic links are not allowed in CSI fixture root: {entry}")
        if entry.is_dir():
            fixtures.append(entry)
            continue
        if entry.name in ALLOWED_ROOT_FILES and entry.is_file():
            continue
        raise CorpusError(f"unexpected entry in CSI fixture root: {entry}")
    if not fixtures:
        raise CorpusError(
            f"real CSI fixture corpus is empty: {root}; promote at least one reviewed capture"
        )
    return fixtures


def preflight_corpus(root: Path) -> list[Path]:
    fixtures = discover_fixture_directories(root)
    required_names = {CAPTURE_FILENAME, SCENARIO_FILENAME}
    for fixture in fixtures:
        fixture_entries = list(fixture.iterdir())
        entries = {entry.name for entry in fixture_entries}
        if entries != required_names:
            missing = sorted(required_names - entries)
            unexpected = sorted(entries - required_names)
            details: list[str] = []
            if missing:
                details.append(f"missing={','.join(missing)}")
            if unexpected:
                details.append(f"unexpected={','.join(unexpected)}")
            raise CorpusError(f"invalid promoted fixture {fixture}: {'; '.join(details)}")
        if any(entry.is_symlink() for entry in fixture_entries):
            raise CorpusError(f"symbolic links are not allowed in fixture: {fixture}")
        if not all((fixture / name).is_file() for name in required_names):
            raise CorpusError(f"fixture entries must be regular files: {fixture}")
        result = verify_capture(fixture)
        print(
            f"[CSI preflight] {fixture.name}: "
            f"frames={result['frame_count']} sha256={result['sha256']}"
        )
    return fixtures


def resolve_pio(explicit: Path | None) -> str:
    if explicit is not None:
        candidate = explicit.expanduser().resolve()
        if not candidate.is_file():
            raise CorpusError(f"PlatformIO executable does not exist: {candidate}")
        return str(candidate)
    discovered = shutil.which("pio")
    if discovered:
        return discovered
    local = Path.home() / ".platformio" / "penv" / "bin" / "pio"
    if local.is_file():
        return str(local)
    raise CorpusError("PlatformIO executable not found (pio or ~/.platformio/penv/bin/pio)")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fixture-root",
        type=Path,
        default=DEFAULT_FIXTURE_ROOT,
        help=f"Reviewed fixture corpus (default: {DEFAULT_FIXTURE_ROOT})",
    )
    parser.add_argument("--pio", type=Path, help="Explicit PlatformIO executable")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        fixture_root = args.fixture_root.expanduser().resolve()
        fixtures = preflight_corpus(fixture_root)
        pio = resolve_pio(args.pio)
        print(f"[CSI preflight] accepted {len(fixtures)} reviewed real fixture(s)")
        environment = os.environ.copy()
        environment["MATRIXHUB_CSI_FIXTURE_ROOT"] = str(fixture_root)
        completed = subprocess.run(
            [
                pio,
                "test",
                "-d",
                str(REPO_ROOT),
                "-e",
                "native",
                "-f",
                "test_csi_capture_replay",
            ],
            cwd=REPO_ROOT,
            env=environment,
            check=False,
        )
        return completed.returncode
    except Exception as exc:  # CLI boundary: emit one actionable failure.
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
