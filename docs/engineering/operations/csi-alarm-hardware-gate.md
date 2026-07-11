# CSI to alarm hardware gate

Use `scripts/tests/csi_alarm_hardware_gate.py` after flashing the exact clean
release candidate. The gate is read-only: it does not change Wi-Fi sensing,
calibration, or alarm configuration.

## Preflight contract

The run stops before collecting evidence unless all of these conditions hold:

- `/api/system/info` reports the exact expected 40-character commit and
  `firmware_dirty=false`;
- exactly one enabled `wifi_csi_motion` rule exists, uses `above 0.5`, and
  exposes strict uint32 `transition_seq`/`device_millis` plus a nonzero,
  lowercase 16-hex boot-scoped `boot_id`;
- `/api/wifisensing/config` exposes enabled CSI alarm configuration with exact
  `hold_ms` and `clear_hold_ms` values used for operator-response deadlines;
- the CSI runtime is enabled, fault-free, reconciled, queue-backed, and has
  forced/stable gain;
- the motion detector is enabled and has a fresh, valid decision frame;
- diagnostics summary and global mutex counters are both available, so reboot,
  HTTP WebSocket drop, and lock-timeout continuity can be proven.

Set credentials through `DEVICE_USER`, `DEVICE_PASSWORD`, or `DEVICE_TOKEN` so
they do not appear in the command line. Choose a new output directory for every
run:

```bash
export DEVICE_URL=https://matrixhub.local
export DEVICE_USER=admin
read -rsp "Device password: " DEVICE_PASSWORD
echo
export DEVICE_PASSWORD
EXPECTED_SHA="$(git rev-parse HEAD)"
OUTPUT="artifacts/csi/alarm-gate/g1-soak-$(date -u +%Y%m%dT%H%M%SZ)"
MARKERS="/tmp/matrixhub-csi-alarm-markers.jsonl"

python3 scripts/tests/csi_alarm_hardware_gate.py run \
  --expected-firmware-sha "$EXPECTED_SHA" \
  --duration-seconds 3600 \
  --minimum-duration-seconds 3600 \
  --poll-interval-seconds 1 \
  --sample-timeout-seconds 1 \
  --minimum-operator-cycles 1 \
  --minimum-verified-reconnect-gaps 1 \
  --minimum-motion-during-reconnect 1 \
  --output-dir "$OUTPUT" \
  --marker-file "$MARKERS"
```

## Operator markers

Append markers from a second terminal. The collector never reads stdin. Each
marker records UTC and its exact host-monotonic action time in the writer
process; delayed file draining therefore does not weaken latency evidence.
Start the gate before adding markers because old lines are deliberately
ignored.

```bash
python3 scripts/tests/csi_alarm_hardware_gate.py mark --marker-file "$MARKERS" --kind quiet_start
python3 scripts/tests/csi_alarm_hardware_gate.py mark --marker-file "$MARKERS" --kind motion_start
python3 scripts/tests/csi_alarm_hardware_gate.py mark --marker-file "$MARKERS" --kind motion_stop
python3 scripts/tests/csi_alarm_hardware_gate.py mark --marker-file "$MARKERS" --kind presence_start
python3 scripts/tests/csi_alarm_hardware_gate.py mark --marker-file "$MARKERS" --kind presence_end
python3 scripts/tests/csi_alarm_hardware_gate.py mark --marker-file "$MARKERS" --kind rf_disturbance
python3 scripts/tests/csi_alarm_hardware_gate.py mark \
  --marker-file "$MARKERS" --kind reconnect_start
python3 scripts/tests/csi_alarm_hardware_gate.py mark \
  --marker-file "$MARKERS" --kind reconnect_end
```

At least one cycle must use this exact order:

1. Ensure the observed state is clear, append `quiet_start`, and keep the area
   quiet until the gate observes detector and alarm clear.
2. Append `motion_start`, then move continuously until both detector and alarm
   are active.
3. Append `motion_stop`, stop moving, and remain still until both return clear.

The gate derives deadlines from `hold_ms`/`clear_hold_ms` and adds one poll
interval plus the configured margin. A state already present before
`motion_start` or `motion_stop` is not accepted as a causal response. Presence
and RF markers are contextual only and never act as a motion oracle.

