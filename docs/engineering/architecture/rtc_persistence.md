Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Architecture](../README.md#runtime-and-architecture)

# RTC Persistence

This document describes the current deep-sleep persistence model used by the firmware.

## Current storage model

The system now uses three persistence layers:

### 1. Live working config

- type: `RTC::ConfigStore`
- location: PSRAM
- symbol: `RTC::store`

This is the in-memory config object used by the running firmware.

### 2. Deep-sleep shadow copy

- type: `RTC::ConfigStore`
- location: RTC memory (`RTC_NOINIT_ATTR`)
- symbol: internal `_store_backup`

This shadow copy is used to restore the PSRAM working config after deep sleep without reloading the filesystem.

### 3. Runtime-only RTC state

Separate RTC-backed structures survive deep sleep but are not persisted to `config.json`, for example:

- `RTC::sensorState`
- `RTC::heapHistory`
- `RTC::runtimeStats`
- `RTC::wifiSensingBuffer`
- `RTC::networkState`

## Source of truth by boot mode

### Cold boot

Source of truth:

- `/config/config.json`
- factory defaults if the file is missing or invalid

Cold boot path:

1. early LittleFS mount
2. `RTC::initConfig(LittleFS)`
3. `RTC::initDefaults()`
4. `CONFIG::load(LittleFS)`
5. `RTC::markValid()`

### Warm boot after deep sleep

Source of truth:

- RTC shadow copy plus runtime RTC structures

Warm boot is only considered valid when both are true:

- reset reason is `ESP_RST_DEEPSLEEP`
- the restored config passes magic/version/CRC validation

If that check fails, the firmware falls back to the cold-boot filesystem path.

## Boot-time orchestration

Relevant files:

- `src/system/init/services/StorageInitializer.cpp`
- `src/system/rtc/RtcConfigLoader.h`
- `src/system/rtc/RtcConfigLoader.cpp`
- `src/system/rtc/RtcConfig.h`
- `src/system/rtc/store/RtcConfigStore.cpp`
- `src/system/rtc/backup/RtcConfigBackup.cpp`
- `src/system/rtc/runtime/RtcRuntimeData.cpp`

Important runtime behavior:

- early LittleFS mount is skipped on warm boot
- RTC config is initialized before services that depend on settings
- config and runtime locks are created after the config path is ready

## Integrity model

### Config store

`RTC::ConfigStore` is protected by:

- `magic`
- schema version
- CRC32 over the payload

That means a firmware layout change should bump `kSchemaVersion` in `RtcConfig.h`.

### Runtime structures

Runtime structures also use validation where implemented:

- `RtcSensorState` has magic + CRC
- `RtcRuntimeStats` has magic + CRC

On warm boot, `validateRuntimeData()` checks those structures and resets them if corrupted.

## What happens before sleep

Before entering deep sleep, the firmware calls `RTC::prepareForSleep()`.

That function currently:

- recalculates CRCs for runtime sensor state
- recalculates CRCs for runtime stats
- refreshes the live config validity fields
- copies the PSRAM `ConfigStore` into the RTC shadow backup

This is why deep-sleep wake can skip filesystem reload on the fast path.

## Access patterns

### Reads

Use:

- `RTC::withConfig(...)` for normal reads under lock
- `RTC::getConfigSafeCopy()` when a full consistent copy is needed
- `RTC::getConfig()` only for fast-path reads where unlocked access is acceptable

### Writes

Preferred write path:

- `RTC::updateConfig(...)`

It handles:

- locking
- mutation
- `magic` / version / CRC refresh

Some code still mutates config directly or through `RtcStatefulService` calling `RTC::markValid()`. Because of that, `prepareForSleep()` does a final validity refresh before copying the store to RTC.

## Persistence matrix

| Data | Live location | Survives deep sleep | Survives cold boot |
| --- | --- | --- | --- |
| `ConfigStore` working copy | PSRAM | yes, via RTC shadow restore | yes, via `/config/config.json` |
| `ConfigStore` shadow copy | RTC | yes | no |
| runtime stats / sensor state / caches | RTC | yes | no |
| `/config/config.json` | LittleFS | yes | yes |
| selected backup keys like `power_cfg` | NVS | yes | yes |

## Adding new persistent config

For a new persisted config block:

1. add a POD type in `src/system/rtc/types/` or extend an existing one
2. add the field to `RTC::ConfigStore`
3. bump `kSchemaVersion`
4. initialize defaults in `RTC::initDefaults()`
5. add load/save handling in `src/config/json/*` and `ConfigManager`
6. wire the service or API update path
7. persist via `CONFIG::save(...)` when changes must survive cold boot

If the data is runtime-only and should not be written to `config.json`, keep it out of `ConfigStore` and manage it as a separate RTC structure.

## Debugging

Useful checks:

- `RTC::logStatus()` for config summary
- `RTC::wasWarmBootPathUsed()` to confirm which boot path was taken
- serial logs from `RtcLoader` and `RtcConfig`

When diagnosing persistence bugs, first determine which layer failed:

1. PSRAM working copy
2. RTC shadow/runtime restore
3. LittleFS config load/save

Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Architecture](../README.md#runtime-and-architecture)
