#!/usr/bin/env python3
from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path

BASE = Path("/Users/michal/local-agent")
ROOT = Path("/Users/michal/local-agent-v4.11.1-macos-metadata-staging")
BRANCH = "v4.11.1-macos-metadata-staging"
PYTHON = Path("/Users/michal/local-agent/.venv/bin/python")


def run(args: list[str], *, cwd: Path, env: dict[str, str] | None = None) -> None:
    print("+", " ".join(args), flush=True)
    subprocess.run(args, cwd=cwd, env=env, check=True)


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one match in {path}, found {count}")
    path.write_text(text.replace(old, new), encoding="utf-8")


if ROOT.exists():
    subprocess.run(
        ["git", "worktree", "remove", "--force", str(ROOT)],
        cwd=BASE,
        check=False,
    )
    if ROOT.exists():
        shutil.rmtree(ROOT)

run(["git", "fetch", "--quiet", "origin", BRANCH], cwd=BASE)
run(
    ["git", "worktree", "add", "--detach", str(ROOT), f"origin/{BRANCH}"],
    cwd=BASE,
)

storage = ROOT / "agent_storage.py"
replace_once(
    storage,
    '''CONTROL_RECOVERABLE_DIRS = (
    ".agent/status",
    ".agent/runs",
    ".agent/results",
    ".agent/daemon/acks",
)
GIT_NETWORK_RETRY_DELAYS = (2.0, 5.0, 15.0)
''',
    '''CONTROL_RECOVERABLE_DIRS = (
    ".agent/status",
    ".agent/runs",
    ".agent/results",
    ".agent/daemon/acks",
)
CONTROL_RECOVERABLE_UNTRACKED_BASENAMES = frozenset({".DS_Store"})
GIT_NETWORK_RETRY_DELAYS = (2.0, 5.0, 15.0)
''',
)
replace_once(
    storage,
    '''def _recoverable_control_path(path: str) -> bool:
    return any(
        path == directory or path.startswith(directory + "/")
        for directory in CONTROL_RECOVERABLE_DIRS
    )


def recover_daemon_owned_control_changes(core_module: Any) -> None:
    """Discard only interrupted daemon-owned control artifacts before sync."""
    entries = _control_status_entries(core_module)
    if not entries:
        return

    dirty = tuple(sorted({path for _code, path in entries}))
    unexpected = tuple(path for path in dirty if not _recoverable_control_path(path))
''',
    '''def _recoverable_control_path(path: str) -> bool:
    return any(
        path == directory or path.startswith(directory + "/")
        for directory in CONTROL_RECOVERABLE_DIRS
    )


def _recoverable_untracked_control_noise(code: str, path: str) -> bool:
    return (
        code == "??"
        and Path(path).name in CONTROL_RECOVERABLE_UNTRACKED_BASENAMES
    )


def recover_daemon_owned_control_changes(core_module: Any) -> None:
    """Discard interrupted daemon artifacts and explicitly allowlisted host noise."""
    entries = _control_status_entries(core_module)
    if not entries:
        return

    dirty = tuple(sorted({path for _code, path in entries}))
    unexpected = tuple(
        path
        for code, path in entries
        if not _recoverable_control_path(path)
        and not _recoverable_untracked_control_noise(code, path)
    )
''',
)
replace_once(
    storage,
    "recovering interrupted daemon-owned control changes before sync: ",
    "recovering safe control checkout changes before sync: ",
)

