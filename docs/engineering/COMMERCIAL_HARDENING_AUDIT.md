# MatrixHub Commercial Hardening Audit

This document is the execution contract for taking MatrixHub from a capable
development project to a supportable commercial product. It turns the broad
audit into one master goal with independently testable, reviewable, and
releasable stages.

## Master Goal

Make MatrixHub commercially supportable by proving CSI/alarm correctness on
real data, stabilizing the matrix rendering pipeline, unifying UI behavior and
design tokens across every route, tightening backend contracts and security,
and finishing with repeatable release and hardware gates.

The master goal is complete only when every stage below has:

1. a written scope and explicit non-goals,
2. reproducible automated tests,
3. the required real-device or visual verification,
4. a reviewed diff with no unresolved blocker,
5. updated contract/operator documentation,
6. one focused English commit pushed to `develop`.

Do not combine unfinished stages into one large commit. A stage that cannot
pass its exit gate remains open; test evidence must not be replaced with a
claim that the implementation "looks correct".

## G0 — Trustworthy Real CSI Evidence Pipeline

Status: complete on 2026-07-10. The focused closure commit is the commit that
introduces this G0 implementation and evidence; its pushed `develop` history is
the authoritative record.

Purpose: stop tuning the production detector against synthetic Python output.
Create a lossless, fail-closed path from the exact ESP32 detector inputs to a
native C++ replay fixture.

Delivered scope:

- separate admin-only `/ws/csi-capture/v1`; legacy `/ws/csi` remains unchanged,
- canonical `MHCB/MHCF v1` records with raw signed I/Q, exact float bits,
  detector time, MAC identities, PHY metadata, lengths, RX sequence, and
  `first_word_invalid`,
- exclusive START and inclusive STOP accepted-sequence fences,
- independent count/first/last completeness proofs in firmware and collector,
- exactly one fresh native replay origin without resetting the live detector,
- motion-control epoch invalidation and mandatory queue-metric snapshots,
- fd-generation isolation for queued targeted WebSocket data,
- private ignored raw artifacts, promotion-time pseudonymization, clean
  firmware provenance, and persisted human-review gates,
- native replay that invokes the real `CsiBandMotionDetector::process()` and
  never uses the Python harness or observed device output as an oracle.

Hardware evidence from the unlabelled transport smoke test:

- 137 real frames over 14.953 seconds (9.162 frames/s),
- source window `119..255`, with START boundary `118` and STOP boundary `255`,
- 384 raw I/Q bytes per frame on Wi-Fi channel 11,
- zero source/transport drops, zero truncation, zero session error flags,
- capture consumer and transport stopped cleanly while the alarm consumer kept
  CSI active.

The raw smoke artifact stays ignored and unpromoted. Its ground truth remains
`unknown`; no physical motion label was fabricated.

Exit gate:

- Python codec/workflow tests,
- native wire, sequence-window, replay, and WebSocket-session tests,
- API contract verification and Python compilation,
- ESP32 dev firmware build and upload,
- real START -> DATA -> STOP -> END -> verify cycle,
- clean lifecycle status after disconnect.

## G1 — CSI Detector And Alarm Correctness On Real Scenarios

Purpose: reproduce and fix the reported long-run inversion where motion can
clear an alarm instead of asserting it.

Required work:

- collect separate reviewed scenarios for quiet calibration, enter/walk/exit,
  occupied-but-stationary, repeated motion/quiet cycles, broad RF disturbance,
  traffic load, first-word-invalid frames, and supported channel/BSSID changes,
- add a file/scenario runner for promoted `test/fixtures/csi/*` data that reads
  `detector_config` and ground truth and reports false-positive, false-negative,
  detection-latency, hold, and clear metrics,
- validate modular monotonicity of captured `processNowMs`, including `millis()`
  wrap,
- add explicit `Fresh / Stale / Unavailable` signal state and last-frame age,
- reset candidate/clear/noisy timers after sequence/time gaps,
- define and test baseline reset semantics for reconnect, BSSID, channel, and
  CSI-width changes,
- decide detector treatment of `first_word_invalid` and test the exact rule,
- prevent slow EWMA baseline absorption from turning sustained presence into
  quiet baseline,
- separate broad RF disturbance from confirmed human motion without silently
  publishing the inverse boolean,
- replace lossy motion-edge callback delivery with a reliable mailbox/state
  reconciliation path,
- invalidate retained RTC alarm input that has no fresh post-boot evidence,
- make calibration preconditions, progress, result, and failure explicit.

Tests and evidence:

- native fixture metrics for every reviewed scenario,
- CSI -> alarm rule -> hold/cooldown -> notifier integration tests,
- source-gap, reconnect, deep-sleep, stale-data, wrap, and config-race tests,
- at least one one-hour real-device soak with scripted/operator timestamps,
- mandatory hardware reproduction of motion during reconnect and after long
  quiet/presence periods.

