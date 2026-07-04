Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Operations](../README.md#operations)

# Testing Guide

This repository currently uses two main test layers:

- PlatformIO native unit tests in `test/`
- SvelteKit frontend checks and tests in `interface/`

There is no `native_test` environment in the current repo. The active PlatformIO host-test environment is `env:native`.

## Native Unit Tests

Run the default host test suite:

```bash
pio test -e native
```

Run a specific suite:

```bash
pio test -e native -f "test_ble_device_type_detector"
pio test -e native -f "test_telegram_json_parsers"
```

Run multiple suites:

```bash
pio test -e native -f "test_ble_device_type_detector" -f "test_macro_api_service"
```

Verbose output:

```bash
pio test -e native -f "test_macro_logic" -v
```

Run the host suite with compiler coverage instrumentation:

```bash
pio test -e native_coverage
```

Generate a summarized coverage report:

```bash
bash scripts/tests/native_coverage.sh
```

## Current Native Environment

`platformio.ini` defines:

```ini
[env:native]
platform = native
test_framework = unity
```

Coverage-enabled host runs are available through:

```ini
[env:native_coverage]
extends = env:native
```

Host tests also rely on `test/stubs/` and `test/stubs/mocks/` include paths configured in `platformio.ini`.

`env:native_coverage` uses the same stubs and suite selection as `env:native`, but adds LLVM coverage instrumentation. `scripts/tests/native_coverage.sh` stores raw profiles under `.pio/build/native_coverage/profiles/`, merges them into `.pio/build/native_coverage/coverage/merged.profdata`, and prints a text summary with `llvm-cov`.

## Frontend Validation

Run the core frontend checks from `interface/`:

```bash
cd interface
npm run check
npm run test:run
npm run lint
```

Useful extras:

```bash
cd interface
npm run test:coverage
npm run size
npm run deps:check
```

## Frontend E2E (Playwright)

The frontend talks to the device through relative `/rest/...` and `/api/...` URLs. During local runs, Vite proxies those requests to the ESP32.

Install browsers once:

```bash
cd interface
npm run test:e2e:install
```

Run E2E tests:

```bash
cd interface
npm run test:e2e
```

Run against another device:

```bash
cd interface
DEVICE_URL=https://192.168.0.30 npm run test:e2e
```

Override credentials if needed:

```bash
cd interface
TEST_USERNAME=admin TEST_PASSWORD=admin npm run test:e2e
```

## Live Device Smoke Tests

Use the HTTPS/JWT smoke suite when validating a flashed device against the
backend API contract and runtime diagnostics:

```bash
python scripts/tests/device_smoke.py --device-url https://192.168.0.30 --username admin --password admin --read-only
python scripts/tests/device_smoke.py --device-url https://192.168.0.30 --username admin --password admin --safe-writes
```

The suite enforces HTTPS, uses JWT login, retries transient transport failures,
checks protected-endpoint 401 recovery, captures heap snapshots before and
after the run, and writes JSON/Markdown reports under
`artifacts/device-smoke/`.

`--safe-writes` only exercises settings endpoints with backup/restore or
disabled no-op payloads. It does not send live Telegram notifications or call
notification delivery test endpoints.

Optional checks:

```bash
python scripts/tests/device_smoke.py --device-url https://192.168.0.30 --username admin --password admin --read-only --wss
python scripts/tests/device_smoke.py --device-url https://192.168.0.30 --username admin --password admin --http-redirect
python scripts/tests/device_smoke.py --device-url https://192.168.0.30 --username admin --password admin --restart
```

Deep-sleep wake smoke is opt-in because it intentionally makes the device
unreachable for the configured sleep interval:

```bash
python scripts/tests/device_smoke.py --device-url https://192.168.0.30 --username admin --password admin --power-sleep-smoke --power-sleep-duration-ms 30000
```

The power sleep smoke backs up `/rest/power/config`, configures timer and BOOT
wake with a short timer interval, requests `/rest/sleep`, waits for the device
to return, validates a timer wake, and restores the original power
configuration.

Power config writes must keep at least one wake source enabled whenever
`sleep_enabled` is true. Direct API clients should expect
`400 input/power_wake_source_required` for sleep-enabled payloads that disable
timer, BOOT, and touch wake at the same time.

## Live Device Stress And Soak Tests

Use the stress suite after smoke passes to exercise authenticated API endpoints,
invalid payload handling, WebSocket reconnects, optional BLE/CSI loops, safe file
round-trips, WiFi recovery probes, and runtime diagnostics drift checks:

```bash
python scripts/tests/stress_test.py --device-url https://192.168.0.30 --username admin --password admin --cycles 100 --delay 0.01
```

The suite writes JSON/Markdown reports under `artifacts/stress/`. It enforces
HTTPS, logs in through JWT, retries login rate limiting, captures before/after
heap/task/mutex/log snapshots, and fails on unexpected restarts, WebSocket queue
drops, growing lock timeout counters, or panic/watchdog/brownout markers in the
device log tail.

Live Telegram delivery is intentionally excluded from the stress path. The
suite may read notification settings, but it must not call delivery/test-send
endpoints against shared devices. Macro execution is also opt-in because USB HID
activity can affect the host:

```bash
python scripts/tests/stress_test.py --device-url https://192.168.0.30 --username admin --password admin --cycles 100 --delay 0.01 --macro-loop
```

Use the soak suite to measure longer runtime stability, heap/fragmentation drift,
task stack watermarks, lock counters, WebSocket drops, and restart counters:

```bash
python scripts/tests/soak_test.py --device-url https://192.168.0.30 --username admin --password admin --duration 1h --interval 10s --plot
```

The soak suite writes CSV, JSON, and Markdown reports under `artifacts/soak/`.
For release evidence, keep the generated Markdown report together with the
firmware build hash and the smoke/stress reports from the same run.

## Release Build And Flash

Use the full release build when UI assets or embedded frontend output matter:

```bash
/home/test/.platformio/penv/bin/pio run -e waveshare_esp32s3_matrix
```

Use the backend-only build for a quick repeat after a successful full build:

```bash
SKIP_UI=1 /home/test/.platformio/penv/bin/pio run -e waveshare_esp32s3_matrix
```

Upload only after stopping any serial monitor:

```bash
pkill -f "pio device monitor" || true
/home/test/.platformio/penv/bin/pio run -e waveshare_esp32s3_matrix -t upload
```

Open the monitor on the detected USB CDC port, for example:

```bash
/home/test/.platformio/penv/bin/pio device monitor --port /dev/ttyACM0
```

## Release Checklist

Run this checklist from a clean worktree or record any intentional local changes
before starting.

1. Build and size:

```bash
/home/test/.platformio/penv/bin/pio run -e waveshare_esp32s3_matrix
SKIP_UI=1 /home/test/.platformio/penv/bin/pio run -e waveshare_esp32s3_matrix
/home/test/.platformio/penv/bin/pio run -e waveshare_esp32s3_matrix -t size
```

Expected result: firmware fits the `app0` partition from
`partitions/partitions_s3.csv`; current release builds should leave meaningful
headroom below `3,502,080` bytes.

2. Host and frontend gates:

```bash
/home/test/.platformio/penv/bin/pio test -e native
cd interface && npm run quality:frontend
```

3. API contract:

```bash
python scripts/contract/verify_api_contract.py
```

4. Runtime diagnostics and smoke:

```bash
python scripts/diagnostics/check_runtime_diagnostics.py --device-url https://192.168.0.30 --username admin --password admin
python scripts/tests/device_smoke.py --device-url https://192.168.0.30 --username admin --password admin --safe-writes
python scripts/tests/device_smoke.py --device-url https://192.168.0.30 --username admin --password admin --power-sleep-smoke --power-sleep-duration-ms 30000
```

5. Stress and soak:

```bash
python scripts/tests/stress_test.py --device-url https://192.168.0.30 --username admin --password admin --cycles 100 --delay 0.01
python scripts/tests/soak_test.py --device-url https://192.168.0.30 --username admin --password admin --duration 1h --interval 10s --plot
```

6. Optional feature toggles:

```bash
python scripts/tests/feature_toggle.py --device-url https://192.168.0.30 --username admin --password admin --feature ble
python scripts/tests/feature_toggle.py --device-url https://192.168.0.30 --username admin --password admin --feature wifisensing
```

7. Evidence:

- save the smoke/stress/soak Markdown reports from `artifacts/`,
- record firmware flash/RAM size,
- record any skipped optional checks and why,
- never commit secrets, tokens, live logs with credentials, `node_modules`, or
  generated build directories.

Release passes only when all required gates pass, runtime counters stay stable,
and the device shows no unexpected restart, panic, watchdog, brownout, or lock
timeout growth. The Vite large-chunk warning is acceptable only while
`npm run size` and `npm run quality:frontend` stay within budget.

## Test Layout

Native tests live under `test/test_<module>/`.

Examples currently in the repo:

- `test/test_ble_device_type_detector/`
- `test/test_macro_api_service/`
- `test/test_telegram_command_parser/`
- `test/test_wifi_sensing_config_json/`

Frontend tests live next to the code they validate, for example:

- `interface/src/routes/.../*.test.ts`
- `interface/src/routes/.../*.test.svelte.ts`
- `interface/src/lib/services/api/...*.test.ts`

## Adding New Native Tests

1. Create a directory like `test/test_my_module/`.
2. Add a Unity test file such as `test/test_my_module/test_my_module.cpp`.
3. Use `#include <unity.h>`.
4. Run it with:

```bash
pio test -e native -f "test_my_module"
```

## Adding New Frontend Tests

- Prefer Vitest for hooks, services, parsers, and UI logic.
- Keep tests close to the feature under `interface/src/...`.
- Use `.test.svelte.ts` when the test exercises Svelte runes/stateful hooks.

Run a focused frontend suite with Vitest:

```bash
cd interface
npm run test -- useWifiManagement
```

## Reading a Coredump

The `coredump` partition (64 KiB at `0x3E7000`) is written automatically when
the firmware panics. To decode the most recent crash into a readable backtrace:

```bash
python scripts/diagnostics/decode_coredump.py
```

This pulls the partition over USB using `esp-coredump` (bundled with the
PlatformIO penv) and resolves symbols against `.pio/build/waveshare_esp32s3_matrix/firmware.elf`
or the `build/elf/latest.elf` symlink maintained by `scripts/build/save_elf.py`.

Common options:

```bash
python scripts/diagnostics/decode_coredump.py --port /dev/cu.usbmodem101
python scripts/diagnostics/decode_coredump.py --elf build/elf/<hash>.elf
python scripts/diagnostics/decode_coredump.py --save crashlogs/2026-05-21.bin
```

Use `--save` when reporting an incident; it stores the raw partition image
and a snapshot of the matching ELF side-by-side so the trace can be re-decoded
later even after the firmware ELF rotates.

The script never writes to the device; the coredump partition is only erased
on the next panic or after an explicit re-flash.

## Troubleshooting

If a native test does not run:

1. Check that its directory exists under `test/`.
2. Check the filter name passed to `-f`.
3. Check whether the suite is listed in `test_ignore` for `env:native`.

If a native test fails to compile because it uses ESP32-only APIs:

1. Add or extend stubs in `test/stubs/`.
2. Separate logic from hardware-specific code.
3. Only add to `test_ignore` when stubbing is not worth the effort.

If frontend E2E fails immediately:

1. Verify the ESP32 is reachable on the expected `DEVICE_URL`.
2. Verify the login credentials.
3. Verify `interface/` dependencies and Playwright browsers are installed.

## Recommended Pre-Commit Validation

For backend-only changes:

```bash
pio test -e native
```

For frontend-only changes:

```bash
cd interface
npm run check
npm run test:run
```

For cross-cutting frontend/backend changes:

```bash
pio test -e native
cd interface && npm run check
cd interface && npm run test:run
```

Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Operations](../README.md#operations)
