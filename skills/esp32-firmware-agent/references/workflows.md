# Workflows

## When To Read This File

- Read this file for flashing, serial monitoring, frontend checks, cleanup, or crash diagnostics.
- The maintained workflow docs live in `docs/agent-workflows/`. Use those files as the source of truth when this skill and `AGENTS.md` disagree.

## Validation Rule

- If a firmware change affects runtime behavior on hardware, prefer `pkill -f monitor`, then `pio run -t upload`, then inspect logs with `pio device monitor`.
- For review-only, host-only, or frontend-only work, use the narrowest validation that fits the task.

## Common Commands

- Kill monitor: `pkill -f monitor`
- Fast developer build: `./scripts/build-fast.sh`
- Clean main env: `./scripts/build-clean.sh`
- Rebuild diagnostics: `./scripts/build-explain.sh`
- Upload firmware: `pio run -e waveshare_esp32s3_matrix -t upload`
- Device monitor: `pio device monitor`
- Backend tests: `pio test -e native`

## Frontend Diagnostics

1. `cd interface`
2. `npm run lint`
3. `npm run check`
4. `npm run test:run`
5. `npm run size`
6. `npm run deps:check`

Notes:

- Frontend API clients live under `interface/src/lib/services/api/`.
- Shared DTO and type definitions live under `interface/src/lib/types/`.
- Settings pages commonly compose around `interface/src/lib/utils/api/useSettings.svelte.ts`.
- Firmware build and upload run `pre:scripts/build/build_interface.py`, so frontend changes can affect embedded assets and final firmware size.

## Clean Builds

Prefer `./scripts/build-clean.sh` for firmware cleanup because it preserves the
PlatformIO cache. Delete `.pio/cache` only when measuring cold-build behavior or
repairing corrupted package state.

For frontend cleanup:

1. `cd interface`
2. `rm -rf build .svelte-kit`
3. `npm install` only when dependencies changed or `node_modules` is corrupted.

## Core Dump Diagnostics

Use this flow when a Task Watchdog triggers or the device crashes.

1. Identify the serial port:
   ```bash
   ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
   ```
2. Read the core dump from flash:
   ```bash
   python scripts/diagnostics/decode_coredump.py --save crashlogs/latest.bin
   ```
3. If manual extraction is needed, the active coredump partition is 64 KiB at `0x3E7000`:
   ```bash
   python3 -m esptool --port /dev/ttyACM0 --baud 921600 read_flash 0x3E7000 0x10000 coredump_latest.bin
   ```
4. Decode with the PlatformIO GDB package when the helper cannot be used:
   ```bash
   /home/test/.platformio/packages/tool-xtensa-esp-elf-gdb/bin/xtensa-esp32s3-elf-gdb --batch -ex "info threads" -ex "thread apply all bt" .pio/build/waveshare_esp32s3_matrix/firmware.elf coredump.elf > coredump_report.txt
   ```
