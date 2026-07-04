# MatrixHub Chrome Extension

This folder contains the Chrome side panel extension built with `WXT + Svelte`.

## What lives here

- `entrypoints/background.ts`
  Sets side panel behavior so clicking the toolbar icon opens the panel.
- `entrypoints/sidepanel/App.svelte`
  Thin composition layer: renders the panel, wires UI events and keeps toast
  rendering local.
- `src/lib/features/sidepanel/*`
  Sidepanel state model and controller that own bootstrap, selection, auth,
  persistence and realtime orchestration.
- `src/lib/features/sidepanel/ui/*`
  Sidepanel-only presentation building blocks such as the header, add-device
  sheet and toast region.
- `src/lib/features/devices/*`
  Device list UI, add/remove flows and device draft validation.
- `src/lib/features/devices/ui/deviceCard/*`
  Internal building blocks for the device card so header, auth and signed-in
  sections can evolve independently.
- `src/lib/features/auth/*`
  Login card, credential validation and session restore/sign-in workflows.
- `src/lib/features/overview/*`
  Telemetry mapping, snapshot actions and realtime state helpers.
- `src/lib/features/realtime/*`
  WebSocket transport and realtime connection state.
- `src/lib/infrastructure/chrome/*`
  Chrome cookies, permissions and storage adapters.
- `src/lib/infrastructure/device/*`
  Authenticated API factories for the selected device.
- `src/lib/domain/*`
  Reusable rules (`device/rules.ts`) and cross-feature errors/results in
  `shared/`.

## How the extension connects to a device

1. The user adds a device by IP or mDNS hostname.
2. `normalizeDeviceOrigin(...)` turns that into a clean origin such as
   `https://matrixhub.local` or `https://192.168.4.1`.
3. `ensureDeviceOriginPermission(...)` requests host permission for that origin.
4. `signInToDevice(...)` performs HTTP login against `/rest/signIn`.
5. The returned access token is stored in `sessions` and persisted in
   `chrome.storage.local`.
6. `syncWsAccessTokenCookie(...)` writes the same token as the `access_token`
   cookie for the `/ws` path.
7. `refreshOverview()` fetches the first HTTP snapshot.
8. `DeviceOverviewSocket.connect(...)` opens `ws://.../ws/system` or
   `wss://.../ws/system`, subscribes to channels and asks for snapshots.

## Sidepanel architecture

The side panel is intentionally split into layers:

- `App.svelte`
  Composes feature components and delegates stateful behavior to the controller.
- `src/lib/features/sidepanel/*`
  Owns bootstrap + persistence, auth + selection and overview + realtime flows.
- `src/lib/features/sidepanel/controller/sessionRuntime/*`
  Splits session orchestration into context, refresh and lifecycle helpers so
  the public controller stays small.
- `src/lib/features/*`
  Organizes feature-specific UI, view models and workflows.
- `src/lib/infrastructure/*`
  Hides Chrome APIs and device transport setup behind small adapters.
- `src/lib/domain/*`
  Holds shared rules, result types and error mapping used across features.

This means future work should usually follow this rule:

- new device/auth/overview behavior: `src/lib/features/<feature>/*`
- new cross-feature sidepanel orchestration: `src/lib/features/sidepanel/*`
- new Chrome or API adapter: `src/lib/infrastructure/*`
- new reusable validation/error rule: `src/lib/domain/*`
- only glue code or section composition: `App.svelte`

## Why the cookie exists

HTTP requests use a Bearer token via the shared `@matrixhub/device-sdk`, but the
current ESP32 WebSocket implementation authenticates with a cookie. The
extension mirrors the token into a cookie so HTTP and realtime flows can use the
same login.

If the device firmware later supports WS Bearer auth or a dedicated WS token,
`src/lib/infrastructure/chrome/cookies.ts` is the first place to revisit.

## How disconnect works

- `overviewSocket.disconnect()`
  Stops the active socket and sets `manualClose = true`, which prevents the
  reconnect timer from bringing the connection back.
- `logout()`
  Clears the WebSocket cookie, disconnects the socket and removes the stored
  session for the selected device.
- `removeDevice(deviceId)`
  Disconnects the socket, clears that device's cookie, removes its session and
  removes the saved device record.
- `selectDevice(deviceId)`
  Disconnects the current socket, clears volatile state and restores the next
  device session if one exists.

## Debugging checklist

If the panel opens but cannot connect:

1. Check `chrome://extensions` and open the extension errors page.
2. Confirm the device origin was granted in extension site permissions.
3. Open the device origin in a normal tab first if it uses self-signed HTTPS.
4. Verify login still works against `/rest/signIn`.
5. Verify the `access_token` cookie exists for the device `/ws` path.
6. Check whether HTTP works but WS fails. If yes, focus on
   `infrastructure/chrome/cookies.ts` and
   `features/realtime/socket/deviceOverviewSocket.ts`.
7. Check whether a reconnect loop is happening. If yes, inspect `manualClose`,
   `onclose` and `scheduleReconnect()` in
   `features/realtime/socket/deviceOverviewSocket.ts`.

## Local build and reload

- Install deps: `npm install`
- Build: `npm run build`
- Type-check: `npm run check`
- Lint: `npm run lint`
- Coverage: `npm run test:coverage`
- Full local gate: `npm run verify`
- Load unpacked extension from `.output/chrome-mv3`
- After code changes, run `npm run build` and press `Reload` in
  `chrome://extensions`

## CI

- GitHub Actions runs `npm run verify` for changes under `extension/` and
  `packages/device-sdk/`

## Shared code

The extension depends on `../packages/device-sdk`. That package is shared with
the main `interface/` app so request helpers, auth helpers and WS parsing stay
consistent.

Removing `extension/` is safe for the main project.
Removing `packages/device-sdk/` is not safe unless the related imports are also
removed from `interface/`.
