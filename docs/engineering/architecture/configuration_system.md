Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Architecture](../README.md#runtime-and-architecture)

# Configuration System

This project has two configuration layers:

- compile-time constants in `src/config/*.h`
- runtime configuration loaded from `/config/config.json` into the RTC/PSRAM-backed config store

This document describes how those layers are split and how code should use them.

## Compile-time headers

The core configuration headers are:

```text
src/config/
├── App.h
├── Hardware.h
├── Network.h
├── System.h
├── TaskConfig.h
└── json/*.h / json/*.cpp
```

### `App.h`

Use for:

- global JSON keys in `CONFIG::Keys`
- factory defaults
- application-level constants
- notification / macro / compensation / feature constants

This file is also where the persisted JSON schema names are defined, so changing a key here can affect backend persistence and frontend compatibility.

### `Hardware.h`

Use for:

- GPIO mappings
- board-specific hardware constants
- thermal thresholds
- low-level hardware-related timing or safety constants

### `Network.h`

Use for:

- Wi-Fi / HTTP / auth constants
- API paths shared across firmware
- BLE UUIDs and radio/network constants

### `System.h`

Use for:

- aggregate inclusion of the config headers

### `TaskConfig.h`

Use for:

- task stacks, priorities, and core affinity
- system timeouts
- task monitoring thresholds

## Include rule

Include the specific header you need where practical. Use `System.h` when a
translation unit genuinely needs the aggregate config view.

Example:

```cpp
#include "config/Hardware.h"
#include "config/System.h"
```

## Runtime configuration

### Persisted file

The canonical runtime config file is:

- `/config/config.json`

`CONFIG::save()` writes atomically using:

- `/config/settings.tmp`
- `/config/settings.bak`

So the runtime config path is no longer `settings.json` in the project root.

### Config manager

`src/core/config/ConfigManager.cpp` is the central JSON orchestrator.

It:

- reads the config file into a PSRAM-backed `JsonDocument`
- dispatches each section to `src/config/json/*`
- rebuilds the full JSON document on save

Current top-level sections come from `CONFIG::Keys` and include:

- `notification`
- `wifiSensing`
- `ble`
- `shelly`
- `alarms`
- `heartbeat`
- `udp_pusher`
- `airmouse`
- `matrix`
- `macros`
- `logging`
- `power`
- `compensation`
- `usb_terminal`

Note that the schema is still mixed in a few historical places. New keys should follow the repo rule of `snake_case`.

## Boot-time behavior

Runtime configuration is initialized in `StorageInitializer`:

1. initialize NVS
2. mount LittleFS early only on cold boot
3. call `RTC::initConfig(LittleFS)`
4. create config and subsystem locks

Boot path rules:

- cold boot: initialize defaults, then load `/config/config.json`
- warm boot after deep sleep: restore config from the RTC shadow copy and skip early filesystem load

That warm/cold split is implemented in `src/system/rtc/RtcConfigLoader.*`.

## Access patterns in code

### Read access

Preferred patterns:

- `RTC::withConfig(...)` for reading selected fields under lock
- `RTC::getConfigSafeCopy()` when a full consistent copy is needed

Avoid copying the whole config store onto the stack unless there is a real reason.

### Write access

Preferred pattern:

- `RTC::updateConfig(...)`

That gives:

- lock protection
- `magic` / schema / CRC refresh

If the update must survive cold boot, it must also be persisted:

- typically via `CONFIG::save(LittleFS)`
- or via a service update handler that already calls `CONFIG::save(...)`

### `RtcStatefulService`

HTTP-facing settings services often wrap a config block using `RtcStatefulService<T>`.

Examples:

- BLE settings
- matrix settings
- notification settings
- Wi-Fi sensing settings

This pattern is useful when a module naturally owns one RTC-backed state object and exposes it over an endpoint. Other modules update config directly with `RTC::updateConfig(...)` instead.

## Backup and compatibility nuances

- power settings still keep a small NVS backup in namespace `power_cfg`
- most feature settings persist through the centralized JSON config file
- runtime-only counters and caches are not part of `config.json`

For the actual deep-sleep persistence model, see [rtc_persistence.md](rtc_persistence.md).

Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Architecture](../README.md#runtime-and-architecture)
