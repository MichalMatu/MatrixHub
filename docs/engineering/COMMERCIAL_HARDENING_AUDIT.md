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

Hardware-proven stages use two provenance-preserving commits when necessary:
first a clean candidate commit that is built and flashed, then a focused
evidence/closure commit containing reviewed fixtures, reports, and final docs.
Do not squash away the candidate SHA: promoted captures must name the exact
clean 40-hex firmware commit that produced them. The second commit is the stage
closure commit for checklist item 6.

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

Status: implementation candidate prepared; stage remains open pending reviewed
real scenarios and the one-hour hardware soak. Synthetic/native tests are
regression evidence, not RF acceptance evidence.

Purpose: reproduce and fix the reported long-run inversion where motion can
clear an alarm instead of asserting it.

Candidate findings addressed:

- boolean alarm sources are canonical `Above 0.5`; stale persisted `Below`
  operators cannot invert CSI, IMU tamper, or digital GPIO semantics,
- a true CSI edge cannot be coalesced away by a later false input before the
  alarm coordinator runs, and a busy coordinator leaves durable retry work,
- stale, invalid, broad-noise, reconnect, and gain-transition states are
  unknown decisions and cannot manufacture a clear edge,
- invalid frames break both motion-candidate and quiet-clear timers, so time
  without usable CSI cannot satisfy either hold window,
- active/RTC-retained motion survives source/width/config/storage recovery and
  requires explicit quiet-room calibration before it may clear,
- manual calibration fails closed unless CSI runtime, detector storage, forced
  gain, configured bands, and a fresh valid frame are all present,
- source pinning, first-word-invalid carrier exclusion, time-based baseline
  adaptation, wrap-safe hold/cooldown, and frame-gap timer reset have native
  regression coverage,
- desired CSI consumers survive lifecycle lock/start/stop failures; a periodic
  5–60 second reconciler repairs desired/runtime drift and deferred cleanup,
- deferred frontend/capture lifecycle work is generation-backed, binds each
  deadline to its generation under a cross-core critical section, retries task
  allocation from the main loop, and is fenced before CSI/RSSI shutdown,
- capture disconnect/rollback cleanup is bound to the current `(fd, session
  generation)` owner, cannot be replaced by an unrelated socket, and remains
  durable after session-lock timeout; late completion from an old generation
  cannot clear a newer capture; ordinary API destruction drains both WebSocket
  queues, while restart/factory-reset/deep-sleep fences producers and requests
  a non-blocking queue stop because those hooks may run on the `httpd` task that
  a synchronous WebSocket sender is waiting for; queue/task/pool ownership is
  retained until the terminal hardware transition,
- WebSocket producers hold one lease across payload reservation,
  serialization/copy, enqueue/rollback, and cleanup; shutdown drains those
  leases before queue/pool disposal, while CSI frontend and capture delivery
  are queued-only and never fall back to a synchronous `httpd` send,
- capture START is admitted only when the CSI service is actually runtime-ready
  with its diagnostic consumer and queue metrics available; desired bits or a
  retained queue cannot impersonate a live pipeline, and failed error enqueue
  closes the capture socket instead of continuing ambiguously,
- RSSI runtime repair cannot apply a stale settings snapshot over a newer HTTP
  update; the configuration generation and apply mutex preserve lock order,
  while an applied-runtime snapshot detects enabled/interval/threshold drift
  even when the worker still reports itself as running,
- RSSI sampler allocation, indices, readers, and teardown share one ownership
  mutex; a timed-out deinit retains the PSRAM buffer for retry instead of
  freeing it under an active reader, and periodic runtime reconciliation now
  runs in a persistent worker rather than blocking `loopTask`,
- driver callbacks use a process-lifetime owner fence, processed-data callbacks
  drain before API destruction, and terminal shutdown rejects late re-enable,
- CSI and GPIO alarm callbacks are wired before their runtime can publish; the
  CSI bootstrap gate preserves retained motion while constructor-default
  settings are being replaced, and a wiring/bootstrap failure aborts service
  initialization instead of publishing a temporary false state,
- IMU retained tamper is restored before the first runtime tick; stale,
  unavailable, non-finite, or baseline-incomplete samples are unknown decisions
  that restart the fresh clear hold rather than clearing the retained alarm,
- alarm-rule edits migrate runtime by canonical runtime identity, so
  reorder, naming, severity, channel, and cooldown edits cannot clear retained
  CSI or re-arm unrelated rules; explicit disable/delete/semantic changes reset
  only the affected state,
