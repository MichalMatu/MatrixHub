#!/usr/bin/env python3
from __future__ import annotations

import sys
from pathlib import Path


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected exactly one match in {path}: {count}")
    path.write_text(text.replace(old, new), encoding="utf-8")


def insert_before_once(path: Path, marker: str, block: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(marker)
    if count != 1:
        raise RuntimeError(f"expected exactly one marker in {path}: {count}")
    path.write_text(text.replace(marker, block + marker), encoding="utf-8")


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: v4112_doc_contract_alignment.py <local-agent-worktree>")
    root = Path(sys.argv[1]).resolve()

    replace_once(
        root / "agent_version.py",
        'RELEASE_VERSION = "4.11.1"',
        'RELEASE_VERSION = "4.11.2"',
    )
    replace_once(
        root / "README.md",
        "**Current release:** `v4.11.1`",
        "**Current release:** `v4.11.2`",
    )

    replace_once(
        root / "docs/AUTONOMOUS_CHAT_LOOP.md",
        "Global execution concurrency is one, so the planner should prefer one small verifiable task at a time even when multiple repositories are configured.",
        "The autonomous planner loop is sequential per active conversation goal, not globally serial. While this conversation is following a running task for that goal, do not queue another task for the same goal. The production executor may still run unrelated tasks from other conversations or repositories concurrently when resource admission permits it. Resource classification remains conservative, and repository-specific status/run/result evidence determines whether the task this conversation is following is active.",
    )

    replace_once(
        root / "docs/SESSION_BOOTSTRAP.md",
        "# Session Bootstrap: ESP32 LiteGraph + Local Agent",
        "# Session Bootstrap: Local Agent Multi-Repository Mac Deployment",
    )
    replace_once(
        root / "docs/SESSION_BOOTSTRAP.md",
        "When the established local-agent flow is requested, default to:\n\n- target: `MichalMatu/esp32s3_LiteGraph`\n- target source branch: `main`\n- target control branch: `agent-control`\n- daemon: `MichalMatu/local-agent/main`\n\nOnly treat `local-agent` itself as the product target when the request explicitly concerns the daemon/infrastructure.",
        "When the established Local Agent flow is requested, derive the target repository and source branch from the active conversation goal plus that repository's own instructions. The current machine registry contains:\n\n- `litegraph` -> `MichalMatu/esp32s3_LiteGraph`;\n- `growbox-ml-controller` -> `MichalMatu/growbox-ml-controller`;\n- `matrixhub` -> `MichalMatu/MatrixHub`.\n\nEach repository uses its own `agent-control` branch. The production daemon source is `MichalMatu/local-agent/main`. Only treat `local-agent` itself as the product target when the request explicitly concerns the daemon/infrastructure.",
    )
    replace_once(
        root / "docs/SESSION_BOOTSTRAP.md",
        "```text\n/Users/michal/Documents/PlatformIO/Projects/esp32s3_LiteGraph\n~/agent-workspace/control\n~/agent-workspace/work\n~/agent-workspace/checkpoints\n~/local-agent\n~/Library/LaunchAgents/com.michal.local-agent.plist\n~/Library/Logs/local-agent.log\n```",
        "```text\nnormal LiteGraph checkout: /Users/michal/Documents/PlatformIO/Projects/esp32s3_LiteGraph\nregistry:                  ~/Library/Application Support/local-agent/repositories.json\nLiteGraph control:         ~/agent-workspace/repos/litegraph/control\nLiteGraph work:            ~/agent-workspace/repos/litegraph/work\nLiteGraph checkpoints:     ~/agent-workspace/repos/litegraph/checkpoints\nGrowbox control:           ~/agent-workspace/repos/growbox-ml-controller/control\nGrowbox work:              ~/agent-workspace/repos/growbox-ml-controller/work\nGrowbox checkpoints:       ~/agent-workspace/repos/growbox-ml-controller/checkpoints\nMatrixHub control:         ~/agent-workspace/repos/matrixhub/control\nMatrixHub work:            ~/agent-workspace/repos/matrixhub/work\nMatrixHub checkpoints:     ~/agent-workspace/repos/matrixhub/checkpoints\ndaemon checkout:           ~/local-agent\ninstalled LaunchAgent:     ~/Library/LaunchAgents/com.michal.local-agent.plist\ndaemon stdout:             ~/Library/Logs/local-agent.log\ndaemon stderr:             ~/Library/Logs/local-agent-error.log\n```\n\nAll three current registry entries use the default non-legacy workspace layout derived from their repository ids. The loaded LaunchAgent runs `~/local-agent/agent_parallel.py --registry \"$HOME/Library/Application Support/local-agent/repositories.json\" --max-workers 2` from `~/local-agent`.",
    )
    replace_once(
        root / "docs/SESSION_BOOTSTRAP.md",
        "The repository copy of the LaunchAgent now lives at `deploy/macos/com.michal.local-agent.plist`. It contains machine-specific paths and is documentation of the established installation, not a generic installer.",
        "The production bounded-parallel LaunchAgent template is `deploy/macos/com.michal.local-agent.parallel.plist`. The installed service remains `~/Library/LaunchAgents/com.michal.local-agent.plist` with label `com.michal.local-agent`; the serial templates are rollback configurations, not additional simultaneous services.",
    )
    replace_once(
        root / "docs/SESSION_BOOTSTRAP.md",
        "4. inspect daemon status and any relevant existing run/result on `agent-control`;\n5. follow an existing active attempt instead of queuing a duplicate;\n6. derive verification from the actual diff and affected integration boundaries;\n7. prefer `efficient-verification-v1` with focused incremental verification before any broad final gate;\n8. publish only the exact validated target changes.",
        "4. inspect daemon status and any relevant existing run/result on the target repository's `agent-control`;\n5. when Chrome Chat Bridge autonomy is active, also read `docs/AUTONOMOUS_CHAT_LOOP.md` and follow one active task for the current conversation goal while allowing unrelated repository work to use normal resource-aware executor concurrency;\n6. follow an existing active attempt instead of queuing a duplicate;\n7. classify task resources conservatively before queueing work;\n8. derive verification from the actual diff and affected integration boundaries;\n9. prefer `efficient-verification-v1` with focused incremental verification before any broad final gate;\n10. publish only the exact validated target changes.",
    )

    replace_once(
        root / "docs/MULTI_REPOSITORY.md",
        "Separate ChatGPT conversations may queue tasks independently in different repositories. Each conversation still follows one active task at a time for its repository.",
        "Separate ChatGPT conversations may queue tasks independently in different repositories. An autonomous Chat Bridge conversation follows one active task at a time for its current goal; this planner sequencing does not globally serialize the executor, so unrelated repository tasks may overlap when resource admission permits it.",
    )

    insert_before_once(
        root / "docs/GOLDEN_STANDARD.md",
        "## Verification/release gate\n",
        "## Planner and Chat Bridge invariants\n\n- The Chrome Chat Bridge is wake-up/control transport only; ChatGPT remains the planner and Local Agent remains the deterministic executor.\n- One autonomous conversation follows one active task at a time for its current goal and never queues a duplicate while that task is active.\n- Planner sequencing is not global executor serialization: unrelated conversations/repositories may overlap when the parallel resource contract permits it.\n- Every bridge wake-up re-reads repository-specific status/run/result evidence before deciding whether to wait, queue one next bounded task, pause for user action or stop a completed goal.\n- Bridge `STOP`/`PAUSE` markers control the conversation loop only; they do not stop or reconfigure the Local Agent supervisor.\n\n",
    )

    insert_before_once(
        root / "docs/OPERATIONS.md",
        "## Multi-repository administration\n",
        "An autonomous Chat Bridge conversation should follow one active task at a time for its current goal. This planner-level sequencing does not reduce the production executor to global concurrency one: unrelated repositories or conversations may still overlap when their effective resources permit it. Always decide from the target repository's current status/run/result evidence.\n\n",
    )

    insert_before_once(
        root / "docs/PARALLEL_EXECUTION_PLAN.md",
        "## Automated evidence\n",
        "## v4.11.2 planner/documentation alignment\n\nThe v4.11.2 patch does not change scheduler, resource, lease or execution behavior. It closes documentation drift found after the v4.11.1 live release: the autonomous Chat Bridge contract now distinguishes per-conversation planner sequencing from global executor concurrency, and the machine-specific bootstrap records the current registry-based three-repository workspace layout plus the production parallel LaunchAgent template. The downstream LiteGraph flow/autopilot documentation is synchronized in the same release gate.\n\n",
    )

    release_notes = root / "docs/RELEASE_NOTES_V4.11.2.md"
    if release_notes.exists():
        raise RuntimeError(f"release notes already exist: {release_notes}")
    release_notes.write_text(
        "# local-agent v4.11.2\n\n"
        "v4.11.2 is a planner/documentation contract alignment patch for the v4.11 bounded-parallel release line.\n\n"
        "## Changes\n\n"
        "- corrects the autonomous Chat Bridge contract so one conversation follows one active task for its goal without falsely claiming global executor concurrency is one;\n"
        "- records that unrelated repositories/conversations may overlap when resource admission permits it;\n"
        "- updates the machine-specific Mac bootstrap to the current three-repository registry layout under `~/agent-workspace/repos/<repository-id>/`;\n"
        "- records the loaded bounded-parallel LaunchAgent topology (`agent_parallel.py --max-workers 2`) and the production `deploy/macos/com.michal.local-agent.parallel.plist` template;\n"
        "- synchronizes LiteGraph planner documentation so its workspace paths and Chat Bridge/autopilot behavior match the canonical Local Agent contract;\n"
        "- does not change task schema, resource arbitration, scheduler behavior, execution leases, watchdogs or serial fallback behavior.\n\n"
        "This patch intentionally bumps `RELEASE_VERSION` so production `main`, the installed daemon revision/version and the immutable release tag remain aligned after the canonical documentation correction.\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
