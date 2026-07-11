Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Operations](../README.md#operations)

# Runtime Diagnostics

Use this guide when deciding whether a flashed MatrixHub firmware build is
stable enough for release. Authenticated live commands must use HTTPS and JWT.

Default target:

```bash
export DEVICE_URL=https://192.168.0.30
export DEVICE_USER=admin
export DEVICE_PASSWORD=admin
```

The mDNS alias is also valid when local DNS resolves it:

```bash
export DEVICE_URL=https://matrixhub.local
```

Typing `matrixhub.local` or the device IP without `https://` is expected to hit
the firmware's plain-HTTP redirect shim and land on the HTTPS UI.

The device certificate is usually self-signed. Diagnostic scripts disable TLS
verification by default; use `--tls-verify` only with a trusted certificate.

## Diagnostic Endpoints

The primary runtime endpoints are:

```text
GET /api/diagnostics/summary
GET /api/diagnostics/heap
GET /api/diagnostics/tasks?details=1
GET /api/diagnostics/mutexes
GET /api/diagnostics/endpoints
GET /api/diagnostics/features
GET /api/system/info
GET /api/system/tasks?details=1
GET /rest/logs/tail?lines=120
```

Quick validation:

```bash
python scripts/diagnostics/check_runtime_diagnostics.py
python scripts/diagnostics/check_memory.py
python scripts/diagnostics/check_tasks.py --details
```

The same data is captured by the live smoke, stress, and soak suites under
`artifacts/device-smoke/`, `artifacts/stress/`, and `artifacts/soak/`.

## Metrics

Heap metrics:

- `free_heap` or internal `free`: current free internal heap. It can move during
  HTTP/TLS activity, so compare before/after deltas, not a single sample.
- `min_free_heap` or `minimumFree`: lowest observed free internal heap since
  boot. It must not keep dropping during a soak.
- `largestBlock`: largest allocatable block in that region. Falling free memory
  together with a falling largest block means fragmentation risk.
- `fragmentationPercent`: percentage derived from free memory and largest block.
  Sustained growth is more important than one noisy sample.
- `free_psram` and PSRAM `largestBlock`: large JSON, WebSocket, log, and UI
  buffers should leave PSRAM with stable free space during stress.

Task and stack metrics:

- `taskCount`: should stay stable across smoke/stress unless a feature is
  intentionally enabled or disabled.
- `stack.worstHighWaterMark`: lowest known stack watermark. Treat zero,
  negative, or a fast downward trend as a release blocker.
- `stack.detailsAvailable`: when false, use the summary only and avoid claiming
  per-task confidence.

Lock and WebSocket metrics:

- `runtime.standard.timeouts` and `runtime.recursive.timeouts` must not grow
  during smoke/stress/soak. Existing non-zero boot counters are acceptable only
  when they are stable.
- `slowAcquires` are warning counters. They may exist from boot, but should not
  grow under normal endpoint stress.
- `wsQueueDrops` must stay flat. Any increase means WebSocket backpressure lost
  data and needs investigation.
- `wsHeapFallbacks` may be non-zero if a fallback path was used, but it should
  not grow during a clean release run.

Boot metrics:

- `bootCount` must not change unless a restart test intentionally runs.
- `unexpectedRestarts` must not increase during any release gate.
- `currentResetReason` should be recorded in the release notes when a build was
  flashed or intentionally restarted before testing.

## Baseline Snapshot

Before changing feature state or flashing, capture a baseline:

```bash
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
OUT="artifacts/diagnostics/$STAMP/baseline"
mkdir -p "$OUT"
python scripts/diagnostics/check_runtime_diagnostics.py --json > "$OUT/runtime.json"
python scripts/diagnostics/check_memory.py --json > "$OUT/memory.json"
python scripts/diagnostics/check_tasks.py --details --json > "$OUT/tasks.json"
TOKEN="$(curl -sk "$DEVICE_URL/rest/signIn" \
  -H 'Content-Type: application/json' \
  -d '{"username":"admin","password":"admin"}' \
  | python -c 'import json,sys; print(json.load(sys.stdin).get("access_token",""))')"
curl -sk "$DEVICE_URL/rest/logs/tail?lines=120" \
  -H "Authorization: Bearer $TOKEN" \
  > "$OUT/log-tail.json"
```

Do not commit tokens or raw secret-bearing config snapshots.

## Feature Enablement Without Losing Config

Prefer scripts that read current settings, mutate the smallest field, and
restore the original payload.

Safe read-only checks:

```bash
python scripts/diagnostics/check_ble_config.py
python scripts/diagnostics/check_usb_features.py
python scripts/diagnostics/check_macros.py
python scripts/diagnostics/check_udp.py
python scripts/diagnostics/check_heartbeat.py
```

BLE and WiFi sensing toggle test:

```bash
python scripts/tests/feature_toggle.py --feature ble --delay 2
python scripts/tests/feature_toggle.py --feature wifisensing --delay 2
```

USB and macro checks can interact with the host. Keep them read-only unless the
operator explicitly allows HID side effects:

```bash
python scripts/diagnostics/check_usb_features.py
python scripts/diagnostics/check_usb_features.py --exercise-hid
python scripts/diagnostics/check_macros.py
python scripts/diagnostics/check_macros.py --safe-writes
```

The legacy `/ws/csi` visualization stream remains opt-in and is not required by
alarm evaluation. `wifi_csi_motion` is a production alarm source when its rule
and CSI detector are enabled; the alarm consumer keeps CSI alive independently
of UI subscribers. Use the separate admin-only `/ws/csi-capture/v1` workflow
for lossless evidence, and run
`scripts/tests/csi_alarm_hardware_gate.py` for the timestamped CSI-to-alarm
release gate. Do not infer alarm correctness from `/ws/csi` rendering traffic.

Notification delivery endpoints are not part of the shared-device release
checklist. Reading `/api/notifications/settings` is fine; do not call live
Telegram, Webhook, or Pushover delivery tests on a device that shares external
accounts with another ESP.

## Release Pass/Fail

Pass:

- smoke, stress, and soak reports are `PASS`,
- `failed_sample_requests` is zero,
- `unexpectedRestarts` and `bootCount` deltas are zero unless explicitly tested,
- lock timeout deltas are zero,
- `wsQueueDrops` delta is zero,
- `min_free_heap` drift is within the soak threshold,
- internal and PSRAM largest blocks do not show sustained collapse,
- worst stack watermark is stable and non-zero,
- log tail contains no panic, watchdog, brownout, assert, or coredump marker.

Fail:

- any panic/watchdog/brownout/restart during non-restart tests,
- any growing lock timeout counter,
- any WebSocket queue drop under normal stress,
- any smoke contract failure,
- any build or frontend quality gate failure,
- any release run that required untracked local config edits to pass.

Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Operations](../README.md#operations)