- the RTC alarm summary carries stable 64-bit semantic identities (schema 48):
  ID, enabled state, source, BLE/GPIO selector, and for analog rules the
  operator plus canonical float threshold; a torn LittleFS/RTC commit therefore
  cannot attach old active runtime to a same-ID rule with new semantics,
- warm LittleFS hydration leaves the complete retained runtime summary intact
  until identity mapping is finished; an RTC commit failure restores the
  evaluation before-image and prevents Matrix, Shelly, notifier and WebSocket
  effects from acknowledging an unretained edge,
- rule updates, pending actions, Shelly reconciliation, and Matrix alarm refresh
  share one `process -> manager` transaction; shared Shelly bindings use global
  OR semantics instead of last-writer-wins,
- Shelly relay intent is latest-wins and generation-backed outside its wake
  queue; queue saturation and transient HTTP failure cannot discard the final
  OFF, stale ACKs cannot clear a newer intent or acknowledge a reconfigured
  IP/relay peer, delayed-stop workers can be restarted without losing intent,
  and allocation-free boot wiring republishes each current binding from
  retained global-OR state without waiting for a new edge,
- Shelly lookup distinguishes mutex contention from a missing device, peer
  revisions fence polling and relay ACKs, task-start failures retain retryable
  ledger work, config changes reconcile the global OR without a new sensor
  edge, and outbox preflight rejects a rule transaction before capacity can be
  exceeded,
- alarm JSON rejects duplicate, empty, overlong, excess, or wrong-typed Shelly
  bindings instead of silently truncating them, and a failed filesystem
  rollback still reapplies the previous runtime snapshot,
- direct and JSON alarm updates share fail-closed validation for enum ranges,
  notification masks, finite/ranged thresholds, BLE/GPIO selectors, duplicate
  names/IDs and bounded Shelly arrays; 24-hour cooldowns use `uint32_t` without
  the former 16-bit wrap,
- alarm boot refuses to continue after incomplete RTC/rule-store reads or a
  failed retained commit; initialized state is atomic, retained Matrix output
  is rebuilt immediately, and same-severity alarm-name changes refresh the real
  display controller,
- an existing malformed config, missing critical alarm section, or missing
  canonical CSI/RSSI/IMU/GPIO dependency for an enabled rule aborts the boot
  load; warm PSRAM hydration preserves retained state and aborts instead of
  falling through to runtime defaults,
- boolean edge mailboxes now preserve CSI motion, IMU tamper, and debounced
  selector-scoped GPIO true-to-false transients until the coordinator commits
  and acknowledges each pass; GPIO changes wake the alarm pipeline directly,
  and removing or disabling a channel publishes a definitive clear,
- REST/UI status exposes decision validity, freshness, frame age, runtime fault,
  and reconciliation state; the UI does not offer calibration before the same
  fail-safe prerequisites used by firmware,
- firmware builds expose embedded Git SHA/dirty identity and the collector
  verifies it against the expected flashed commit before creating evidence.

Remaining evidence gates:

- promote reviewed real-device fixtures covering the complete scenario list,
- explicitly exercise detach/reconnect/reattach to detect any stale frame from
  the closed-source driver callback generation boundary,
- run motion during reconnect plus long quiet and sustained-presence cases,
- complete and retain the one-hour timestamped soak with zero unexplained alarm
  inversion, missed clear, false clear, callback/lifecycle fault, or capture
  loss.

Remaining work before G1 closure:

- collect and human-review separate real scenarios for quiet calibration,
  enter/walk/exit, occupied-but-stationary, repeated motion/quiet cycles, broad
  RF disturbance, traffic load, first-word-invalid frames, and supported
  channel/BSSID changes,
- promote only fixtures whose embedded clean firmware identity matches the
  candidate SHA and whose labels/review metadata pass the strict replay gate,
- run the reconnect/presence/dropout hardware matrix and one-hour soak, then
  record results in the evidence/closure commit without rewriting the candidate
  SHA that produced the captures,
- verify Shelly eventual convergence on hardware with Wi-Fi/HTTP outage, an
  in-flight ON superseded by OFF, worker restart and boot with retained active
  and inactive bindings,

Explicitly deferred to G5 (not G1 closure gates):

- keep the hard-power-loss case after deleting the final alarm binding scoped
  to G5 unless a durable alarm-ownership journal is added; current boot
  reconciliation intentionally controls current bindings and does not assume
  ownership of unrelated/manual-only Shelly devices,
- decide and implement the G5 delivery guarantees for notification outbox,
  config-file backup recovery/whole-snapshot serialization, and Shelly pending
  intent across deep sleep; none of these may be presented as guaranteed by
  the G1 in-memory retry mechanisms,
