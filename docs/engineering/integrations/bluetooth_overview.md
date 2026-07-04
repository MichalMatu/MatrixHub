Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Integrations](../README.md#integrations-and-specialized-subsystems)

# Bluetooth Overview

This document describes the current BLE stack in the firmware. It is an architecture overview, not a changelog or audit log.

The BLE implementation in this repository is scanner-only.

The project no longer ships:

- BLE peripheral advertising
- GATT services for client devices
- passkey pairing / bonding flows

## Top-level architecture

The BLE subsystem is centered around `BLE::BleService` and split into a small set of scanner-focused modules:

- `src/ble/BleService.*` - thin facade used by the rest of the application.
- `src/ble/core/BleLifecycleManager.*` - NimBLE startup/shutdown for scanner runtime only.
- `src/ble/core/BleStatusManager.*` - runtime state for scanner activity and discovery.
- `src/ble/scanner/*` - BLE scanner, parsing, discovery cache, whitelist updates.
- `src/ble/settings/BleSettingsService.*` - persisted scanner settings and whitelist management.

`BleService::loop()` is intentionally small. It handles deferred events and delegates long-running work to the scanner stack.

## Runtime model

- `enabled = false` means the BLE subsystem is not running and discovery is unavailable.
- `enabled = true` means the BLE subsystem is allowed to run in scanner mode.
- `scanner_active` is a runtime state, not a persisted preference.
- `scanner_active` can still be `false` while `enabled = true`, for example during Wi-Fi AP coexistence handling.

## Settings and status contracts

The scanner-only contract is intentionally small:

- persisted settings: `{ enabled, sensors[] }`
- REST status: `{ enabled, running, scanner_active, devices[] }`
- BLE snapshot channel: same settings shape plus discovered devices
- binary system-status packet: Wi-Fi status, Wi-Fi flags, RSSI, CPU temperature

Compatibility peripheral fields such as advertising state, connection counters,
passkeys, or pairing state are no longer part of the live API.

## Discovery and coexistence

- The scanner updates RTC-backed BLE caches and can feed higher-level modules such as alarms, Telegram, widgets, and whitelist management.
- On this ESP32-S3 target, scanner discovery is allowed to coexist with Wi-Fi AP mode by default.
- The deferred AP-start scanner shutdown path is retained only as a target-level fallback when `kAllowScannerInApMode` is set to `false`.

## Memory strategy

BLE follows the repo-wide memory rules:

- long-lived scanner state lives where it is safe for the platform
- ISR-sensitive and flash-access-sensitive stacks stay in internal DRAM
- discovery payloads stay small and predictable
- JSON is used on HTTP/WebSocket surfaces; BLE itself is now scanner ingestion only

## Debugging checklist

When BLE behavior looks wrong, verify in this order:

1. Is `BleService` enabled and running?
2. Is `scanner_active` true when you expect discovery to be live?
3. Does the whitelist in BLE settings contain the expected sensors?
4. Are BLE readings reaching RTC cache and the `ble` system channel snapshot?
5. If discovery stops after Wi-Fi mode changes, check AP-mode coexistence and deferred scanner stop logic.

Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Integrations](../README.md#integrations-and-specialized-subsystems)
