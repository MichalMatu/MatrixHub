# MatrixHub Logging Golden Standard

This document defines the firmware logging policy used by MatrixHub runtime
code and the admin live tail.

## Runtime Flow

Normal firmware code must use `LOGE`, `LOGW`, `LOGI`, `LOGD`, `LOGV`, or
`LOG::Logging::log(level, tag, ...)`.

Boot starts at an emergency-only startup level until Phase 1 loads the persisted
RTC/FS logging config. The loaded level is applied before RTC diagnostics and
the rest of boot continue, so live tail filtering is consistent even from a
restart captured at boot.

The central path is:

1. `LOG*` formats a bounded stack message.
2. `LOG::Logging::log()` writes to ESP-IDF output with `esp_log_write()`.
3. The same message is appended to `LOG::RingBuffer`.
4. `/rest/logs/tail` streams the ring buffer.
5. The Svelte live tail polls with `since` and keeps a compact local tail.

Do not use `Serial.*`, `USBSerial.*`, `printf`, or `ESP_LOG*` in normal runtime
modules. If a log is useful to an operator, it should be visible through the
central ring buffer and live tail.

## Level Policy

- `error`: a feature cannot continue, data may be lost, or restart/sleep/fault
  handling is being requested.
- `warn`: degraded behavior, retry/backoff, missing dependency, mutex timeout,
  invalid config, network failure, parse failure, or recoverable hardware issue.
- `info`: service start/stop, major config/state transitions, user-requested
  actions, recovery after repeated failures, and notification/alarm lifecycle.
- `debug`: diagnostic decisions, state changes, one-shot setup details, and
  throttled periodic summaries.
- `verbose`: protocol/raw IO, repeated connection state details, per-cycle
  scheduler details, and data useful only while actively tracing a subsystem.

Routine success loops such as "sent OK", "poll OK", "scan started", "no new
updates", and sensor samples must not be `info`.

## Noise Controls

Use the shared helpers from `src/system/logging/Logging.h`:

- `LOG*_THROTTLED(intervalMs, ...)` for repeated success/failure messages. The
  helper appends a suppressed-repeat count when messages were skipped.
- `LOG*_ON_CHANGE(value, ...)` for logs that should appear only when a simple
  state value changes.
- Existing `LOG_PROFILE_*` and `LOG_STACK_*` helpers for timing and stack data.

Prefer state-change logging over periodic logging when the underlying state is
stable. If a periodic summary is needed, use the existing jittered intervals in
`TASK_MONITOR` so multiple modules do not log at the same millisecond.

## Allowed Direct Exceptions

The following direct calls are intentional and should stay small:

- `src/system/logging/Logging.cpp`: `esp_log_write()` is the central logger's
  ESP-IDF output sink.
- `src/system/logging/LogRingBuffer.cpp`: `ESP_LOGE()` reports logger storage
  initialization failures when the ring buffer itself is unavailable.
- `src/system/logging/LogOutput.cpp`: `USBSerial.write()` and
  `esp_log_set_vprintf()` implement USB log transport and boot-log replay.
- `src/system/init/core/MemoryConfig.cpp`: constructor-priority PSRAM checks run
  before normal system initialization and may abort before `Logging::begin()`.
- `src/system/restart/RuntimeRestart.cpp`: emergency restart paths log directly
  because the process may already be unhealthy.
- `src/system/memory/SystemAllocator.h` and `src/system/memory/PsramAllocator.h`:
  allocator failures avoid the central ring path to reduce recursion risk while
  memory is already failing.

Any new `ESP_LOG*`, `Serial.*`, `USBSerial.*`, or `printf` in `src/` must either
fit one of these categories or update this exception list with a reason.

## Module Tag Policy

| Area | Tags | Default behavior |
| --- | --- | --- |
| Boot/system | `Main`, `App`, `Init`, `Boot`, `Shutdown` | `info` for phases and lifecycle, `debug` for detailed timing when diagnostics are enabled. |
| Logging/live tail | `LOG`, `LogTailHandler` | Internal exceptions only; API tail should not log per line. |
| RTC/config | `RtcService`, `RtcStore`, config service tags | warnings on lock/config failures, debug only for one-shot diagnostics. |
| Sensors | `TelemetryLo`, `SCD4x`, `SensorBin` | warnings with suppression for read failures, throttled debug for samples and writes. |
| Alarms | `AlarmCoord`, `AlarmService`, `AlarmSettings` | `info` for trigger/clear notifications, `debug` only on state changes or reminders. |
| BLE | `BleLife`, `BleProc`, `BLE`, `BleScanner` | `info` for service/config transitions, throttled debug for scans and telemetry. |
| WiFi sensing/CSI | `WifiSense`, `CsiService`, CSI worker tags | `info` for start/stop/config, debug on motion state changes and throttled performance. |
| Shelly | `Shelly`, `ShellyWork`, `ShellyCtrl` | `info` for relay commands and persistent reachability changes, throttled debug for poll success. |
| Notifications | `TgWorker`, `TgTls`, `TgQueue`, `Pushover`, `WebHook` | `info` for worker lifecycle and recovery, throttled debug for success paths, warn/error for delivery failures. |
| Power/thermal | `Power`, `PowerApi`, `Thermal` | `info` for user/system state transitions, debug for noisy activity sources. |
| WebSockets/API | `Ws*`, `ChannelSubs`, API tags | warnings/errors for auth/queue failures, debug for connect/disconnect/subscription events. |
| Matrix/UI device output | `Matrix*`, `MatrixMenu` | debug for content/layer changes, no per-frame stable-state logs. |

## Live Tail Expectations

At `info`, live tail should show boot/service lifecycle, user actions, important
state transitions, recovery, warnings, and errors.

At `debug`, live tail should add diagnostic state changes and throttled periodic
summaries without being dominated by stable success loops.

At `verbose`, protocol and raw transport details may be noisy, but they should
still avoid unbounded hot-loop logging.

The current UI polls `/rest/logs/tail` with `since`, deduplicates exact repeated
entries, supports pause/copy/clear, and requests a capped tail window. Firmware
is responsible for keeping repeated source messages throttled before they enter
the ring buffer.

## Audit Commands

Use these checks after logging changes:

```sh
rg -n "\bESP_LOG[EWIDV]\s*\(|esp_log_write\s*\(|esp_log_set_vprintf\s*\(|Serial\.(print|println|printf|write)\s*\(|USBSerial\.(print|println|printf|write)\s*\(|\bprintf\s*\(|\bvprintf\s*\(|\bfprintf\s*\(|\bputs\s*\(" src -g '!src/wifisensing/csi/vendor/**' -g '!lib/framework/**'
rg -n "LOGD\(|LOGV\(" src -g '!src/wifisensing/csi/vendor/**' -g '!lib/framework/**'
./scripts/build-fast.sh
pio test -e native
```
