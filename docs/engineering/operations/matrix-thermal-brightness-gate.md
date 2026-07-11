Navigation: [Project README](../../../README.md) · [Commercial hardening audit](../COMMERCIAL_HARDENING_AUDIT.md) · [Matrix manager](../architecture/matrix_manager.md)

# Matrix Thermal And Brightness Hardware Gate

Use this gate to close G2.3a on the Waveshare ESP32-S3 Matrix. Native tests prove
state/order contracts; this procedure proves that the exact release binary
changes physical light output without stale frames, inverted recovery, or an
unsafe current increase.

## Safety and evidence prerequisites

- Build, flash, and record one clean commit SHA. Verify the device reports that
  same SHA with `firmware_dirty=false` before collecting evidence.
- Use a current-limited supply on a non-flammable surface. Prefer an inline
  current meter or photodiode; a camera is acceptable only with exposure, gain,
  focus, white balance, and framing locked.
- Capture serial logs and poll `/rest/power/status`. Record
  `thermal_state`, `thermal_temp_c`, and `thermal_matrix_limit` with timestamps.
- Back up `/api/matrix/settings` and restore it in `finally`, even after a failed
  test. Do not use white at brightness 255 for this gate.

## Reference frames

1. Select a static red frame (`0xFF0000`) with user brightness 32. A single RGB
   channel makes current and channel-order failures easier to distinguish.
2. At a stable normal temperature, record light/current references for the same
   frame at user brightness 32, 16, and 2. Briefly verify user brightness 0 only
   in a dedicated diagnostic build if one exists; production config clamps it
   to 2 because zero is reserved for the thermal runtime mute.
3. Restore user brightness 32. Confirm the frame hash/content and red channel
   are identical to the initial reference.

## Thermal cycle

Use controlled ambient heating or a chamber. Do not heat a component directly
with an uncontrolled flame or hot-air nozzle.

1. Cross the soft threshold (60 C). After the state transition,
   `thermal_matrix_limit` must be 16 and the unchanged static frame must reach
   the brightness-16 reference without a content edit.
2. Cross the hard threshold (68 C), remaining below the critical threshold.
   The limit must be 2 and the unchanged frame must reach the brightness-2
   reference.
3. Cool below 63 C. The state must return to soft and the limit to 16; equality
   at 63 C must remain hard because hysteresis uses a strict lower boundary.
4. Cool below 55 C. The state must return to normal and the limit to 255, so the
   effective output returns to the user target 32. Equality at 55 C must remain
   soft.
5. Repeat the sequence `32 -> 16 -> 2 -> 16 -> 32` at least three times. No
   cycle may stick, invert, flash an older layer, or require a reboot/content
   change to recover.

The Matrix task uses a 31 ms active cadence while brightness/rotation/content
commands are pending, but it can already be inside one 200 ms idle wait when a
new command arrives. Allow at most 350 ms from an observed cap transition to a
settled static frame, including a full brightness -> rotation -> content drain;
temperature sampling itself has a separate interval of up to five seconds.

## Content coverage

Repeat a reduced `16 -> 2 -> 16` cycle for:

- static solid and a static custom icon,
- scrolling text,
- one Native3D effect,
- one data visualization with a fixed input fixture,
- representative full-frame and partial-update legacy effects.

For every case, check for stale-frame flashes, random residual pixels, black
intermediate latches, freezes, channel swaps, and recovery jumps. Record at
least 60 seconds of video/current trace around each transition.

## Critical zero limitation

Production maps zero only at the critical threshold (80 C) and immediately
requests cooling sleep. Ordinary user-brightness or native tests do not prove
that physical thermal-zero ordering end to end.

The normal release gate may combine:

- native proof that critical applies limit 0 before the sleep request,
- driver proof that limit 0 submits 64 black pixels,
- hardware proof of the shared runtime mute path at a safe diagnostic setting.

A full production-SHA critical test requires a controlled thermal chamber,
automatic heater cutoff, current trace, serial capture, and timer wake. Treat it
as a laboratory test; a temporary override endpoint changes the binary and is
not equivalent release evidence.

## Acceptance record

Store the following with the release candidate:

- commit SHA, firmware SHA-256, board ID, supply/current-limit details,
- settings backup and restored-settings confirmation,
- timestamped status/log capture and current/light/video artifacts,
- three-cycle result table for thresholds, limits, settle time, and recovery,
- explicit PASS/FAIL for every content type,
- explicit statement whether physical critical-zero was tested or remains an
  acknowledged laboratory-only gate.
