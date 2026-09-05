# AGENTS.md

These instructions apply to the whole repository.

## Role And Language

- Work as an ESP32-S3 firmware and SvelteKit UI agent for MatrixHub.
- Chat with the user in Polish. Keep code, identifiers, comments, logs, commit
  messages, and commands in English unless the existing file uses Polish.
- Target board/env: Waveshare ESP32-S3 Matrix, PlatformIO env
  `waveshare_esp32s3_matrix`.
- The firmware uses the Arduino framework on the pioarduino ESP32 platform, plus
  ESP-IDF/FreeRTOS APIs where the codebase already uses them.
- The UI is in `interface/` and is embedded into firmware through
  `scripts/build/build_interface.py`.

## Hard Rules

- Do not modify `lib/framework/` unless the user explicitly asks for framework
  changes or the change is unavoidable and explained.
- Do not modify `src/wifisensing/csi/vendor/` or the prebuilt CSI vendor library
  unless the user explicitly asks for vendor work.
- Do not change firmware logic when the task is about build speed, tooling,
  documentation, or PlatformIO workflow.
- Respect the existing dirty tree. Do not revert user changes or generated files
  you did not intentionally touch.
- Keep edits scoped. Prefer existing local patterns over new abstractions.
- For host-only code, use the `native` env and keep it under `src/native/` or
  `test/`. `src/native/**` is intentionally excluded from the ESP32 env.

## Local Agent Workflow

This repository is hard-bound as:

- repository id: `matrixhub`;
- repository: `MichalMatu/MatrixHub`;
- agent binding: `033327ab-700d-43b4-9b3b-caff1acaa2c7`;
- control branch: `agent-control`.

Use this repository's own `agent-control` branch for tasks and evidence; never route MatrixHub work through another repository's control branch.

If Chat Bridge is active, the wake envelope must identify exactly `LA_AGENT=033327ab-700d-43b4-9b3b-caff1acaa2c7`, `LA_REPO=matrixhub`, and `LA_REPOSITORY=MichalMatu/MatrixHub`. Never infer or switch repositories from conversation history. A different repository requires explicit Bridge **Rebind**.

- Inspect `.agent/binding.json` and `.agent/status/daemon.json` on `agent-control` before queueing work when Local Agent compatibility matters. The control binding must match the identity above.
- Treat `daemon_version`, `self_revision`, `execution_model` / `execution_variant`, current task state, and `supervisor_pid` as repository-worker truth. Supervisor-wide fields such as `max_parallel_workers` are not guaranteed to be repeated in every repository-worker snapshot; read the shared supervisor status when that field matters.
- Queue immutable tasks under `.agent/tasks/<task-id>.json`; every executable task must contain exactly `"agent_binding": "033327ab-700d-43b4-9b3b-caff1acaa2c7"`. Follow `.agent/runs/<task-id>.json` and read `.agent/results/<task-id>.json` before reporting completion.
- Local Agent requires local registry binding == `.agent/binding.json` binding == task `agent_binding` before claim/execution. Missing repository binding reports `unbound`; a control mismatch reports `binding_error`; missing/wrong task binding is terminally rejected before any task command runs. The serial fallback enforces the same contract.
- Set `work_branch` explicitly. Follow MatrixHub Git policy: normal development is on `develop`; `main` is the stable release line unless the user explicitly requests another branch.
- One MatrixHub task executes at a time, but another registered repository may overlap when resource admission permits it.
- Every task must declare `resources` explicitly; missing, malformed, duplicated, oversized, or non-canonical declarations are terminal task-contract errors with no compatibility fallback.
- Use `resources: []` for repository-local software work, including PlatformIO builds/tests, when no exclusive external device or host-global state is used. `memory_limit_mb` is an independent RSS watchdog and does not determine resource classification.
- Use stable named resources such as `board:matrixhub-s3` for USB, serial, upload/flash, monitor, and hardware work so only tasks sharing that concrete resource serialize.
- Use `resources: ["machine"]` only for genuine whole-host operations such as global Local Agent maintenance or host-global toolchain mutation. Resource contention is a wait state and must continue with `NEXT`, not `STOP`.
- Repository workers must not perform supervisor-wide restart/self-update. The production Local Agent runtime lives on `MichalMatu/local-agent/main`; `agent_multirepo.py` remains the serial fallback.
- Binding failures are safety evidence, not routing hints. Do not guess, rotate, or borrow another repository's binding to make a task execute.
- A successful Local Agent result proves local execution/verification, not source publication. Commit/push according to this repository's Git rules as an explicit final gate.

