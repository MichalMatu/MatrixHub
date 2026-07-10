# MatrixHub Product Readiness

This note captures product-facing decisions and the near-term readiness backlog.
It is intentionally separate from firmware architecture docs so sales, UX,
hardware, and release work can be tracked without changing runtime behavior.

The staged engineering audit and closure gates are maintained in
[MatrixHub Commercial Hardening Audit](engineering/COMMERCIAL_HARDENING_AUDIT.md).

## Current Product Position

MatrixHub should be positioned first as a local CO2 and ventilation automation
device:

- indoor CO2, temperature, and humidity monitoring
- local dashboard served directly from the device
- alarm rules with matrix display, Telegram, Webhook, and Pushover delivery
- Shelly smart-plug control for fans, purifiers, and ventilation helpers
- local logs and charts without requiring a cloud account

Advanced features such as Wi-Fi CSI, BLE scanning, USB HID, macros, and the USB
terminal are valuable differentiators, but they should not be the first sales
message. Treat them as advanced or maker-oriented capabilities until the core air
quality workflow is easy to explain, set up, and support.

## Explicit Credential Decision

Development builds keep the factory credentials:

- username: `admin`
- password: `admin`

This is a deliberate development-phase decision. It keeps local boards easy to
recover, reflash, and test while the product shape is still moving.

Before flashing any customer board, MatrixHub needs a separate provisioning flow
with per-device credentials, customer-facing recovery instructions, and a
repeatable record of what was flashed. Do not change the current `admin/admin`
defaults in-place as part of ordinary firmware, UX, repository cleanup, or build
speed work.

## Next Workstreams

### 1. UX and First-Run Flow

Goal: make the first 10 minutes feel like a product, not a developer firmware.

- define a simplified "Air Quality" dashboard as the default product view
- add setup copy around Wi-Fi, time sync, notifications, and alarm presets
- make CO2 alarm presets visible and easy: normal, office, bedroom, ventilation
- keep advanced pages available, but reduce their prominence in the first-run path
- verify mobile layout for dashboard, alarms, Wi-Fi setup, and notifications
- write a one-page operator quick start that matches the actual UI

### 2. Repository and Release Hygiene

Goal: make the repo trustworthy before producing test units.

- classify `artifacts/`, `crashlogs/`, `export/`, and verbose build logs as kept,
  ignored, or generated-outside-git
- keep build helper scripts as the documented path for firmware iteration
- add a release checklist covering build command, artifact names, version, and
  smoke-test commands
- make sure generated files are not rewritten when bytes are unchanged
- keep GPL source availability and third-party license notes visible for sales
  and customer handoff

### 3. Hardware Add-On Decisions

Goal: increase sales value without adding fragile support promises.

- prefer SHT40/SHT41 or AHT20 over DHT22 for extra temperature/humidity sensing
- prototype 433 MHz as an optional RX+TX add-on with a "learn remote" flow
- do not promise universal 433 MHz socket compatibility; ship a tested list
- consider PM2.5 or mmWave presence as a later Pro variant, not the first SKU
- verify the exact Waveshare board memory variant used in the BOM and docs

### 4. Production Provisioning

Goal: separate development convenience from customer safety.

- keep `admin/admin` only for development images
- create a customer flashing checklist with per-device credentials
- record firmware version, build date, device identifier, and credential handoff
- decide whether AP setup remains open or gets a per-device password/QR label
- document factory reset behavior for support

### 5. Customer Remote Access

Goal: let customers reach their MatrixHub admin panel from the Internet without
router configuration or public device IP addresses.

- design remote access as an outbound ESP32 WebSocket relay, not as direct access
  to the customer's home IP
- use Cloudflare Pages, Worker, and Durable Objects as the target production
  architecture
- use Raspberry Pi 5 plus Cloudflare Tunnel only as an MVP/lab relay before
  moving the product surface to Cloudflare
- use pairing codes only as short-lived, single-use setup tokens
- store long-lived `device_id` and `device_secret` only after pairing
- add a local UI switch, status indicator, disconnect action, and revocation
  action for remote access
- keep full remote admin behind an explicit security review before exposing all
  routes
- implementation guide: [remote access Cloudflare relay][remote-access-cloudflare]

## Recommended Immediate Sequence

1. Audit the current UI routes and identify the minimum product-first route set.
2. Clean repository outputs only after classifying which files are intentional.
3. Prototype the sensor/RF add-on on a breadboard, outside the main firmware path.
4. Prototype remote access with a Raspberry Pi 5 relay behind Cloudflare Tunnel,
   limited to read-only status APIs.
5. Build a 5-device pilot process before changing defaults or manufacturing flow.

## Initial Audit Notes

Captured on 2026-06-30:

- `git status` was clean before this document and the credential note were added.
- Ignored local artifacts such as `.pio/`, `artifacts/`, `crashlogs/`, build logs,
  `interface/build/`, `interface/.svelte-kit/`, and `node_modules/` are already
  covered by `.gitignore` and are not tracked.
- `.vscode/settings.json` is tracked even though `.vscode/` is ignored. Decide
  later whether this shared editor setting is intentional.
- `export/` is tracked and contains module export packages with README files.
  Treat it as intentional until someone decides these exports are stale.
- `extension/` is tracked as a separate browser extension project. Its
  `node_modules/` and WXT output are ignored, so do not remove the project during
  ordinary repository cleanup.
- The current UI has many routes. The product-first visible path should be:
  dashboard, charts, alarms, Wi-Fi STA/AP, notifications, Shelly, matrix, status,
  logs, help, and users. BLE, Wi-Fi sensing/CSI, USB HID, macros, file manager,
  styles, IMU, and power should remain available but be presented as advanced
  capabilities unless a specific SKU depends on them.
- The frontend already has useful quality gates in `interface/package.json`,
  especially `quality:frontend`, `quality:frontend:fast`, `ui:contract`, build
  size checks, Vitest, Svelte checks, dependency-cruiser, and Knip.

[remote-access-cloudflare]: engineering/integrations/remote_access_cloudflare.md
