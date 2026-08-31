from pathlib import Path

root = Path.home() / "local-agent-v4.11.3-repository-onboarding-staging"


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f"expected text not found in {path}: {old!r}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    root / "agent_version.py",
    'RELEASE_VERSION = "4.11.2"',
    'RELEASE_VERSION = "4.11.3"',
)
replace_once(
    root / "README.md",
    "**Current release:** `v4.11.2`",
    "**Current release:** `v4.11.3`",
)
replace_once(
    root / "AGENTS.md",
    "- `MichalMatu/MatrixHub` — update root `AGENTS.md` on `main` and the active long-lived development branch when it differs; currently `develop` must stay synchronized.\n",
    "- `MichalMatu/MatrixHub` — update root `AGENTS.md` on `main` and the active long-lived development branch when it differs; currently `develop` must stay synchronized.\n"
    "- `MichalMatu/esp32_c6_zigbee` — update root `AGENTS.md` on `main` when the Local Agent task/control/resource/status/planner contract changes.\n",
)
replace_once(
    root / "docs/SESSION_BOOTSTRAP.md",
    "- `matrixhub` -> `MichalMatu/MatrixHub`.\n",
    "- `matrixhub` -> `MichalMatu/MatrixHub`;\n"
    "- `esp32-c6-zigbee` -> `MichalMatu/esp32_c6_zigbee`.\n",
)
replace_once(
    root / "docs/SESSION_BOOTSTRAP.md",
    "MatrixHub checkpoints:     ~/agent-workspace/repos/matrixhub/checkpoints\n",
    "MatrixHub checkpoints:     ~/agent-workspace/repos/matrixhub/checkpoints\n"
    "ESP32-C6 Zigbee control:   ~/agent-workspace/repos/esp32-c6-zigbee/control\n"
    "ESP32-C6 Zigbee work:      ~/agent-workspace/repos/esp32-c6-zigbee/work\n"
    "ESP32-C6 Zigbee checkpoints: ~/agent-workspace/repos/esp32-c6-zigbee/checkpoints\n",
)
replace_once(
    root / "docs/SESSION_BOOTSTRAP.md",
    "All three current registry entries use the default non-legacy workspace layout derived from their repository ids.",
    "All four current registry entries use the default non-legacy workspace layout derived from their repository ids.",
)

notes = root / "docs/RELEASE_NOTES_V4.11.3.md"
notes.write_text(
    "# local-agent v4.11.3\n\n"
    "v4.11.3 is a repository-onboarding and topology-documentation patch for the v4.11 bounded-parallel release line.\n\n"
    "## Changes\n\n"
    "- adds `MichalMatu/esp32_c6_zigbee` as the fourth registered downstream repository, using repository id `esp32-c6-zigbee`;\n"
    "- records its repository-scoped control/work/checkpoint topology under `~/agent-workspace/repos/esp32-c6-zigbee/`;\n"
    "- synchronizes the canonical downstream-documentation audit list with the new repository;\n"
    "- updates the ESP32-C6 Zigbee repository with a root `AGENTS.md` describing Local Agent task/evidence/resource and ESP-IDF hardware rules;\n"
    "- leaves scheduler behavior, task schema, resource arbitration, watchdogs, execution leases and serial fallback unchanged.\n\n"
    "The machine-local registry and new `agent-control` workspace are provisioned and verified separately as a live deployment gate before the release is considered complete.\n",
    encoding="utf-8",
)

print("PATCH_OK")
