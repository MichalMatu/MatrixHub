# MatrixHub — Firmware for Waveshare ESP32-S3-Matrix

**MatrixHub** is an open-source firmware and web dashboard for the
[Waveshare ESP32-S3-Matrix](https://www.waveshare.com/wiki/ESP32-S3-Matrix) board.
It combines environmental monitoring (CO₂, temperature, humidity), a rich web UI,
BLE scanning, Wi-Fi motion sensing, USB HID emulation, Telegram/Webhook/Pushover
notifications, Shelly smart-plug integration, and a scriptable macro engine —
all running on a dual-core ESP32-S3 with PSRAM.

> **License**: GPL-3.0 — free for personal/hobbyist use; commercial redistribution
> of closed-source derivatives is not permitted. See [LICENSE](LICENSE).

---

## Table of Contents

- [Hardware](#hardware)
- [Features](#features)
- [First Boot and Device Access](#first-boot-and-device-access)
- [Build and Flash](#build-and-flash)
- [Module Reference](#module-reference)
  - [Environmental Sensor (SCD41)](#environmental-sensor-scd41)
  - [LED Matrix Display](#led-matrix-display)
  - [Alarm System](#alarm-system)
  - [Notifications](#notifications)
  - [Telegram Bot](#telegram-bot)
  - [Webhook Integration](#webhook-integration)
  - [Pushover Integration](#pushover-integration)
  - [UDP Push](#udp-push)
  - [Shelly Integration](#shelly-integration)
  - [BLE Scanner](#ble-scanner)
  - [Wi-Fi CSI Motion Sensing](#wi-fi-csi-motion-sensing)
  - [Air Mouse (USB HID)](#air-mouse-usb-hid)
  - [USB Keyboard & Mouse Jiggler](#usb-keyboard--mouse-jiggler)
  - [Macro Engine](#macro-engine)
  - [IMU (QMI8658)](#imu-qmi8658)
  - [Sensor Compensation](#sensor-compensation)
  - [Data Logger](#data-logger)
  - [Power Management](#power-management)
  - [Thermal Protection](#thermal-protection)
  - [System Button](#system-button)
  - [Web UI](#web-ui)
- [Repository Layout](#repository-layout)
- [Architecture Notes](#architecture-notes)
- [Video Demos](#video-demos)
- [Contributing](#contributing)

---

## Hardware

| Component | Details |
|-----------|---------|
| **Board** | Waveshare ESP32-S3-Matrix |
| **MCU** | ESP32-S3, dual-core Xtensa LX7 @ 240 MHz |
| **PSRAM** | 8 MB SPIRAM (all large buffers live here) |
| **LED Matrix** | 8 × 8 WS2812B on GPIO 14 |
| **IMU** | QMI8658 (gyro + accelerometer) on I2C1 — SDA=11, SCL=12, INT1=8 |
| **CO₂ Sensor** | SCD41 on I2C2 (Wire1) — SDA=6, SCL=7, address 0x62 |
| **Boot button** | GPIO 0 (active LOW) — multi-function: short/medium/long press |
| **USB** | Native USB (HID Mouse + Keyboard via TinyUSB) |

---

## Features

| Category | Highlights |
|----------|-----------|
| **Sensing** | CO₂ (400–10 000 ppm), temperature, humidity via SCD41 |
| **Display** | 8×8 RGB matrix — live readings, alarm icons, custom icons, menu |
| **Alarms** | Per-metric rules with severity, cooldown, and multi-channel dispatch |
| **Notifications** | Telegram bot (send + receive commands), Webhook (HTTP POST), Pushover |
| **Smart home** | Shelly smart-plug monitoring and control |
| **BLE** | Passive BLE device scanner with configurable scan windows |
| **Wi-Fi CSI** | Channel-state-information motion sensing (WebSocket streaming) |
| **USB HID** | Air Mouse (gyro-driven cursor), keyboard jiggler, macro shortcuts |
| **Macros** | Scriptable action sequences triggered by button clicks or API calls |
| **Power** | Configurable deep-sleep after inactivity, periodic wake |
| **Thermal** | CPU throttle at 60 °C / 68 °C, emergency sleep at 80 °C |
| **Data** | Binary ring-log on LittleFS; charts in the web UI |
| **Web UI** | SvelteKit dashboard served from embedded LittleFS (HTTPS) |
| **Security** | JWT auth, HTTPS (mbedTLS), rate limiting, BLE bonding with MITM |
| **UDP push** | Real-time telemetry broadcast to local network listeners |

---

## First Boot and Device Access

If no Wi-Fi STA network is saved the device starts in **Access Point** mode.

| Parameter | Default value |
|-----------|--------------|
| AP SSID | `matrixhub.local` |
| AP password | *(open)* |
| AP address | `192.168.4.1` |
| Hostname | `matrixhub` |
| Browser URL | `matrixhub.local` or `192.168.4.1` |

**Recommended first-run flow**

1. Flash the firmware.
2. Connect to the Wi-Fi AP `matrixhub.local`.
3. Open `matrixhub.local` or `192.168.4.1` in a browser. The device redirects
   to HTTPS automatically; accept the self-signed certificate warning.
4. Sign in with the configured credentials.
5. Go to **WiFi Station** and add your home network.
6. Open **System Status** to confirm the device is online.
7. Configure alarm thresholds and notification channels.

---

## Build and Flash

> **Prerequisites**: [PlatformIO](https://platformio.org/) CLI or IDE extension,
> Node.js ≥ 18 for the SvelteKit frontend.

Create a local HTTPS certificate header before the first build:

```bash
cp src/config/certificates.local.example.h src/config/certificates.local.h
# Replace the placeholder certificate and private key with your own device keypair.
```

`src/config/certificates.local.h` is intentionally ignored by Git. Do not commit
device certificates or private keys.

```bash
# Kill any open monitor first (required before every upload)
pkill -f monitor

# Full build (UI + firmware) and upload
pio run -e waveshare_esp32s3_matrix -t upload

# Fast firmware-only iteration (skips UI rebuild)
SKIP_UI=1 pio run -e waveshare_esp32s3_matrix -t upload

# Device monitor (after flashing)
pio device monitor

# Backend unit tests on the host (Unity)
pio test -e native
```

Frontend standalone build:

```bash
cd interface
npm install
npm run build
```

---

## Module Reference

### Environmental Sensor (SCD41)

`src/sensors/scd4x/`

Reads CO₂ concentration, temperature, and humidity from the Sensirion SCD41 over I²C.
Samples every **5 s**; the result is pushed to the live dashboard via callbacks and
also fed to the alarm engine. The sensor undergoes automatic self-healing with
exponential back-off if consecutive reads fail.

| Metric | Range |
|--------|-------|
| CO₂ | 400–10 000 ppm |
| Temperature | −20 °C – +70 °C |
| Humidity | 0–100 % RH |

---

### LED Matrix Display

`src/matrix/`

Controls the 8×8 WS2812B matrix via the RMT peripheral (DMA buffer in internal DRAM).

- **Live data mode** — scrolls current CO₂ / temperature / humidity reading
- **Alarm mode** — displays a dedicated alarm icon with configurable colour
- **Custom icons** — up to N user-defined 8×8 bitmaps stored in LittleFS
- **Effect modes** — selectable animation effects per state
- **Menu overlay** — matrix-based navigation menu accessed via the boot button

Settings (brightness, scroll speed, colour palette) are persisted in LittleFS
and applied at runtime without reboot.

---

### Alarm System

`src/alarms/`

Flexible rule engine evaluated every **10 s**.

- Rules are defined per metric (CO₂, temperature, humidity) with:
  - threshold value and direction (above / below)
  - severity level (info / warning / critical)
  - cooldown period (10 s – 24 h)
- On trigger, the alarm notifier dispatches to all enabled channels
  (matrix, Telegram, Webhook, Pushover).
- Rules are stored in LittleFS and editable via the web UI.

---

### Notifications

`src/notifications/`

A unified dispatcher routes alarm events to one or more channels:

| Channel | Module |
|---------|--------|
| Telegram | `notifications/telegram/` |
| Webhook | `notifications/webhook/` |
| Pushover | `notifications/pushover/` |

Each channel has independent enable/disable, credentials, and validation logic.

---

### Telegram Bot

`src/notifications/telegram/`

Full two-way Telegram integration with TLS (mbedTLS).

- **Outbound**: sends alarm notifications and status summaries to a configured chat.
- **Inbound**: polls for commands (long-poll, 1 s timeout, exponential back-off up
  to 5 min) and responds to `/status`, `/co2`, `/temp`, `/humidity`, and custom commands.
- TLS connection is shared and reused; a warm-up handshake with retry ensures
  reliability on flaky networks.
- Maximum message length: 768 characters. Secrets (bot token, chat ID) are
  stored in LittleFS, not hard-coded.

---

### Webhook Integration

`src/notifications/webhook/`

Sends an HTTP POST with a JSON payload to any user-configured URL when an alarm fires.

- Configurable URL, custom headers possible.
- JSON body includes metric name, value, severity, and timestamp.
- Backed by a queue + worker task to avoid blocking the alarm engine.

---

### Pushover Integration

`src/notifications/pushover/`

Sends push notifications to the Pushover service (iOS / Android app).

- TLS connection to `api.pushover.net`.
- Priority and sound configurable per notification.

---

### UDP Push

`src/udp/`

Broadcasts real-time sensor telemetry as UDP packets to a configurable host/port.
Useful for feeding data to local dashboards (e.g. InfluxDB via Telegraf) without
requiring a cloud service.

---

### Shelly Integration

`src/shelly/`

Monitors and controls Shelly smart plugs over the local network (HTTP / Gen1 & Gen2 API).

- Polls power consumption, relay state, and device status.
- Allows toggling relay state via the web UI or alarm actions.
- Supports multiple Shelly devices, each with configurable IP and name.
- Alarm wiring: a Shelly relay can be automatically switched by an alarm rule.

---

### BLE Scanner

`src/ble/`

Passive BLE advertiser scanner powered by NimBLE.

- Scans for nearby BLE devices on a configurable interval.
- Reports RSSI, device name, and last-seen timestamp via the web UI and API.
- Turbo discovery mode increases scan duty cycle for faster device enumeration.
- BLE stack runs on **Core 0** alongside the Wi-Fi stack.
- Scan parameters: 5 s scan duration, 500 ms interval, 450 ms window (90 % duty
  cycle) in normal mode.

---

### Wi-Fi CSI Motion Sensing

`src/wifisensing/`

Uses 802.11 Channel State Information (CSI) to detect motion without a dedicated
PIR sensor.

- CSI packets arrive via the Wi-Fi task callback at up to 10 Hz and are copied
  into an internal-RAM FreeRTOS queue.
- A processing task computes variance across subcarrier amplitudes; a configurable
  threshold triggers a motion event.
- Live CSI data is streamed to the browser over a binary WebSocket.
- Feature is **disabled by default** and requires STA mode (not available in AP-only mode).

---

### Air Mouse (USB HID)

`src/airmouse/`

Turns the ESP32-S3-Matrix into a gyroscope-driven wireless USB mouse.

- IMU (QMI8658) gyro data drives cursor movement via USB HID.
- Calibration removes gyro drift at startup or on demand.
- Single / double / triple button clicks map to left click, right click, and
  configurable macro execution.
- Shares the USB HID interface with the keyboard service (mutex-protected).
- Configurable sensitivity and dead-zone via the web UI.

---

### USB Keyboard & Mouse Jiggler

`src/keyboard/` · `src/airmouse/features/JigglerController.cpp`

- **USB Keyboard**: injects HID keystroke sequences programmatically (used by
  macros and the jiggler).
- **Mouse Jiggler**: periodically moves the cursor by a tiny amount to prevent
  screen lock / screen savers. Jiggle interval, distance, and pattern are
  configurable. The jiggler can also send keystrokes to simulate activity.

---

### Macro Engine

`src/macros/`

A lightweight scripting engine for automating keyboard/mouse actions.

- Macros are defined as action sequences (key press, delay, mouse click, etc.)
  and stored in LittleFS.
- Triggered by: physical button clicks (single / double / triple), Telegram
  commands, or HTTP API calls.
- The engine runs on a dedicated FreeRTOS task; execution is atomic per macro.
- Macros are editable and reloaded at runtime without a reboot.

---

### IMU (QMI8658)

`src/sensors/imu/`

Driver and manager for the QMI8658 6-axis IMU.

- Provides calibrated gyroscope (°/s) and accelerometer (g) data.
- Used by the Air Mouse module for cursor control.
- Interrupt-driven data-ready signalling on INT1 (GPIO 8).
- Shared consumer flag system allows multiple subsystems to subscribe to
  movement and click events without polling.

---

### Sensor Compensation

`src/compensation/`

Corrects the SCD41 temperature reading for self-heating caused by the ESP32-S3 MCU.

- Applies a dynamic offset: `offset = base + slope × (cpu_temp − ref_cpu_temp)`.
- CPU temperature is sampled from the internal ADC and smoothed with an
  exponential moving average (α = 0.05).
- Compensation parameters (base offset, slope, reference CPU temp, min/max clamp)
  are configurable via the web UI and persist in LittleFS.
- When disabled, a fixed factory default offset is applied.

---

### Data Logger

`src/system/datalogger/`

Writes sensor history to LittleFS in a compact binary format.

- Aggregates samples over a **5-minute window** before writing to flash
  (reduces wear and keeps files representative of wall-clock time).
- Old log files are rotated when free space drops below 50 KB.
- Log files are downloadable and viewable as charts in the web UI.
- A separate in-RAM ring buffer (500 entries) feeds the live log tail endpoint
  without touching flash.

---

### Power Management

`src/system/power/`

Configurable deep-sleep to extend battery life or reduce idle power.

| Parameter | Default | Range |
|-----------|---------|-------|
| Inactivity timeout | 5 min | 5 min – 24 h |
| Grace period after boot | 1 min | 1 min – 10 min |
| Wake interval | 10 min | — |

- Activity is tracked from HTTP requests, button presses, and BLE events.
- Before sleeping, a config snapshot is saved to RTC memory for fast warm-boot restore.
- Deep sleep is **disabled by default** and must be enabled in the web UI.
- When deep sleep is enabled, at least one wake source must stay enabled:
  timer, BOOT button, or touch wake.

---

### Thermal Protection

`src/system/thermal/`

Three-stage CPU temperature protection:

| Stage | Threshold | Action |
|-------|-----------|--------|
| Soft throttle | 60 °C | Reduce CPU to 160 MHz |
| Hard throttle | 68 °C | Lock CPU at 160 MHz |
| Critical / emergency | 80 °C | Deep sleep for 30 s to cool down |

Hysteresis of 5 °C prevents rapid toggling between states.
Temperature is monitored every **5 s**.

---

### System Button

`src/system/button/`

The boot button (GPIO 0) is multi-function:

| Gesture | Action |
|---------|--------|
| Short press (< 1 s) | Single click — configurable (macro / air mouse click) |
| Medium hold (2 s) | Open / close matrix navigation menu |
| Long hold (10 s) | Arm factory reset (device warns at 7 s) |
| Double-click after 10 s hold | Confirm factory reset (within 5 s window) |

During a medium-press hold, periodic actions (e.g. macro triggers) are fired
every configured interval — the button does not need to be released first.

---

### Web UI

`interface/` (SvelteKit, embedded in LittleFS)

The dashboard is served over HTTPS directly from the device.

| Section | Content |
|---------|---------|
| **Dashboard** | Live CO₂, temperature, humidity with mini-charts |
| **Charts** | Historical sensor data from the binary data logger |
| **Alarms** | Rule editor — thresholds, severity, cooldowns |
| **Notifications** | Telegram, Webhook, Pushover configuration |
| **Shelly** | Device list, relay control, power readings |
| **BLE** | Nearby device list with RSSI and last-seen |
| **Wi-Fi Sensing** | Enable/disable CSI, live WebSocket data view |
| **Air Mouse** | Enable/disable, sensitivity, calibration |
| **Macros** | Create, edit, and trigger macro sequences |
| **Matrix** | Brightness, speed, custom icons, display mode |
| **Compensation** | SCD41 temperature offset parameters |
| **Power** | Sleep settings, inactivity timeout |
| **WiFi Station** | Add/remove Wi-Fi networks |
| **System Status** | Heap, PSRAM, uptime, task list, CPU temperature |
| **Logs** | Live log tail + downloadable daily log files |
| **System** | Reboot, factory reset, firmware update, time sync |
| **Help** | Quick-start guide and feature overview |

---

## Repository Layout

```text
MatrixHub/
├── src/                    # Firmware (C++17 / ESP-IDF + Arduino)
│   ├── airmouse/           # USB HID air mouse & jiggler
│   ├── alarms/             # Alarm rule engine & notifier
│   ├── api/                # HTTP API controllers (/api/…)
│   ├── ble/                # BLE scanner (NimBLE)
│   ├── compensation/       # SCD41 temperature compensation
│   ├── config/             # System.h, Hardware.h, App.h — source of truth
│   ├── keyboard/           # USB HID keyboard service
│   ├── macros/             # Macro engine, parser, persistence
│   ├── matrix/             # LED matrix driver & menu
│   ├── notifications/      # Telegram / Webhook / Pushover dispatchers
│   ├── sensors/            # SCD41, IMU (QMI8658), logging
│   ├── shelly/             # Shelly smart-plug integration
│   ├── system/             # Boot, power, thermal, button, data logger, watchdog
│   ├── udp/                # UDP telemetry push
│   ├── utils/              # Shared utilities
│   └── wifisensing/        # RSSI sensing and CSI diagnostics
├── interface/              # SvelteKit web dashboard
│   └── src/
├── lib/
│   ├── framework/          # Upstream web/API framework base (LGPL-3.0)
│   ├── PsychicHttp/        # HTTPS server (MIT)
│   ├── ArduinoJson/        # JSON (MIT)
│   ├── NimBLE-Arduino/     # BLE stack (Apache-2.0)
│   └── sensirion_scd4x/    # SCD41 driver (BSD-3-Clause)
├── test/                   # Unity unit tests (native host)
├── boards/                 # PlatformIO board definitions
├── docs/                   # Maintained engineering, workflow, and user notes
├── LICENSE                 # GPL-3.0
└── platformio.ini          # Build configuration
```

---

## Architecture Notes

- **Dual-core pinning**: Wi-Fi / BLE / networking on Core 0; UI logic, matrix,
  and application tasks on Core 1.
- **Memory strategy**: everything > 512 B that does not require DMA lives in
  PSRAM (`MALLOC_CAP_SPIRAM`). DMA buffers (matrix, SPI) stay in internal DRAM.
- **Service pattern**: long-lived services are owned and wired by
  `ServiceRegistry`; API services stay transport-focused.
- **Config persistence**: settings live in a PSRAM working store, persist to
  LittleFS JSON, and use an RTC shadow copy for deep-sleep restore.
- **No magic numbers**: all timeouts, thresholds, and pin assignments are in
  `src/config/Hardware.h`, `App.h`, and `TaskConfig.h`.

---

## Video Demos

- [This Device Warns Me About High CO₂ and Sends It to Telegram](https://youtu.be/uUkitUXndD8?si=6oeI0EoqvSQORf07)
- [MatrixHub Full Interface Tour: Alerts, Telegram, BLE, Shelly, and System Pages](https://youtu.be/ElIpB5tRcJQ)

---

## Contributing

1. Keep all task configs in `src/config/TaskConfig.h`.
2. Do **not** modify `lib/framework/` or `src/wifisensing/csi/vendor/` without
   explicit approval.
3. Run backend tests before submitting: `pio test -e native`.
4. Use `LOGI()` / `LOGW()` / `LOGE()` — never `Serial.print()`.
5. All new large buffers (> 512 B) must use `MALLOC_CAP_SPIRAM`.

See [docs/engineering/README.md](docs/engineering/README.md) for the full
engineering reference.
