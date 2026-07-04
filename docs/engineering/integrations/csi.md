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

- The Wi-Fi CSI callback copies data only. It must not allocate, block, or run
  detector logic.
- `CsiDataQueue` stores packet data in PSRAM; queue metadata stays in internal
  RAM for FreeRTOS bookkeeping.
- `CsiService::processingTask()` owns packet processing. Its stack and task
  control block stay in internal RAM; the transient WebSocket batch buffer is
  allocated in PSRAM.
- CSI is enabled only while at least one consumer is active.

Current CSI consumers are defined by `WIFISENSING::CSI::CsiConsumer`:

| Consumer | Purpose |
| --- | --- |
| `Frontend` | Keeps CSI active while `/ws/csi` has clients. |
| `AlarmSystem` | Keeps CSI active for `wifi_csi_motion` without the UI open. |
| `Boot` | Reserved boot/runtime consumer. |
| `MatrixVisualization` | Keeps CSI active when matrix data visualization uses Wi-Fi CSI. |

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
- CSI motion detector state, score, confidence, selected carrier count, and
  reset reason

The calibration endpoint applies to the alarm detector. Matrix visualization is
display-only and should not require calibration or alarm baseline readiness.

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
- non-blocking calibration request path

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
pio test -e native -f test_csi_visualization_reducer
pio test -e native -f test_matrix_task
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
