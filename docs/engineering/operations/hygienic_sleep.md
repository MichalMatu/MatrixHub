Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Operations](../README.md#operations)

# Hygienic Sleep

This document describes the current hygiene-sleep implementation, not a wishlist of possible optimizations.

In this project, "hygienic sleep" means a very short deep-sleep cycle used to reset DRAM-heavy runtime state while preserving RTC-backed configuration and diagnostics.

## Why it exists

The main reason is heap health:

- internal RAM fragmentation can make TLS and other allocations unreliable
- a short deep-sleep cycle clears volatile runtime state
- RTC-backed configuration, counters, and caches survive the sleep/wake cycle

This is part of the normal power-management path. It is not a separate reboot mechanism.

## Entry points

### Manual trigger

Admin HTTP endpoint:

- `POST /rest/power/hygieneSleep`

Current behavior:

- the handler replies `200`
- waits briefly so the HTTP response can flush
- marks `RTC::runtimeStats.hygieneSleepActive`
- applies a one-shot timer wake override of `100ms`
- requests sleep through `PowerManager`

The one-shot override is runtime-only. It does not change the user's persistent
timer, BOOT, or touch wake-source settings.

### Automatic trigger

`HeapMonitor::checkHygieneConditions()` can request hygiene sleep when:

- the largest internal free block drops below the configured threshold
- or fragmentation stays too high for a sustained period

Automatic hygiene sleep also:

- records `ShutdownReason::HYGIENE_SLEEP` in `BootTracker`
- increments `RTC::runtimeStats.hygieneSleepCount`
- updates `RTC::runtimeStats.lastHygieneSleepMs`

### Related maintenance sleep

The `hygieneSleepActive` flag is also reused by some maintenance/recovery paths, for example thermal emergency cooling sleep. Do not treat that flag as "heap fragmentation only".

## Actual sleep flow

Once sleep is requested, the path is:

1. `PowerSleepController::requestSleep(...)`
2. `PowerSleepController::enterDeepSleep(...)`
3. sleep-entry failsafe task is armed
4. `SleepService::executeSleepCallbacks()`
5. pre-sleep hook from `ShutdownSequence::execute()`
6. wake-source configuration
7. `RTC::prepareForSleep()`
8. `esp_deep_sleep_start()`

`ShutdownSequence` currently does the heavy lifting before the chip actually sleeps.
If shutdown cleanup hangs for the configured failsafe window, the failsafe
prepares wake sources and RTC state, then forces deep sleep so unattended
devices do not remain awake indefinitely.

## What `ShutdownSequence` stops

Before sleep, the shutdown path currently stops or soft-stops:

- `NotificationWorker`
- `WifiSensingService`
- `AirMouseService`
- `MacroService`
- `SensorLoggingTask`
- `ShellyService`
- `MatrixTask`
- `ThermalMonitor`
- BLE via `BleService::prepareForSleep()`
- Wi-Fi radio via `WiFi.mode(WIFI_OFF)`

It also persists RTC/boot state and flushes serial output.

## What is intentionally *not* done

Notably:

- no dedicated pre-sleep `TelegramClient::disconnect()` step
- no explicit `server->stop()` during shutdown
- no `MDNS.end()` in the shutdown path
- no `Wire.end()` before deep sleep
- no full NimBLE deinit before sleep

Those omissions are intentional. The current code prefers a reliable, non-blocking shutdown path over aggressive cleanup that previously caused hangs or instability.

## State that survives

Hygiene sleep relies on RTC-backed state surviving the cycle, including:

- configuration stored in `RTC::ConfigStore`
- runtime stats such as `hygieneSleepActive`, `hygieneSleepCount`, `telegramLastUpdateId`
- heap history and diagnostics
- BLE / sensor / network caches kept in RTC structures
- boot-tracking metadata

## Practical validation

Minimal validation flow:

1. call `POST /rest/power/hygieneSleep`
2. observe a short sleep/wake cycle on serial logs
3. confirm the device returns quickly
4. inspect counters such as `RTC::runtimeStats.hygieneSleepCount` or health output after wake

If hygiene sleep is failing, the most likely problems are in the shutdown path, wake-source configuration, or RTC persistence, not in Telegram/Webhook-specific cleanup.

Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Operations](../README.md#operations)
