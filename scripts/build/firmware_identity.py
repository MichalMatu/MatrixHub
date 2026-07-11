"""Embed the exact Git worktree identity in ESP32 firmware builds."""

from __future__ import annotations

import subprocess
from pathlib import Path
from typing import Any, Callable


UNKNOWN_COMMIT = "unknown"


def read_firmware_identity(
    project_dir: Path,
    *,
    runner: Callable[..., subprocess.CompletedProcess[str]] = subprocess.run,
) -> tuple[str, bool]:
    """Return HEAD SHA and whether tracked/untracked worktree content is dirty."""

    project_dir = project_dir.resolve()
    try:
        head = runner(
            ["git", "rev-parse", "--verify", "HEAD"],
            cwd=project_dir,
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip().lower()
        if len(head) != 40 or any(ch not in "0123456789abcdef" for ch in head):
            return UNKNOWN_COMMIT, True

        status = runner(
            ["git", "status", "--porcelain", "--untracked-files=normal"],
            cwd=project_dir,
            check=True,
            capture_output=True,
            text=True,
        ).stdout
        return head, bool(status.strip())
    except (OSError, subprocess.SubprocessError):
        return UNKNOWN_COMMIT, True


def configure_build_environment(build_env: Any) -> None:
    project_dir = Path(str(build_env["PROJECT_DIR"]))
    commit, dirty = read_firmware_identity(project_dir)
    # Escaped quotes must survive the compiler command line and become a C
    # string literal. Unknown identity is always dirty/fail-closed.
    build_env.AppendUnique(
        CPPDEFINES=[
            ("MATRIXHUB_GIT_SHA", f'\\"{commit}\\"'),
            ("MATRIXHUB_GIT_DIRTY", 1 if dirty else 0),
        ]
    )
    suffix = "-dirty" if dirty else ""
    print(f"Firmware identity: {commit}{suffix}")


try:
    Import("env")  # type: ignore[name-defined]  # PlatformIO/SCons injects Import.
except NameError:
    env = None

if env is not None:
    configure_build_environment(env)
