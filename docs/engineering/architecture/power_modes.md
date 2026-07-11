Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Architecture](../README.md#runtime-and-architecture)

# Power Modes and Sleep Flow

This document describes the current power subsystem in the repo.

## Current model

Power behavior is driven by `POWER::PowerManager` and consists of:

- inactivity-based auto-sleep
- manual sleep requests
- maintenance sleeps such as hygiene sleep
- wake-source handling
- RTC-backed activity restoration after wake

The main code paths are:

- `src/system/power/PowerManager.*`
- `src/system/power/PowerActivityTracker.*`
- `src/system/power/PowerSleepController.*`
- `src/system/power/PowerWakeController.*`
- `src/system/shutdown/ShutdownSequence.cpp`

## Persistent settings

### Runtime source of truth

Power settings live in RTC config:

- `RTC::ConfigStore::power`

JSON-facing keys are:

- `sleep_enabled`
- `inactivity_timeout_ms`
- `grace_after_boot_ms`
- `wake_timer_enabled`
- `wake_button_enabled`
- `wake_touch_enabled`
- `wake_interval_ms`
- `timer_wake_awake_ms`
- `button_wake_awake_ms`
- `wake_touch_gpio`
- `wake_touch_threshold`

Those are loaded/saved through `src/config/json/PowerSettingsJson.cpp`.
Deep sleep remains disabled by default through `sleep_enabled = false`. Wake
sources can still be configured ahead of time so they are ready once sleep is
enabled.

When `sleep_enabled` is true, at least one wake source must be enabled. The
configuration API rejects sleep-enabled payloads with all of
`wake_timer_enabled`, `wake_button_enabled`, and `wake_touch_enabled` set to
false. Persisted configs from older firmware are sanitized on load by enabling
timer wake rather than accepting an unrecoverable sleep configuration.

### NVS backup

`PowerSettings` still keeps a Preferences backup in namespace `power_cfg`:

- `inact_ms`
- `grace_ms`
- `sleep_en`
- `wake_tmr`
- `wake_btn`
- `wake_tch`
- `wake_ms`
- `tmr_awake`
- `btn_awake`
- `tch_gpio`
- `tch_thr`

These are implementation details for backup compatibility. New docs and frontend code should use the JSON keys above.

## HTTP endpoints

### `GET /rest/power/status`

Authenticated endpoint returning current power state, including:

- `wake_reason`
- `wake_cause_raw`
- `wake_gpio_mask`
- `wake_ext1_mask`
- `sleep_requested`
- `sleep_eta_ms`
- `sleep_enabled`
- `inactivity_timeout_ms`
- `grace_after_boot_ms`
- `wake_timer_enabled`
- `wake_button_enabled`
- `wake_touch_enabled`
- `wake_interval_ms`
- `timer_wake_awake_ms`
- `button_wake_awake_ms`
- `wake_touch_gpio`
- `wake_touch_threshold`
- `wake_awake_window_ms`
- `wake_awake_eta_ms`
- `last_activity_ms`
- `thermal_state`
- `thermal_temp_c`
- `thermal_cpu_mhz`
- `thermal_throttled`
- `thermal_matrix_limit`
- `thermal_soft_c`
- `thermal_hard_c`
- `thermal_critical_c`
- `uptime_ms`

This endpoint is intentionally read-only from the inactivity tracker point of
view: polling power status for diagnostics must not refresh `last_activity_ms`.
An open UI still prevents accidental sleep through the `/ws/system` activity
path, which represents an active client session rather than passive status
scraping.

### `GET/POST /rest/power/config`

Authenticated reads and admin writes for:

- `sleep_enabled`
- `inactivity_timeout_ms`
- `grace_after_boot_ms`
- `wake_timer_enabled`
- `wake_button_enabled`
- `wake_touch_enabled`
- `wake_interval_ms`
- `timer_wake_awake_ms`
- `button_wake_awake_ms`
- `wake_touch_gpio`
- `wake_touch_threshold`

Changes are applied immediately through `PowerManager` and persisted to RTC plus NVS backup.

Validation rule: if `sleep_enabled` is true and no wake source is enabled, the
endpoint returns `400 input/power_wake_source_required`.

### `POST /rest/power/hygieneSleep`

Admin endpoint that triggers a short maintenance sleep used for heap hygiene.
The implementation applies a one-shot timer wake override before requesting
sleep. It does not rewrite the persistent wake-source settings selected by the
user.

### `POST /rest/sleep`

Admin endpoint that requests normal deep sleep through `PowerManager` instead of relying on the framework default handler.

## Activity tracking

`PowerActivityTracker` is responsible for deciding when inactivity sleep should happen.

Current rules:

- last activity and boot timestamps are restored from RTC when possible
- AP clients count as activity, so the device should not sleep while someone is connected to SoftAP
- `/ws/system` and other active control/data channels count as activity
- passive diagnostic reads such as `GET /rest/power/status` do not count as activity
- the grace period is respected after boot
- inactivity sleep only happens if `sleep_enabled` is true
- timer, BOOT, and touch wakes can use a short post-wake awake window before normal inactivity rules apply
- timer wakes use `timer_wake_awake_ms`
- BOOT and touch wakes use `button_wake_awake_ms`, giving an administrator time to connect
- any real client or control activity cancels the short post-wake window and returns to normal inactivity timing

If inactivity exceeds the configured timeout, the tracker requests:

- `requestSleep("inactivity")`

If a device wakes only to perform scheduled checks and no client activity
appears during the post-wake window, the tracker requests:

- `requestSleep("timer-wake-window")`
- `requestSleep("button-wake-window")`
- `requestSleep("touch-wake-window")`

## Sleep path

Once sleep is requested:

1. `PowerSleepController` waits for the requested delay, if any
2. a sleep-entry failsafe task is armed
3. `SleepService::executeSleepCallbacks()` is called
4. the pre-sleep hook runs `ShutdownSequence::execute()`
5. wake sources are configured by `PowerWakeController`
6. `RTC::prepareForSleep()` updates RTC CRCs and snapshots
7. the firmware enters deep sleep with `esp_deep_sleep_start()`

The failsafe is intentionally conservative. If callbacks or shutdown cleanup
hang long enough to miss the sleep-entry deadline, it prepares wake sources and
RTC state, then forces `esp_deep_sleep_start()`. That protects unattended nodes
from staying awake forever, while serial logs still point at the shutdown work
that failed to finish cleanly.

`PowerWakeController` also has a last-resort guard: if no requested wake source
can be armed, it attempts to arm the minimum timer wake before deep sleep. If
that fallback cannot be armed either, deep sleep is cancelled to avoid a device
that can only recover by reset or power-cycle.

## Wake interval

Normal timer wake interval comes from persistent `wake_interval_ms`, clamped by
the firmware limits in `POWER::WAKE_INTERVAL_MIN_MS` and
`POWER::WAKE_INTERVAL_MAX_MS`.

Specific paths can override it for a single sleep request, for example:

- hygiene sleep uses `100ms`
- thermal emergency cooling can set a longer interval before requesting sleep

The one-shot override is runtime-only and does not mutate the user's stored
timer wake setting.

## Wake sources

Supported deep-sleep wake sources are:

- timer wake, controlled by `wake_timer_enabled` and `wake_interval_ms`
- BOOT button wake, controlled by `wake_button_enabled`, using GPIO0 as EXT1 active-low
- touch wake, controlled by `wake_touch_enabled`, `wake_touch_gpio`, and `wake_touch_threshold`

Touch wake is restricted to the ESP32-S3 touch-capable GPIO range used by this
firmware. The default is GPIO4, with touch wake disabled until the user enables
it explicitly.

All wake sources may be disabled while `sleep_enabled` is false. Once
auto-sleep is enabled, the firmware and UI require at least one of timer, BOOT,
or touch wake so the device has a normal path back to runtime.

## Frontend implications

Frontend code should treat `GET /rest/power/status` as the authoritative status endpoint and use:

- `sleep_enabled`
- `inactivity_timeout_ms`
- `grace_after_boot_ms`
- `wake_timer_enabled`
- `wake_button_enabled`
- `wake_touch_enabled`
- `wake_interval_ms`
- `timer_wake_awake_ms`
- `button_wake_awake_ms`
- `wake_touch_gpio`
- `wake_touch_threshold`
- `wake_reason`
- `sleep_eta_ms`
- `wake_awake_eta_ms`

Do not build new UI against backup-only names such as `grace_ms`.

Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Architecture](../README.md#runtime-and-architecture)
