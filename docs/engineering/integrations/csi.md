Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Integrations](../README.md#integrations-and-specialized-subsystems)

# Wi-Fi CSI

This document is the maintained reference for the current Wi-Fi CSI runtime
path. It covers the firmware data flow, the API contract, alarm separation, and
the matrix-display visualization caveats.

## Scope

Wi-Fi CSI currently serves three independent consumers:

- `/ws/csi` diagnostics and browser charts
- `wifi_csi_motion` alarm input
- matrix LED data visualization

RSSI variance motion detection remains separate as `wifi_motion`. CSI alarm
configuration must not change RSSI behavior, and matrix visualization must not
depend on alarm baseline readiness.

## Runtime Path

The active firmware path is:

```text
ESP Wi-Fi CSI callback
  -> CsiDataQueue
  -> CsiService::processingTask()
  -> CsiGainController
  -> CsiVisualizationReducer
  -> CsiBandMotionDetector
  -> /ws/csi, matrix visualization snapshot, alarm motion callback
```

Important ownership rules:

- The Wi-Fi CSI callback runs in the Wi-Fi task and copies data only. It must not
  allocate, block, touch PSRAM-backed handoff buffers, or run detector logic.
- `CsiDataQueue` stores packet data and queue metadata in internal RAM so the
  Wi-Fi RX path is not exposed to flash/PSRAM cache faults.
- `CsiService::processingTask()` owns packet processing. Its stack and task
  control block stay in internal RAM; the transient WebSocket batch buffer is
  allocated in PSRAM.
- CSI is enabled only while at least one consumer is active.
- Consumer bits represent desired ownership. They are committed before bounded
  lifecycle locking so a start/stop timeout cannot erase the request. A
  low-frequency reconciler retries desired/runtime drift with 5–60 second
  backoff and reaps deferred task/queue cleanup.
- The Wi-Fi driver receives a process-lifetime callback fence rather than a
  `CsiService*`. Detach clears the owner before unregister/drain, so even a
  driver-dispatched callback that enters late cannot dereference a destroyed
  service or queue.

Current CSI consumers are defined by `WIFISENSING::CSI::CsiConsumer`:

| Consumer | Purpose |
| --- | --- |
| `Frontend` | Keeps CSI active while `/ws/csi` has clients. |
| `AlarmSystem` | Keeps CSI active for `wifi_csi_motion` without the UI open. |
| `Boot` | Reserved boot/runtime consumer. |
| `MatrixVisualization` | Keeps CSI active when matrix data visualization uses Wi-Fi CSI. |
| `DiagnosticCapture` | Keeps CSI active only for an explicit raw capture session. |

## WebSocket Wire Format

`/ws/csi` emits one or more concatenated records per WebSocket message.

Each record is:

```text
uint32  timestamp
int8    rssi
uint16  payload length
uint8   gain compensation * 10
float32 motionScore
uint8   isMotionDetected
int8[]  interleaved I/Q payload
```

The header is 13 bytes. Frontend and tooling must keep this aligned with:

- `src/api/wifisensing/CsiWireFormat.*`
- `interface/src/lib/features/wifisensing/csi/parseCsiFrame.ts`
- `tools/csi_client.py`
- `scripts/csi_monitor.py`

Receivers should parse records until the payload is exhausted. A partial
trailing record is malformed and should be dropped.

The browser stream is intentionally compact and is not a lossless detector
fixture format. It quantizes gain and omits radio/source metadata. Do not use
`/ws/csi` captures or `scripts/sensing_analysis/collect_long_data.py` as input
to detector acceptance tests.

## Lossless Capture And Native Replay

Real detector fixtures use the admin-only diagnostic endpoint:

```text
/ws/csi-capture/v1
```

The endpoint is inactive until an authenticated admin client sends the exact
text command `START`. It replies with `HELLO`, streams canonical `DATA`
records, and finishes only after the same client sends `STOP` and receives the
FIFO-fenced `END` record. `HELLO.rxAcceptedStart` is an exclusive source
boundary and STOP snapshots an inclusive source boundary. A promotable file
must contain every accepted sequence in that exact modular interval: the first
record is `start + 1`, the last is the STOP boundary, and the record count is
the distance between them. This catches a missing first or boundary packet in
addition to gaps inside the file.

While valid CSI DATA continues to flow, the capture session refreshes the power
activity lease every 30 seconds. This keeps long real-data sessions from being
cut off by the normal five-minute-or-longer inactivity sleep policy without
changing the user's persisted power configuration.

A disconnected session, missing `HELLO`/`END`, incomplete source window,
WebSocket queue drop, truncated input, motion-control epoch change, or session
error flag makes the capture incomplete and therefore ineligible for promotion.
The global source-queue drop delta remains diagnostic because packets after the
inclusive STOP boundary may legitimately change it; loss inside the capture is
proved precisely by the accepted-sequence window. Queue metrics are mandatory
at START, and an unavailable snapshot fails closed instead of silently
reporting zero drops.

Raw captures use the little-endian MatrixHub CSI Fixture format (`MHCF` v1):

- a 32-byte file header with version, endian marker, session, frame count, and
  exact frame-section size
- canonical 64-byte frame headers followed by the unmodified signed I/Q bytes
- exact float32 gain bits and the exact `processNowMs` passed to the production
  detector
- source/destination identity, RX sequence, `first_word_invalid`, lengths,
  RSSI, and canonical PHY metadata
- observed motion score/state as diagnostic parity evidence only

Canonical `MHCF` file-header offsets are:

| Offset | Type | Meaning |
| ---: | --- | --- |
| 0 | `char[4]` | `MHCF` |
| 4 | `uint8,uint8` | major/minor (`1.0`) |
| 6 | `uint16` | file-header bytes (`32`) |
| 8 | `uint32` | endian marker (`0x01020304`) |
| 12 | `uint16` | frame-header bytes (`64`) |
| 14 | `uint16` | flags, zero in v1 |
| 16 | `uint32` | capture session ID |
| 20 | `uint32` | final frame count |
| 24 | `uint64` | exact frame-section bytes |

Every frame starts with this canonical header; all multi-byte values are little
endian:

| Offset | Type | Meaning |
| ---: | --- | --- |
| 0 | `uint16` | whole record bytes (`64 + storedLen`) |
| 2 | `uint16` | header bytes (`64`) |
| 4 | `uint32` | accepted CSI sequence |
| 8 | `uint32` | exact detector `processNowMs` |
| 12 | `uint32` | Wi-Fi RX timestamp in microseconds |
| 16 | `float32 bits` | exact compensation gain |
| 20 | `float32 bits` | observed score, diagnostics only |
| 24 | `uint16` | original I/Q length |
| 26 | `uint16` | stored I/Q length |
| 28 | `uint16` | Wi-Fi RX sequence |
| 30 | `uint16` | signal length |
| 32 | `uint8[6]` | source identity |
| 38 | `uint8[6]` | destination identity |
| 44 | `int8,int8` | RSSI and noise floor |
| 46..60 | `uint8[15]` | rate, signal mode, MCS, CWB, smoothing, not-sounding, aggregation, STBC, FEC, SGI, AMPDU count, channel, secondary channel, antenna, RX state |
| 61 | `uint8` | bit 0 first-word-invalid, bit 1 truncated, bit 2 observed motion, bit 3 replay origin |
| 62 | `uint16` | reserved, zero in v1 |
| 64 | `int8[storedLen]` | unmodified interleaved I/Q |

WebSocket batches use a 16-byte `MHCB` header: magic at offset 0, version at
4, message type at 6, header size at 7, canonical record-header size at 8,
record count at 10, and session ID at 12. `HELLO` and `END` use fixed 40- and
64-byte control payloads respectively. `HELLO.rxAcceptedStart` is exclusive;
`END.rxAcceptedEnd` carries the inclusive STOP fence captured before drain, not
a later global-counter sample. Encoder, collector, and native replay tests must
change together if any offset changes.

Exactly the first record carries `replay origin`. It defines a fresh native
detector run for deterministic fixture replay; capture does **not** reset or
otherwise mutate the live production detector. Consequently, captured
score/motion fields describe the already-running device only and are diagnostic
evidence, not replay expectations.

The native replay starts a fresh detector at that origin and calls the real
`CsiBandMotionDetector::process()` for every record using the captured
`processNowMs`. Python must never implement the detector verdict or treat the
captured observed score/state as expected behavior.

### File Lifecycle

Collect into ignored local artifacts:

```bash
python scripts/sensing_analysis/csi_capture.py collect \
  --device-url https://192.168.0.18 \
  --scenario-id long-motion-inversion-01 \
  --duration 30m \
  --firmware-commit <git-sha-of-flashed-build> \
  --description "Repeated walk/quiet cycles in a fixed room"
```

`--firmware-commit` is mandatory and must be the full 40-hex commit actually
flashed. A local transport smoke test may use `<sha>-dirty`, but promotion
rejects dirty or unknown provenance. ESP32 builds embed their Git SHA and dirty
bit in `/api/system/info`; before creating any artifact, the collector compares
that device-reported identity with the CLI expectation. The scenario records
only the verified device value, so an accidentally mislabeled or different
flash fails closed. Promotion and native replay additionally require
`firmware_dirty=false`, the collector verification marker, and the remaining
firmware identity fields reported by the device.

Before creating an artifact, the collector verifies that the CSI alarm is
enabled with at least one valid configured band, CSI is running with an
allocated queue, no runtime fault or reconciliation is pending, gain calibration
has reached `forced`, and the active detector has received a fresh valid frame.
A failed preflight leaves no `.partial` directory; correct the device
configuration and retry.

The collector writes:

```text
artifacts/csi/raw/<UTC>-<scenario-id>/
  frames.mhcf
  capture.json
  scenario.json
```

`artifacts/` is ignored by Git. An interrupted or invalid run remains in a
`.partial` directory with an incomplete manifest and must not be renamed or
committed manually. Inside this repository, `collect --output-root` is accepted
only below ignored `artifacts/`; raw directories and files are created with
owner-only `0700`/`0600` permissions. Promotion is the only supported path into
`test/fixtures/csi`.

Add operator ground truth using half-open intervals relative to the first
captured `processNowMs`:

```bash
python scripts/sensing_analysis/csi_capture.py annotate ARTIFACT_DIR \
  --from 2m --to 3m \
  --motion present \
  --occupancy occupied \
  --environment stable \
  --evidence operator \
  --confidence high \
  --note "Continuous walking across the room"

python scripts/sensing_analysis/csi_capture.py annotate ARTIFACT_DIR --review

python scripts/sensing_analysis/csi_capture.py acceptance ARTIFACT_DIR \
  --max-false-positive-ms 2s \
  --max-false-negative-ms 4s \
  --max-invalid-decision-ms 4s \
  --max-detection-latency-ms 2s \
  --max-clear-latency-ms 4s \
  --max-motion-dropout-ms 500ms \
  --max-missed-motion-intervals 0 \
  --max-uncleared-transitions 0

python scripts/sensing_analysis/csi_capture.py verify ARTIFACT_DIR
python scripts/sensing_analysis/csi_capture.py report ARTIFACT_DIR
```

Ground truth describes physical reality, never the current detector result.
The v1 vocabulary is:

- `motion`: `none`, `present`, `unknown`
- `occupancy`: `empty`, `occupied`, `unknown`
- `environment`: `stable`, `rf_disturbance`, `reconnect`, `unknown`
- `evidence`: `operator`, `scripted_action`, `video_timestamp`, `unknown`
- `confidence`: `high`, `medium`, `low`, `unknown`

Unknown transition intervals are valid, but a reviewed scenario must contain at
least one interval with known motion and complete context/evidence fields.
Choose and persist scenario-specific acceptance limits before using a candidate
detector replay to judge the capture. The values above illustrate the command,
not universal product limits. `max_false_positive_ms` and
`max_false_negative_ms` limit total misclassified known time, while
`max_invalid_decision_ms` limits known time for which the production detector
does not expose a valid alarm decision.
`max_detection_latency_ms`, `max_clear_latency_ms`, and
`max_motion_dropout_ms` limit the worst event/run. The remaining limits count
completely missed motion windows and motion-to-quiet transitions that never
clear. Time labeled `unknown` is reported separately and is excluded from the
confusion metrics. Label detector warm-up/calibration as `unknown`; otherwise
its invalid decision time is intentionally charged against the reviewed limit.
The same `acceptance` command upgrades a reviewed G0 raw/promoted scenario that
predates the acceptance block; release verification still rejects it until all
limits have been explicitly supplied.

Promote only a lossless capture whose ground truth was already persisted as
reviewed with `annotate --review`; `--reviewed` is an additional explicit
operator attestation, not a shortcut around that stored review:

```bash
python scripts/sensing_analysis/csi_capture.py promote ARTIFACT_DIR \
  --fixture-id csi-long-motion-inversion-01 \
  --reviewed
```

Promotion writes only `frames.mhcf` and `scenario.json` under
`test/fixtures/csi/<fixture-id>/`. It maps every captured MAC/BSSID identity to
non-reversible locally administered pseudonyms while preserving equality
relationships. It excludes device URL/IP, SSID, credentials, the device MAC
from system metadata, and all other raw manifest fields. Never copy a raw
capture directly into `test/fixtures/`. Free-text descriptions and annotation
notes are also scanned fail-closed for SSID/BSSID labels, hostnames, IP/MAC
addresses, email addresses, tokens, and secrets; keep identifying room/network
details out of those fields.

Run every promoted, reviewed real-device scenario through the exact production
`CsiBandMotionDetector` with:

```bash
python scripts/tests/run_csi_fixture_replay.py
```

The wrapper first validates MHCF structure, modular `processNowMs` ordering,
hashes, provenance, human ground truth, and reviewed acceptance limits. It then
passes the corpus to the native PlatformIO runner through
`MATRIXHUB_CSI_FIXTURE_ROOT`. The wrapper fails if the corpus is absent or empty,
if a fixture contains anything other than `frames.mhcf` and `scenario.json`, or
if any acceptance limit is exceeded. The ordinary `pio test -e native` run keeps
the same test visible as `SKIPPED` when no real corpus was explicitly enabled;
synthetic frames exercise only codec, parser, and metric unit behavior and are
never accepted as release evidence. Across the enabled corpus, the native gate
also requires at least one reviewed motion-present window and one transition
from motion to known quiet, so a quiet-only fixture cannot falsely exercise the
detection, dropout, and clear gates. This minimum does not replace the complete
G1 scenario list below.

Start the real-data corpus with separate captures for quiet calibration,
enter/walk/exit, occupied-but-stationary, repeated motion/quiet cycles over at
least one hour, broad disturbance without human motion, traffic load without
motion, channel/BSSID changes that do not break the capture connection, and
frames marked `first_word_invalid`. Keep transitions explicitly labeled
`unknown` rather than guessing.

A full STA disconnect or deep-sleep wake also disconnects this WebSocket, so it
cannot produce one complete v1 capture across that boundary. Preserve the
separate before/after raw sessions, but do not concatenate or promote them as a
single fixture until MHCF gains an explicit typed lifecycle-event section and
the native replay defines its reset semantics.

## Status And Control

Primary endpoints:

```text
GET  /api/wifisensing/status
GET  /api/wifisensing/config
POST /api/wifisensing/config
POST /api/wifisensing/csi/calibrate
```

`/api/wifisensing/status` returns:

- RSSI sensing state and statistics
- CSI consumer state and queue pressure
- CSI packet, batch, and WebSocket counters
- explicit `runtime_fault` and `runtime_reconcile_pending` lifecycle state
- CSI motion detector state, score, confidence, selected carrier count, and
  reset reason, plus `decision_valid`, `has_frame`, `data_fresh`, and frame age

The calibration endpoint applies to the alarm detector. Matrix visualization is
display-only and should not require calibration or alarm baseline readiness.
`POST /api/wifisensing/csi/calibrate` returns HTTP 503 with
`csi/calibration_unavailable` unless the alarm is enabled with at least one
band, runtime and detector storage are healthy, gain is forced, and a fresh
valid non-`Unavailable` frame exists. This fail-safe is intentional: accepting
calibration during an outage could forget a retained active alarm and learn an
occupied room as quiet.

## Alarm Semantics

The alarm engine consumes only `wifi_csi_motion` as a boolean-like value:

```text
false -> 0.0
true  -> 1.0
```

`motionScore`, confidence, selected carriers, and noisy state are diagnostics.
They are useful in the CSI UI and status endpoint, but they should not be used
directly by alarm rules.

The detector implementation is `CsiBandMotionDetector`:

- gain-compensated per-subcarrier energy
- quiet-room baseline and noise floor
- selected-band top-K z-score
- hysteresis and hold/clear-hold windows
- noisy-environment gating
- serialized, fail-closed manual calibration request path

Binary publication is fail-safe:

- only `Monitoring` and `MotionConfirmed` are definitive decisions,
- `Unavailable`, stale, noisy, invalid-frame, reconnect, baseline collection,
  and required-calibration states preserve the last stable boolean but do not
  publish a clear,
- an active decision retained across RTC boot or detector storage recovery is
  reapplied before any automatic baseline can run and remains latched until an
  operator-authorized calibration produces fresh definitive quiet evidence,
- disabling the alarm is the explicit exception and publishes one durable
  `false` synchronization value.

The alarm bridge preserves a true edge even if a false sample arrives before
the main-loop coordinator pass. Failed coordinator lock attempts remain queued,
and boolean-like rule sources are always evaluated as `Above 0.5`; persisted
inverse operators cannot turn motion into a clear-trigger rule.

### Lifecycle Residual To Prove On Hardware

The stable callback fence closes the lifetime/UAF window, but the public
Espressif CSI API does not expose a registration-generation token. A callback
dispatched under an old registration and delayed until after detach/reattach
could theoretically enter the new generation as one stale frame. The detector
hold/gain-reset rules make one frame non-authoritative, but reconnect captures
and the one-hour soak must still verify that no stale frame crosses the real
driver boundary. This is a hardware acceptance gate, not a reason to synthesize
an expected result in Python.

## Matrix Visualization

Matrix data visualization uses `CsiVisualizationReducer`, not
`CsiBandMotionDetector`. This is intentional:

- visualization does not depend on `motion.baselineReady`
- alarm settings do not change the displayed CSI shape
- the matrix path can run even when CSI alarms are disabled

The current 8x8 visualization is an MVP. It reduces raw CSI to 64 bins and a
0..100 display value. It is useful for showing that CSI is alive, but subtle
room-motion changes may remain weak on an 8x8 matrix because the live amplitude
shape often changes slowly and broadly rather than as a large frame-to-frame
spike.

The matrix status endpoint exposes the `MatrixVisualization` consumer state,
packets per second, last CSI packet timestamp, current bin count, and the fresh
data reason used by the LED renderer. The legacy
`/api/matrix/data-visualization/csi/calibrate` route is intentionally kept as a
compatibility alias, but it now requests a visualization reducer reset. The HTTP
handler only sets a reset flag; the CSI processing task applies the reset before
processing the next packet so reducer mutation remains single-threaded.

## Future Work

Do not tune CSI visualization by repeatedly changing constants unless the
feature becomes a product priority. A better future implementation should be a
separate display-only pipeline:

1. Keep a fast EMA and a slow EMA per bin.
2. Render `abs(fast - slow)` for motion visibility instead of only
   current-frame deltas.
3. Add a short median or Hampel filter to mute impulse jitter.
4. Clip or compress dominant static CSI peaks with percentiles so they do not
   steal the 8x8 dynamic range.
5. Boost only sustained changes over several frames, roughly 300-800 ms, so
   small fast noise is suppressed and larger stable room changes become visible.
6. Split modes into `CSI Shape` and `CSI Motion`.
7. Extend the host LED-frame simulator with captured CSI fixtures and golden
   hashes before changing runtime behavior.

Keep this future visualization path independent from CSI alarm calibration and
`wifi_csi_motion`.

## Validation

Useful targeted checks:

```bash
pio test -e native -f test_csi_band_motion_detector
pio test -e native -f test_csi_motion_calibration_gate
pio test -e native -f test_csi_motion_control_fence
pio test -e native -f test_csi_motion_publication_gate
pio test -e native -f test_csi_rx_callback_fence
pio test -e native -f test_csi_runtime_cleanup_gate
pio test -e native -f test_alarm_service_csi_pipeline
pio test -e native -f test_csi_capture_wire_format
pio test -e native -f test_csi_capture_sequence_window
pio test -e native -f test_csi_capture_replay
pio test -e native -f test_csi_visualization_reducer
pio test -e native -f test_matrix_task
python -m unittest test.test_csi_capture_tools
python scripts/tests/run_csi_fixture_replay.py
python scripts/analyze_csi.py
python scripts/csi_monitor.py
```

For live debugging, start with:

```bash
python scripts/diagnostics/check_runtime_diagnostics.py
python scripts/diagnostics/check_tasks.py --details
```

Then inspect `GET /api/wifisensing/status` for consumer state, queue drops,
packet counters, and motion state.
