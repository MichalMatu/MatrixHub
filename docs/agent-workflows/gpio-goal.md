# GPIO Module Implementation Goal

## Objective

Implement GPIO as a separate MatrixHub module that exposes stable digital boolean
states. Alarm rules may consume these states, but the GPIO module must not depend
on the alarm module.

## Scope

- Add a standalone GPIO domain module.
- Add a GPIO menu entry and a dedicated UI page.
- Support digital inputs with debounce, pull mode, inversion, and logical state.
- Support digital outputs with explicit admin-only writes.
- Expose GPIO states through API as `true` / `false`.
- Add alarm consumption of GPIO boolean states through a selected GPIO channel.
- Add focused firmware and UI tests for validation, serialization, menu wiring,
  alarm integration, and regression coverage.

## Non Goals For V1

- No arbitrary GPIO number entry in the UI.
- No analog thresholds.
- No PWM, pulse counting, interrupts, wake-up triggers, or scripted automation.
- No GPIO configuration from the alarm screen.
- No use of risky, boot-critical, USB, UART, sensor, matrix, flash, or PSRAM pins.

## Safe Pins Policy

Firmware owns the allow-list. The UI may only select pins returned by the backend.
V1 should prefer fewer pins over broader coverage.

Initial conservative allow-list:

- GPIO1
- GPIO2
- GPIO4
- GPIO5
- GPIO33
- GPIO38
- GPIO39
- GPIO40

Reserved or blocked by default:

- GPIO0: BOOT / user button / wake source.
- GPIO3: strapping risk.
- GPIO6, GPIO7: SCD41 I2C in this project.
- GPIO8, GPIO10, GPIO13: IMU interrupt uncertainty across project docs and board variant.
- GPIO11, GPIO12: IMU I2C.
- GPIO14: WS2812 matrix.
- GPIO19, GPIO20: native USB.
- GPIO26-GPIO32: flash / PSRAM risk on ESP32-S3 modules.
- GPIO43, GPIO44: UART0 TX/RX.
- GPIO45, GPIO46: strapping / limited-use risk.

## Architecture Rules

- GPIO service is owned by `ServiceRegistry`.
- GPIO API uses `/api/gpio/...`.
- GPIO business logic lives under `src/gpio/`.
- API classes stay transport-focused.
- Alarm integration reads GPIO state through an injected service interface or
  narrow dependency, not through globals.
- The alarm module stores only the selected GPIO channel id for GPIO-backed rules.
- Boolean alarm evaluation remains `1.0` for true and `0.0` for false.

## Acceptance Criteria

- Existing baseline tests pass before functional changes.
- GPIO configuration rejects every blocked pin.
- GPIO status endpoint reports raw and logical state.
- Output writes are admin-only and rejected for non-output channels.
- Alarm rules can select a GPIO channel and trigger from its logical boolean state.
- Existing alarm sources keep their behavior.
- UI menu includes a dedicated GPIO entry.
- UI alarm form shows GPIO channel selection only for the GPIO alarm source.
- Fast firmware build passes.
- Native tests pass.
- After flashing, the board boots, system status works, and a short smoke run does
  not show unexpected restarts or WebSocket regressions.

