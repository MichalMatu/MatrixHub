Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Architecture](../README.md#runtime-and-architecture)

# Firmware Module Map (src)

Below is the overview of the ESP32 firmware modules located in `src`. The project uses a modular architecture with `ServiceRegistry` as the composition root and `src/system/init/` as the phased boot wiring layer.

## Core Structure

### 1. SYSTEM (`src/system`)
Core system services and infrastructure.
- **`src/system/power`**: `PowerManager`, `PowerSettings`, Sleep/Wake logic.
- **`src/system/rtc`**: `RtcStatefulService`, persistence across deep sleep.
- **`src/system/datalogger`**: `BinaryDataLogger` for flash storage.
- **`src/system/init`**: Initialization sequences (`InitSequence`).
- **`src/system/services`**: `ServiceRegistry` and API service storage.
- **`src/system/logging`**: Logging infrastructure.
- **`src/system/matrix_manager`**: Central director for Z-index layers and notification queue on Matrix displays.
- **`src/system/memory`**: Memory and heap monitoring.
- **`src/system/utils`**: Shared runtime helpers such as locks, static services, and JSON streaming.

### 2. SENSORS (`src/sensors`)
Hardware drivers and data acquisition.
- **`src/sensors/imu`**: QMI8658 IMU driver.
- **`src/sensors/scd4x`**: CO2/Temp/Hum sensor driver.
- **`src/sensors/logging`**: Background tasks for sensor data.

### 3. INTEGRATIONS & COMMUNICATION
External APIs and network protocols.
- **`src/shelly`**: Shelly relays integration.
- **`src/notifications/telegram`**: Telegram client, polling, queueing, and command handlers.
- **`src/udp`**: UDP broadcasting support.
- **`src/ble`**: Bluetooth Low Energy stack (NimBLE facade).

### 4. DOMAIN MODULES
Business logic isolated by feature.
- **`src/airmouse`**: Air Mouse / remote control features using IMU data.
- **`src/alarms`**: Rule engine and alert storage.
- **`src/compensation`**: Sensor environmental compensation logic.
- **`src/keyboard`**: Bluetooth Keyboard emulation logic.
- **`src/macros`**: Automated action sequences.
- **`src/matrix`**: Matrix display drivers.
- **`src/notifications`**: Global notification dispatcher.
- **`src/usb_terminal`**: USB Serial/terminal interface.
- **`src/wifisensing`**: RSSI-based motion detection.

### 5. API (`src/api`)
HTTP API endpoints and WebSocket-adjacent broadcasters, organized per module (for example `src/api/power`, `src/api/alarms`, `src/api/system`).

### 6. CONFIG (`src/config`)
Central configuration definitions (`App.h`, `System.h`, etc.) and JSON serializers.

## Directory Map

```text
src/
├── airmouse/        # Air Mouse control
├── alarms/          # Alarm logic
├── api/             # HTTP endpoints and API-side helpers
├── ble/             # BLE stack and BLE settings
├── compensation/    # Sensor data adjustment
├── config/          # Configuration headers
├── filemanager/     # Flash FS manager
├── keyboard/        # BT Keyboard emulation
├── macros/          # Action execution macros
├── matrix/          # Matrix display hardware drivers
├── notifications/   # Notification center and Telegram integration
├── sensors/         # Hardware Drivers (IMU, SCD4x, LTR)
├── shelly/          # Shelly relay integration
├── system/          # Core System (boot, init, services, RTC, health)
├── udp/             # UDP broadcasting
├── usb_terminal/    # USB configuration interface
├── utils/           # Shared low-level helpers
├── wifisensing/     # WiFi Sensing logic
└── main.cpp         # Entry point
```

Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Architecture](../README.md#runtime-and-architecture)
