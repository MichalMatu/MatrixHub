Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Architecture](../README.md#runtime-and-architecture)

# Boot Sequence Reference

This document describes the current startup flow of the firmware.

## Source of Truth

The current boot path is defined primarily by:

- `src/main.cpp`
- `src/system/Application.cpp`
- `src/system/init/core/InitSequence.cpp`
- `src/system/services/ServiceRegistry*.cpp`
- `src/system/init/services/*Initializer*.cpp`

When this document and the code disagree, the code wins.

## High-Level Flow

The runtime path is:

```text
src/main.cpp
  -> Application::instance()
  -> Application::setup(server, framework)
  -> runBootSequence(...)
  -> InitSequence phases 1..7
```

`Application::setup(...)` first applies the aggressive PSRAM strategy and then executes the phased boot sequence.

## Phase 1: Storage

Implemented in `src/system/init/services/StorageInitializer.cpp`.

This phase:

- initializes NVS
- mounts LittleFS early on cold boot
- skips the early LittleFS mount on warm-boot fast path
- initializes RTC-backed config via `RTC::initConfig(LittleFS)`
- creates core locks used later by sensors and hardware helpers

Important decision:

- cold boot: mount LittleFS before reading config
- warm boot: reuse RTC state and let the framework mount the filesystem later

## Phase 2: Logging

Implemented in `src/system/init/core/InitSequence.cpp`.

This phase:

- starts `LoggingConfig`
- applies the saved log level without clearing early ring-buffer logs
- starts `BootTracker`

The goal is to get diagnostics online before hardware and service initialization becomes expensive.

## Phase 3: Power and Early Hardware

Implemented in `src/system/init/services/PowerInitializer.cpp`.

This phase:

- starts `TaskWatchdog`
- starts `PowerManager`
- registers `ShutdownSequence::execute` as the pre-sleep hook
- logs wake reason
- initializes `MatrixService`

## Network Configuration

Still part of the boot sequence, but executed between power and framework start in `InitSequence::configureNetwork(...)`.

This step:

- configures the HTTP server
- configures the HTTPS server wrapper as well
- pins server tasks according to current network/task config

## Phase 4: Framework

Implemented in `src/system/init/core/InitSequence.cpp`.

This phase:

- initializes the PSRAM buffer used by `Utils::JsonResponseWriter`
- starts `ESP32SvelteKit` with HTTPS cert/key
- mounts the filesystem through the framework when required

## Phase 5: Services

Implemented by `ServiceRegistry::begin(...)`, `ServiceRegistry::runInitializationSequence()`,
and the registry/service initializer split under `src/system/init/services/`.

Current registry order:

1. core services
2. business services
3. BLE services
4. matrix services
5. IMU services
6. interaction services
7. notification services
8. API services
9. runtime callback wiring

### Core Services

Initialized in `src/system/init/services/CoreServicesInitializer.cpp`.

Notable services:

- `CompensationService`
- `BleService`
- `AlarmService`
- `Scd4xSensorService`
- `CsiService`
- `WifiSensingService`

### Business Services

Initialized in `ServiceRegistry::initializeBusinessServices(...)` in
`src/system/services/ServiceRegistryInitialization.cpp`.

Notable services:

- `ShellyService`
- `WifiSensingSettings`
- `NotificationSettingsService`

Important decision:

- `ShellyService` is constructed always, but `begin()` only runs when saved devices exist

### BLE Services

Initialized in `src/system/init/BleServicesInitializer.cpp`.

This phase:

- creates `BleSettingsService`
- injects the saved BLE config into `BleService`
- wires runtime callbacks for settings changes
- starts `BleService` only if BLE is enabled in settings

### Notification Services

Initialized in `src/system/init/NotificationServicesInitializer.cpp`.

This phase creates:

- `TelegramClient`
- `TelegramWorker`
- `TelegramNotifier`
- `WebhookWorker`
- `WebhookNotifier`
- `PushoverWorker`
- `NotificationWorker`

Important decision:

- `NotificationWorker::begin()` runs only if at least one notification backend is enabled

### API Services

Initialized in `src/system/init/ApiServicesInitializer.cpp`.

This phase registers the HTTP API and supporting services, including:

- power
- logs
- config
- notifications
- WiFi sensing
- system
- alarms
- Shelly
- BLE
- heartbeat
- UDP
- keyboard
- compensation
- USB terminal
- file manager

## Phase 6: Runtime Tasks

Implemented in `src/system/init/RuntimeTasksInitializer.cpp`.

This phase:

- starts USB log output
- starts `MatrixTask`
- starts `BinaryDataLogger`
- links WiFi sensing into BLE
- starts `Heartbeat`
- starts `SensorLoggingTask`
- configures `UdpPusher`

## Phase 7: Monitoring

Implemented in `src/system/init/services/MonitoringInitializer.cpp`.

This phase:

- starts `ButtonHandler`
- wires button callbacks into `AirMouseService` when present
- starts `HeapMonitor`
- registers the current task in `TaskWatchdog`
- starts `SystemHealth`
- starts `ThermalMonitor`

## Telegram Command Runtime Behavior

Telegram command handling is part of the unified notifications runtime.

Current behavior:

- `TelegramWorker` lives under `src/notifications/telegram/runtime/`
- command parsing and dispatch live under `src/notifications/telegram/commands/`
- inbound polling runs when Telegram is configured and either:
  - `commands_enabled` is true, or
  - `chat_id` has not yet been auto-discovered

## After Setup: Main Loop

After boot completes, `Application::loop()` repeatedly drives:

- `PowerManager::loopTick()`
- `buttonHandler.update()`
- `framework.loop()`
- registry-owned `BleService::loop()`
- `SystemHealth::update()`
- alarm processing through `AlarmService::processPending()`
- registry-owned `UdpPusher::update()`
- `UsbTerminalService::loop()`
- `HealthMaintenancePulse::update()`

The main loop also feeds `TaskWatchdog`.

## Persistence Decisions During Boot

- RTC config is the fast-path source for warm boot
- LittleFS is the fallback source for cold boot
- service settings exposed through HTTP endpoints persist through `CONFIG::save(...)`

## Practical Summary

- Storage and RTC state come first
- logging and boot diagnostics come second
- power/framework come before services
- `ServiceRegistry` is the composition root for runtime wiring
- task startup is separate from service construction
- monitoring comes last, after services and tasks already exist

Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Architecture](../README.md#runtime-and-architecture)
