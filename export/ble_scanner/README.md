# Bluetooth Low Energy (BLE) Scanner Integration Export

This folder contains the BLE Scanner and Manager module (frontend, backend, and core drivers) exported from the source project.
It handles discovering BLE beacon devices, registering them to a whitelist, broadcasting updates, and acting as a data provider for the rest of the system (e.g. Alarms based on BLE token presence).

## Structure
- `/frontend`: Contains Svelte/JS UI components (Scanner Modal, Settings, Status, Dashboard Widget), utilities, and API service.
- `/backend`: Contains the ESP32 backend logic including Core Lifecycle, Services, Configurations, API Handlers, Broadcasters, and initializers.
- `/backend/lib/NimBLE-Arduino`: The underlying lightweight BLE stack (Apache NimBLE) optimized for ESP32.
- `/test`: Contains Unity tests for BLE payload parsing, device detection, config logic, and stubs for NimBLE to allow native environment testing.
- `/docs`: Markdown documentation, setup flows, engineered overviews, and screenshots.

## Instructions for the Next Agent

### 1. Driver & Libraries
- Move the `NimBLE-Arduino` library from `backend/lib` into the new project's `lib/` directory or PlatformIO dependency manager. NimBLE uses significantly less RAM/Flash than Bluedroid (the default ESP-IDF Bluetooth stack), making it ideal alongside WiFi.
- Ensure the project enables Bluetooth configurations in `sdkconfig` or `platformio.ini` if doing a custom build.

### 2. Backend Implementation
- Move the contents of `backend/src/` into the exact same folder structure within the destination project (`src/`).
- Integrations:
  - Add `RtcBleTypes.h` to the main RTC context for deep-sleep persistency.
  - Initialize the `BleServicesInitializer` early in the application's boot process.
  - Register `BleApiService` and `BleBroadcaster` with the HTTP server and eventbus/websocket loop.
- The `BleDeviceProcessor` and `BleDataParser` handle incoming raw advertisement data. If you have new types of sensors (e.g. Xiaomi thermometers), add their MAC prefixes or Service UUIDs to `BleDeviceTypeDetector`.

### 3. Frontend Implementation
- Copy the Svelte files from `frontend/lib/features/bluetooth` to the target's `interface/src/lib/features/bluetooth`.
- Inject the `BleApiService.ts` into the main API wrapper.
- Mount the `bluetooth` routes (`frontend/routes/bluetooth`) into SvelteKit.
- **Translations:** Copy the translation keys prefixed with `ble_` from `messages/en.json` and `messages/pl.json` across to the target project.

### 4. Verifications
- Run backend unit tests (`pio test -e native`) on the `test_ble_*` directories. The Stubs will mock NimBLE for local execution.
- Build the firmware and flash it to an ESP32. Navigate to the Bluetooth settings page in the UI and click "Scan". If the device detects local Bluetooth beacons or smartphones, the integration is successful.