When the Local Agent task/control/resource/status/binding contract changes, update this section together with the canonical `MichalMatu/local-agent` documentation rather than relying on remembered chat context.

## Build Workflow

Prefer the helper scripts because they handle the local PlatformIO path and the
optimized build settings.

| Area | Command | Notes |
| --- | --- | --- |
| Fast firmware build | `./scripts/build-fast.sh` | Preferred iteration path. Uses `waveshare_esp32s3_matrix_dev`, skips UI, defaults to `-j4`. |
| Clean main env | `./scripts/build-clean.sh` | Cleans only `waveshare_esp32s3_matrix`; the next build should reuse `.pio/cache` when signatures match. |
| Rebuild diagnosis | `./scripts/build-explain.sh` | Writes `build-explain.log` using SCons `--debug=explain`. |
| Native tests | `pio test -e native` | Use `$HOME/.platformio/penv/bin/pio` if `pio` is not in PATH. |
| Full release build | `pio run -e waveshare_esp32s3_matrix` | Builds UI and firmware. Use for final artifacts or UI embed changes. |
| Upload | `pio run -e waveshare_esp32s3_matrix -t upload` | Stop any active monitor first. |
| Monitor | `pio device monitor` | Use after flashing to inspect runtime logs. |

Build speed facts measured on the Raspberry Pi 5 host:

- A cold full build can take about 15 minutes.
- A normal no-change build is about 38-40 seconds.
- With `.pio/cache` warmed, recreating a clean build directory is about
  50 seconds instead of recompiling everything.
- Do not run repeated cold clean builds unless the task specifically requires
  timing cache invalidation or PlatformIO/SCons behavior.
- The main env uses `lib_ldf_mode = chain`. Do not switch to `deep` or `deep+`
  before checking missing includes and explicit `lib_deps`.

Detailed build notes:

- `BUILD_SPEED_OPTIMIZATION.md`
- `docs/main_docs/BUILD_SPEED_OPTIMIZATION.md`

Detailed agent workflows:

- `docs/agent-workflows/README.md`
- `docs/agent-workflows/build.md`
- `docs/agent-workflows/clean.md`
- `docs/agent-workflows/core_dump.md`
- `docs/agent-workflows/frontend-diagnostics.md`
- `docs/agent-workflows/language_preference.md`
- `docs/agent-workflows/new_module.md`
- `docs/agent-workflows/tests_backend.md`

## Project Shape

Important entry path:

- `src/main.cpp`
- `src/system/Application.cpp`
- `src/system/init/core/InitSequence.cpp`
- `src/system/services/ServiceRegistry*.cpp`
- `src/system/init/services/*Initializer.*`

Configuration and state:

- `src/config/System.h` is the aggregate include for system config.
- `src/config/TaskConfig.h` owns task stack sizes, priorities, core affinity,
  timeouts, and task monitoring thresholds.
- `src/config/App.h`, `src/config/Hardware.h`, and `src/config/Network.h` hold
  app, board, hardware, and network constants.
- Runtime JSON config serialization lives in `src/config/json/`.
- RTC-backed persistent structs live in `src/system/rtc/types/`.
- Increment `RTC::kSchemaVersion` in `src/system/rtc/RtcConfig.h` when changing
  RTC struct layout.

Core module directories:

- `src/api/`: HTTP and WebSocket API controllers.
- `src/system/`: boot, lifecycle, service registry, logging, memory, power,
  watchdog, health, RTC, and shutdown.
- `src/notifications/telegram/`: Telegram client, commands, queue, polling, and
  worker runtime.
- `src/udp/`: UDP pusher and settings.
- `src/wifisensing/`: RSSI and CSI sensing.
- `src/ble/`: BLE facade, scanning, parsing, and BLE settings.
- `src/alarms/`, `src/shelly/`, `src/sensors/`, `src/matrix/`,
  `src/keyboard/`, `src/macros/`, `src/airmouse/`, `src/usb_terminal/`, and
  `src/compensation/`: domain services and settings.
- `interface/src/`: SvelteKit UI routes, services, stores, components, types,
  and tests.

## Architecture Rules

- Services are owned by `ServiceRegistry` and wired during boot through the
  initializer classes under `src/system/init/services/`.
