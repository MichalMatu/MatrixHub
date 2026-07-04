# Sidepanel Data Inventory

This is the current user-facing data inventory for the Chrome side panel after the first reduction pass.
The extension is moving toward a compact, dashboard-like control surface focused on:

- quick device switching
- quick sign-in
- fast sensor glance
- a compact settings summary for only key device status

## Transport Sources

- Auth is done over REST: `/rest/signIn`
- Overview snapshot is done over REST: `/api/system/info`
- Wi-Fi recovery is done over REST: `/api/system/wifi/recover`
- Live updates are done over WebSocket: `/ws/system`
- If the device origin is `https://...`, the socket is `wss://...`
- If the device origin is `http://...`, the socket is `ws://...`
- The WebSocket currently authenticates via the mirrored `access_token` cookie on `/ws`

## Current UI Data

### Navigation

- `screen_tabs`
  Current UI: `Add`, `Devices`
  Source: local sidepanel state
  Keep: yes

### Devices

- `device_card_selection`
  Current UI: one compact card per device
  Source: selected device id
  Keep: yes
- `device_name`
  Current UI: device card header
  Source: saved device record
  Keep: yes
- `device_card_realtime_state`
  Current UI: small status dot next to the selected signed-in card title
  Source: WebSocket connection state
  Keep: yes
- `device_name_input`
  Current UI: add form
  Source: device draft
  Keep: yes
- `device_address_input`
  Current UI: add form
  Source: device draft
  Keep: yes
- `device_address_error`
  Current UI: inline validation
  Source: validation layer
  Keep: yes

### Session

- `username_input`
  Current UI: inline login inside selected device card
  Source: credentials draft
  Keep: yes
- `password_input`
  Current UI: inline login inside selected device card
  Source: credentials draft
  Keep: yes

### Overview

- `telemetry_co2`
  Current UI: selected signed-in device card
  Source: telemetry snapshot over WebSocket
  File: `features/overview/mappers/viewModels.ts`
  Keep: yes
- `telemetry_temperature`
  Current UI: selected signed-in device card
  Source: telemetry snapshot over WebSocket
  File: `features/overview/mappers/viewModels.ts`
  Keep: yes
- `telemetry_humidity`
  Current UI: selected signed-in device card
  Source: telemetry snapshot over WebSocket
  File: `features/overview/mappers/viewModels.ts`
  Keep: yes
- `telemetry_sensor_quality`
  Current UI: settings / status section
  Source: telemetry snapshot + freshness state
  File: `features/overview/mappers/viewModels.ts`
  Keep: yes
- `telemetry_live_state`
  Current UI: settings / status section
  Source: WebSocket connection state
  File: `features/overview/mappers/viewModels.ts`
  Keep: yes
- `telemetry_last_refresh`
  Current UI: settings / status section
  Source: HTTP snapshot + last telemetry timestamp
  File: `features/overview/mappers/viewModels.ts`
  Keep: yes

### Settings

- `settings_signed_in_user`
  Current UI: settings / device section
  Source: saved session
  Keep: yes
- `settings_firmware`
  Current UI: settings / device section
  Source: latest normalized system snapshot
  Keep: yes
- `settings_uptime`
  Current UI: settings / device section
  Source: latest normalized system snapshot
  Keep: yes
- `status_realtime`
  Current UI: settings / status section
  Source: connection state + telemetry freshness
  Keep: yes
- `status_wifi`
  Current UI: settings / status section
  Source: normalized diagnostics + binary system status packet
  Keep: yes

## Removed In First Reduction Pass

- `saved_device_count`
- `signed_in_access_level`
- `overview_signal`
- `overview_core_temp`
- `overview_ap_stations`
- `overview_widgets`
- `device_origin`
- `settings_device_origin`
- `settings_build_time`
- `health_heap`
- `health_psram`
- `health_storage`
- `health_http_ws`
- `overview_firmware`
- `overview_wifi_mode`
- `connection_origin`
- `last_http_refresh`
- `quick_summary_ap_ssid`
- `quick_status_sta_ip`
- `quick_status_ap_ssid`
- `quick_status_ap_ip`
- `quick_summary_sta_ssid`
- `quick_status_sta_ssid`
- `quick_hostname`
- `quick_connection_mode`

## Current Reduction Anchors

- Overview cards are controlled centrally in `features/overview/mappers/viewModels.ts`
- Sidepanel orchestration is controlled by `features/sidepanel/controller/sidepanelController.ts`
- Device card flow is rendered by `entrypoints/sidepanel/App.svelte` and `features/devices/ui/DeviceCard.svelte`
- Future UX cuts should start in `viewModels.ts` and the sidepanel controller first
