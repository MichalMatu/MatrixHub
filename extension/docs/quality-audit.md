# Extension Quality Audit

This note captures the current quality baseline for `extension/` after the sidepanel refactor and parser hardening pass.

## Contract Matrix

| Area                    | Owner                                                                                       | Input contracts                                     | Output contracts                          | Notes                                                                |
| ----------------------- | ------------------------------------------------------------------------------------------- | --------------------------------------------------- | ----------------------------------------- | -------------------------------------------------------------------- |
| Bootstrap + persistence | `features/sidepanel/controller/sidepanelController.ts` + `infrastructure/chrome/storage.ts` | `chrome.storage.local` snapshot                     | Normalized `ExtensionState`               | Orphan sessions are dropped during load.                             |
| Auth + selection        | `features/auth/*`, `features/devices/*`, `features/sidepanel/controller/*`                  | Credentials draft, device draft, host permissions   | Session map, selected device, auth toasts | Validation and permission failures return explicit result kinds.     |
| Overview + realtime     | `features/overview/*`, `features/realtime/*`, `packages/device-sdk/src/ws.ts`               | HTTP system info, WS text frames, WS binary packets | Normalized `OverviewState` + view models  | Runtime parsers now sanitize WS payloads before they reach UI state. |
| UI rendering            | `entrypoints/sidepanel/App.svelte`, `features/*/ui/*`                                       | `SidepanelState`, overview/settings view models     | Sidepanel DOM                             | UI reads structured view models instead of raw transport payloads.   |

## Raw Data Touchpoints

- `chrome.storage.local`
  Normalized by `sanitizeExtensionState(...)` before hydration.
- `/rest/signIn`
  Wrapped by `signInSelectedDevice(...)`, which returns a typed result union.
- `/api/system/info`
  Normalized into `OverviewState` by `applyHttpOverviewSnapshot(...)`.
- `/ws/system` JSON frames
  Sanitized in `packages/device-sdk/src/ws.ts`.
- `/ws/system` binary packets
  Parsed by `parseSystemStatusPacket(...)` before entering controller state.

## Quality Axes

| Flow                   | Complexity | Bad data resilience | Session safety | Device switch safety | Testability |
| ---------------------- | ---------- | ------------------- | -------------- | -------------------- | ----------- |
| Bootstrap              | Medium     | High                | Medium         | High                 | High        |
| Add device             | Medium     | High                | High           | High                 | High        |
| Sign in                | Medium     | High                | High           | High                 | High        |
| Refresh + realtime     | Medium     | High                | High           | High                 | High        |
| Logout / remove device | Low        | High                | High           | High                 | High        |

## Remaining Follow-ups

- Add the new local lint and coverage dependencies to `extension/node_modules`, then run `npm run lint` and `npm run test:coverage`.
- If firmware expands diagnostics payloads, add matching parsers in `packages/device-sdk/src/ws.ts` before exposing the fields in view models.