- Keep API classes in `src/api/*` transport-focused: parse requests, call
  services, and serialize responses. Put business logic in the domain service.
- Prefer explicit dependency injection through `begin()` or constructors. Avoid
  adding new global `extern` dependencies.
- Register new long-lived services in `ServiceRegistry` and document ownership
  or shutdown implications when they matter.
- Keep task stack sizes, priorities, and core choices in `CONFIG::TASKS`; do not
  hardcode `xTaskCreate*` parameters inline.
- Use `/rest/...` for framework/system-style endpoints and `/api/...` for domain
  endpoints, matching existing routing.

## Memory And Concurrency

- Default to PSRAM for large non-DMA, non-ISR buffers. As a rule of thumb,
  temporary user-space buffers over 512 bytes should use PSRAM.
- Use internal DRAM for DMA buffers, ISR-accessed data, and latency-critical
  small structures.
- Use `SYSTEM::SpiRamJsonDocument` for large JSON documents and stream large API
  responses in chunks instead of building huge `String` objects.
- Use `heap_caps_free()` for memory allocated with `heap_caps_malloc()`.
- Avoid long-lived Arduino `String` objects in services, large buffers, and
  persistent data. Prefer fixed `char[]`, `std::string` with project allocators,
  or existing PSRAM helpers.
- ISR callbacks must not allocate, block, log heavily, or take mutexes. Use
  fixed-size data and FreeRTOS ISR-safe queue calls.
- For CSI, keep the callback path minimal and pass `CsiPacket` data to worker
  tasks through the existing queue path in `src/wifisensing/csi/data/`.

## UI And Generated Files

- `scripts/build/build_interface.py` already avoids rewriting
  `lib/framework/core/WWWData.h` when content is unchanged. Preserve that
  behavior.
- If adding a generator, write generated files only when bytes actually differ.
  Avoid timestamp-only headers that invalidate half the firmware.
- Use `custom_skip_ui = yes` or `SKIP_UI=1` only for developer firmware
  iteration. Full release builds must embed the UI.

## Testing Guidance

- For firmware logic that can run on host, add or update `pio test -e native`
  coverage.
- For ESP32-only behavior, keep tests focused and explain what was verified by
  build, logs, or static inspection.
- USB Terminal `target_port: "auto"` has been verified end-to-end on
  Linux/Raspberry Pi. When working on macOS, re-test the auto port detection on
  real hardware. Windows and Android are not covered by the current POSIX shell
  auto-detection path and need separate host-mode verification or implementation.
- For UI changes, follow the existing `interface/` test/build workflow. Do not
  rebuild the UI during firmware-only work unless the UI embed is part of the
  task.

## Git Notes

- Never create additional branches unless the user explicitly asks for a new
  branch. Commit directly on the current branch when the user asks for a commit.
- Keep `main` as the stable release line only. Do normal development on
  `develop`, then release by merging the completed work to `main`, bumping the
  version, running a full release build, tagging `vX.Y.Z`, and pushing both the
  branch and tag.
- Keep commits focused and do not include unrelated user changes.
- This repo may be ahead of `origin/main` if GitHub HTTPS authentication is not
  configured. If `git push` fails with a username/auth error, report it instead
  of rewriting remotes or history.

### Direct GitHub work and local execution

Use an available GitHub tool with write permission for bounded source/configuration/documentation changes when the exact diff and relevant CI can verify the outcome. A commit proves publication, not successful execution. Report the exact commit and completed checks. Do not create an artificial Local Agent task when GitHub evidence already provides the required verification.

Use Local Agent for Mac command execution, local builds/tests, device access and machine-specific evidence. A hybrid flow may edit through GitHub and run a read-only local verification task for the exact committed SHA; verify that SHA explicitly in an early stage (`expected_head` is not a supported task field). Check current daemon/run evidence before a direct write and avoid racing a local task that is modifying the same branch. Follow this repository's branch policy.

Local tasks retain their unique immutable ids, exact `agent_binding`, explicit `resources`, bounded limits and terminal result requirement. When Chat Bridge is active, both paths remain confined to its immutable repository binding. Use `STOP` only after the goal has the required CI or local result evidence. A different repository requires explicit operator Rebind. Canonical policy: `MichalMatu/local-agent/main/docs/AUTONOMOUS_CHAT_LOOP.md` and `docs/OPERATIONS.md`.