- add a durable provisioned-device/factory-reset-intent marker; a wholly absent
  config file still follows the explicit factory-default policy, which must not
  be allowed to masquerade as an intentional reset after storage loss,
- full raw route-dispatch request-lifetime fencing remains a framework-level G5
  change; G1 fences local callbacks, workers, queues, and handler bodies but
  does not claim that the shared dispatcher cannot enter after owner teardown.

Tests and evidence:

- native fixture metrics for every reviewed scenario,
- CSI -> alarm rule -> hold/cooldown -> notifier integration tests,
- retained-state migration, torn-commit semantic identity with stable golden
  vectors, warm-hydration preservation, RTC failure rollback, alarm-update
  concurrency, boot fail-closed behavior, strict rule/cooldown boundaries,
  CSI/IMU edge mailboxes, retained Matrix restore, shared-Shelly OR, durable
  Shelly intent, task-allocation retry, API shutdown, and settings drift/race
  tests,
- source-gap, reconnect, deep-sleep, stale-data, wrap, and config-race tests,
- at least one one-hour real-device soak with scripted/operator timestamps,
- mandatory hardware reproduction of motion during reconnect and after long
  quiet/presence periods.

Current automated candidate evidence (2026-07-11):

- complete native matrix: 159 suites and 1,239/1,240 succeeded test cases, with
  one intentional skip for the not-yet-reviewed real CSI corpus and zero
  failures,
- focused post-audit suites pass WiFi/API 49/49, Shelly 64/64, the alarm
  pipeline 32/32, IMU detector/initializer 14/14, service-registry init 10/10,
  critical config 9/9, and CSI retained bootstrap 2/2,
- ESP32-S3 developer build: success; 3,159,363 / 3,502,080 bytes flash
  (90.2%), and the full release build with generated UI embedded succeeds at
  3,159,251 bytes flash; both use 82,392 / 327,680 bytes RAM
  (25.1%) and 2,220 / 7,680 bytes RTC SLOW (28.91%),
- frontend: 142/142 Vitest files and 654/654 assertions passed, Svelte reports
  zero errors/warnings, Prettier/ESLint and dependency checks pass, and the
  static build passes its size gates at 426.21 KB gzip JS, 39.71 KB maximum
  chunk, 30.46 KB gzip CSS, and 1.44 MB total; the two known UI-contract date
  controls plus six unused values/two types remain explicitly scoped to G3,
- capture/identity Python tests pass 27/27, the device SDK typecheck and 11/11
  tests pass, and API-contract verification covers 85 paths across 390 source
  occurrences,
- the strict real-corpus runner fails closed while `test/fixtures/csi` is empty;
  this expected failure is the reason the real-scenario and soak gates remain
  open rather than being inferred from synthetic/native data,
- independent post-fix reviews found no remaining P0/P1 in capture ownership,
  queue producer lifetime, runtime admission, alarm bootstrap/config
  dependencies, GPIO/IMU retention, Shelly convergence, shutdown ordering, or
  deferred-worker wakeups; the shared raw request-lifetime fence remains
  explicitly assigned to G5.

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

Findings already queued by the G1 cross-project quality run:

- `ui:contract` currently fails its zero-debt `rawStyledControl` gate on the two
  `type="date"` controls in `charts/components/DateSelector.svelte`; migrate
  both to the existing shared `FormInput` while preserving normalized
  `onchange` behavior, then keep the contract at zero rather than adding a
  baseline exception,
- `unused:check` currently reports six exported values and two exported types
  across GPIO, Matrix, Power, and USB Terminal modules; remove or internalize
  them only after verifying external/package consumers,
- the pre-commit hook runs lint/check but did not run `ui:contract`, allowing
  the zero-debt regression to enter `develop`; align local and CI gates so a
  contract declared mandatory cannot be skipped silently.

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
- serialize complete config snapshot creation and atomic write under one lock,
  and recover a validated `.bak`/`.tmp` after power loss before falling back to
  defaults,
- define a durable notification outbox/ack policy so a committed alarm edge
  cannot silently consume cooldown after every delivery queue rejects it,
- persist explicit alarm ownership/last desired state for Shelly outputs and
  define a bounded deep-sleep flush/interlock policy,
- validate and arm every wake source before terminal pre-sleep teardown, or
  provide a complete runtime recovery path when deep-sleep entry is cancelled,
- align firmware DTOs, frontend schemas, and the maintained API contract,
- fence and drain full REST/WebSocket request lifetimes during shutdown rather
  than only their activity callbacks,
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