Every REST poll records a host-monotonic interval from immediately before its
first GET through completion of its last GET. Only a sample whose capture
started at or after an action marker can prove the causal `motion_start` or
`motion_stop` response. A capture crossing either causal marker fails as
temporally ambiguous when it contains the target, any state/sequence change,
missing or invalid decision data, or a detector/alarm mismatch; a later stable
target cannot rescue that step. A capture crossing `quiet_start` is simply
ignored for baseline acceptance, and the gate waits for the first complete
post-action clear sample.

The default closure also requires this reconnect edge scenario while the
pre-gap state is clear: append `reconnect_start`, append `motion_start` during
the endpoint gap, keep moving through recovery, then append `reconnect_end`.
The motion marker qualifies only when at least one network-level gap sample
starts at or after its action timestamp and before the first complete recovery
sample. Such a sample must either report exact
`wifi.sta_connected=false` or contain `transport_error` from at least two
distinct core endpoints in the same sample. A single endpoint failure, HTTP
error, invalid JSON/shape, authentication/client error, or other non-transport
failure does not count as reconnect proof. A late `motion_start` after recovery
does not qualify even if `reconnect_end` has not been appended yet.
The post-`motion_start` sample must carry this network proof itself; it cannot
inherit proof from an earlier gap sample. Once a planned gap is open,
non-transport endpoint failures remain hard errors rather than planned-gap
warnings.
The first complete REST snapshot may still show the unchanged pre-gap state
while the configured hold runs, or exactly one same-`boot_id` false→true
transition. The normal motion deadline starts only when a complete, fresh
post-recovery snapshot is available. Any other sequence delta or parity fails.
After detection, append `motion_stop` and remain still to close the operator
cycle. Diagnostics boot count/uptime remain a separate reboot proof; `boot_id`
is the authoritative alarm transition epoch.

## Evidence and verdict

`trace.jsonl` contains full endpoint snapshots with UTC and host-monotonic
start/completion timestamps, including every bounded consistency reread used
to resolve the legal race between sequential Wi-Fi and alarm REST reads.
Cadence and soak duration use sample completion times; reports also expose the
longest capture window. Preflight uses the normal diagnostic client timeout;
the soak uses a separate no-retry polling client with the explicit one-second
default timeout so a short reconnect can produce multiple independent
transport observations without consuming the 60-second gap budget. The trace
may contain SSIDs, IP addresses, MAC addresses, rule names, and other
device-local data, so the gate creates it with mode `0600` inside a mode `0700`
directory. Do not attach or commit it without a separate redaction review.

The detector configuration is snapshotted on every poll and must remain equal
to preflight, so hold, sensitivity, or band changes cannot mix incompatible
evidence in one closure. Runtime `motion.has_frame` and the stable
`motion.state`/`detected` relationship must also remain ready on every poll.

`report.json`, `report.md`, and `closure-summary.json` are deterministic,
allow-listed summaries. Only a run whose closure summary records at least
3600 seconds, one correlated operator cycle, and one verified reconnect can
close G1; the default reconnect must contain the motion-start edge described
above. The closure contains the SHA-256 of the closed private trace without
copying private fields. The gate fails on detector/alarm inversion or false
clear, rule or firmware drift, invalid-state retention failure, runtime fault,
unobservable gaps, queue/capture/HTTP WebSocket drops, growing lock timeout
counters, device restart, or failure to exercise a complete
clear→motion→clear cycle in detector, alarm state, and transition metadata.
The CLI requires at least 3600 seconds of host-monotonic evidence by default;
shorter diagnostic runs must explicitly lower `--minimum-duration-seconds` and
do not qualify as the G1 soak closure.

Default cadence requires at least 80% of one-second samples with no sample gap
over 90 seconds. Invalid/stale detector decisions are reported separately
from endpoint reconnect gaps and fail above 10 seconds continuously or 30
seconds cumulatively. Planned reconnect endpoint gaps have their own finite
budgets and fail above 60 seconds continuously or 120 seconds cumulatively.
`batches_dropped_total` is intentionally not a failure signal because it can
increase when the legacy `/ws/csi` stream has no receiver.