## G2 — Deterministic Matrix Rendering And Better Effects

Purpose: remove glitches and make the 8x8 output understandable and visually
intentional rather than merely active.

Required work:

- establish one brightness/gamma/color-correction pipeline and remove double
  quantization,
- define frame ownership, snapshot boundaries, command ordering, and buffer
  swaps so no effect can expose a partially written frame,
- audit raw-pixel paths, thermal dim/restore behavior, speed units, and command
  races,
- separate data reduction from visual mapping for CSI/RSSI/sensors,
- redesign the small set of commercial effects around legible visual purposes:
  status, trend, threshold, motion/presence, notification, and ambient mode,
- remove or demote effects that are visually noisy but communicate nothing,
- add reduced-flash and brightness-safe defaults.

Tests and evidence:

- host frame simulator with deterministic golden frames/sequences,
- bounds, NaN, speed, brightness, thermal, and concurrent-command tests,
- recorded real-matrix comparison at multiple brightness levels,
- long hardware soak with no torn frame, random pixel, freeze, or restore jump.

## G3 — Shared UI State Model, Tokens, And Accessibility

Purpose: make every screen feel like one product before polishing pages one by
one.

Required work:

- inventory and name the actual route set (currently 34 audited pages),
- define shared `Loading / Ready / Empty / Stale / Error / Disabled / ReadOnly`
  behavior with persistent retry surfaces,
- finish semantic design tokens for color, spacing, typography, radii, focus,
  motion, elevation, status, and chart/matrix visualization,
- repair BaseCard keyboard semantics, nested interactive BaseWidget markup,
  form error ARIA links, modal names/focus, and responsive card titles,
- centralize localized document titles and route/feature/role guards,
- reset stale WebSocket state consistently after disconnect/reconnect,
- implement reduced-motion behavior and stop using global overflow hiding as a
  substitute for responsive layout,
- preserve the empty gap below top navigation by keeping help/actions inside
  the relevant card header, toolbar, alert, or content surface.

Tests and evidence:

- Svelte check, lint, dependency, token, a11y, and unit gates,
- shared-state component tests for all seven states,
- keyboard-only and screen-reader smoke tests,
- mobile widths 320/360/390 and desktop widths without hidden overflow.

## G4 — Page-By-Page Functional Frontend Audit

Purpose: verify every route against its real backend behavior after the shared
foundation is stable.

Audit every page in bounded batches. For each route verify:

- direct navigation, feature flag, role, title, and help context,
- initial GET, retry, empty state, stale state, reconnect, and destructive error,
- draft versus applied state, validation, save/cancel/reset, and restart impact,
- backend DTO and error-code handling without false-success toasts,
- keyboard/focus order, accessible name, contrast, and reduced motion,
- 320px mobile through desktop layout and long translated content,
- cross-page synchronization when two tabs edit or observe the same service.

Each route gets an audit row with status, findings, automated coverage, and
manual evidence. Close batches with focused commits; do not defer all 34 pages
to one final integration commit.

## G5 — Backend Contracts, Transactions, And Security

Purpose: ensure UI consistency is backed by reliable service semantics.

Required work:

- standardize operation outcomes such as `Applied`, `Queued`, `Busy`,
  `NotReady`, and `Failed`,
- distinguish accepted work from delivered notification/test results,
- add validation and rollback for Macro, GPIO, Power, BLE, HID, and security
  settings where current writes can partially apply,
- make alarm rule updates preserve unrelated runtime state,
- align firmware DTOs, frontend schemas, and the maintained API contract,
- revalidate or expire open WebSockets after role/password/JWT-secret changes,
- close file-manager config exposure and heartbeat SSRF surfaces,
- finish production credential provisioning and secret write-only semantics.

Changes under `lib/framework/` are a separate reviewed package and require
explicit user authorization under repository policy.

## G6 — Release Hardening And Pilot Gate

Purpose: prove the integrated product, not only individual modules.

Required gates:

- all native, firmware, frontend, contract, and E2E suites green,
- full release build with embedded UI and documented size margin,
- Playwright coverage for every route, role, API failure, WebSocket reconnect,
  and mobile breakpoint,
- multi-hour CSI/alarm/matrix/network soak on real hardware,
- upgrade, reboot, deep-sleep/wake, factory reset, and corrupted-config recovery,
- per-device production credentials and recorded firmware/build identity,
- security checklist, third-party licenses, operator quick start, support and
  recovery instructions,
- merge completed `develop` to `main`, version bump, release build, tag, and
  push according to repository policy.

## Stage Closure Template

Use this checklist in every stage handoff and commit message/PR description:

```text
Scope:
Non-goals:
Findings fixed:
Automated tests:
Hardware/manual evidence:
Remaining risks moved to next goal:
Docs/contracts updated:
Diff review result:
Commit:
Push:
```