tests = ROOT / "tests/test_agent_storage.py"
replace_once(
    tests,
    'self.assertIn("daemon-owned control changes", core.log.call_args.args[0])',
    'self.assertIn("safe control checkout changes", core.log.call_args.args[0])',
)
replace_once(
    tests,
    '''    def test_sync_control_rejects_unexpected_dirty_paths(self) -> None:
''',
    '''    def test_sync_control_recovers_untracked_ds_store_noise(self) -> None:
        process = mock.Mock(side_effect=[
            {"exit_code": 0, "output": "?? .DS_Store\\0"},
            {"exit_code": 0, "output": ""},
            {"exit_code": 0, "output": ""},
            {"exit_code": 0, "output": "agent-control"},
            {"exit_code": 0, "output": "Already up to date."},
        ])
        core = SimpleNamespace(
            CONTROL=Path("/tmp/control"),
            CONTROL_BRANCH="agent-control",
            CONTROL_GIT_LOCK=nullcontext(),
            process=process,
            log=mock.Mock(),
        )
        storage.sync_control(core)
        self.assertEqual(
            process.call_args_list[1].args[0],
            ["git", "clean", "-fd", "--", ".DS_Store"],
        )
        self.assertIn("safe control checkout changes", core.log.call_args.args[0])

    def test_sync_control_rejects_other_untracked_host_metadata(self) -> None:
        process = mock.Mock(return_value={
            "exit_code": 0,
            "output": "?? .localized\\0",
        })
        core = SimpleNamespace(
            CONTROL=Path("/tmp/control"),
            CONTROL_BRANCH="agent-control",
            CONTROL_GIT_LOCK=nullcontext(),
            process=process,
            log=mock.Mock(),
        )
        with self.assertRaisesRegex(RuntimeError, "unexpected local changes"):
            storage.sync_control(core)
        process.assert_called_once()

    def test_sync_control_rejects_unexpected_dirty_paths(self) -> None:
''',
)
replace_once(
    tests,
    '''    def test_control_recovery_refuses_real_task_change(self) -> None:
''',
    '''    def test_control_recovery_removes_real_ds_store_noise(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            subprocess.run(["git", "init", "-q"], cwd=repo, check=True)
            noise = repo / ".DS_Store"
            noise.write_text("finder metadata\\n", encoding="utf-8")

            def process(args, cwd, **_kwargs):
                completed = subprocess.run(
                    args,
                    cwd=cwd,
                    text=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    check=False,
                )
                return {"exit_code": completed.returncode, "output": completed.stdout}

            core = SimpleNamespace(CONTROL=repo, process=process, log=mock.Mock())
            storage.recover_daemon_owned_control_changes(core)
            self.assertFalse(noise.exists())
            status = subprocess.run(
                ["git", "status", "--porcelain=v1"],
                cwd=repo,
                text=True,
                stdout=subprocess.PIPE,
                check=True,
            )
            self.assertEqual(status.stdout, "")

    def test_control_recovery_refuses_real_task_change(self) -> None:
''',
)

golden = ROOT / "docs/GOLDEN_STANDARD.md"
replace_once(
    golden,
    "- Terminal Git failures produce actionable diagnostics even when Git itself emitted no text.\n",
    "- Terminal Git failures produce actionable diagnostics even when Git itself emitted no text.\n"
    "- Control checkout recovery may remove only daemon-owned control artifacts plus explicitly allowlisted untracked host metadata (`.DS_Store`); every other unknown local change remains fatal.\n",
)

release = ROOT / "docs/RELEASE_NOTES_V4.11.1.md"
replace_once(
    release,
    "- refuses to auto-clean unexpected control changes such as tasks or daemon control requests;\n",
    "- safely removes untracked macOS Finder `.DS_Store` metadata from control checkouts while continuing to reject every other unknown local file;\n"
    "- refuses to auto-clean unexpected control changes such as tasks or daemon control requests;\n",
)

test_env = dict(os.environ)
test_env.pop("LOCAL_AGENT_LEASE_FDS", None)
test_env.pop("LOCAL_AGENT_LEASE_KEYS_DIGEST", None)

run([str(PYTHON), "-m", "py_compile", "agent_storage.py", "tests/test_agent_storage.py"], cwd=ROOT, env=test_env)
run(
    [
        str(PYTHON),
        "-m",
        "unittest",
        "-q",
        "tests.test_agent_storage",
        "tests.test_multirepo_crash_recovery",
        "tests.test_parallel_control",
        "tests.test_parallel_integration",
    ],
    cwd=ROOT,
    env=test_env,
)
run([str(PYTHON), "-m", "unittest", "discover", "-q"], cwd=ROOT, env=test_env)
run(["git", "diff", "--check"], cwd=ROOT)
run(["git", "status", "--short"], cwd=ROOT)
run(
    [
        "git",
        "add",
        "agent_storage.py",
        "tests/test_agent_storage.py",
        "docs/GOLDEN_STANDARD.md",
        "docs/RELEASE_NOTES_V4.11.1.md",
    ],
    cwd=ROOT,
)
run(["git", "commit", "-m", "Tolerate macOS Finder metadata in control checkouts"], cwd=ROOT)
run(["git", "push", "origin", f"HEAD:{BRANCH}"], cwd=ROOT)
run(["git", "rev-parse", "HEAD"], cwd=ROOT)
