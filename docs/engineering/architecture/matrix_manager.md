Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Architecture](../README.md#runtime-and-architecture)

# Matrix Manager Service Reference

## Overview
The `MatrixManagerService` acts as the "Director" for the Matrix display. It decouples the business logic (queues, layers, and content priority) from the "dumb" hardware rendering driver (`MatrixService`).

**Location:** `src/system/matrix_manager/`

## Architecture

The Matrix Manager subsystem is responsible for determining *what* to display and *when*. It manages:
1. **Z-Index Layering:** Deciding which feature has priority to be shown.
2. **Notification Queueing:** A PSRAM-allocated FIFO queue that handles incoming messages so they don't block logic or get instantly overwritten.

### Core Components

#### 1. `MatrixLayerManager`
Manages the Z-index priority using an internal layer stack.
*   **Layers (Highest Priority to Lowest):**
    *   `RESET_MODAL` (Factory-reset warning and confirmation feedback)
    *   `MENU` (Button-driven interactive configuration)
    *   `SYSTEM_MODAL` (Critical, self-clearing system feedback)
    *   `NOTIFICATION` (Popup messages queued by runtime or API events)
    *   `ALARM` (Latched warning state, restored after higher transient layers)
    *   `IDLE` (Animations, Dashboard)
    *   `BACKGROUND` (Bottom priority)
*   **Behavior:** Only the highest-priority active layer is rendered. Thread-safe via mutexes.

#### 2. `MatrixNotificationQueue`
A thread-safe FIFO ring buffer (`MAX_ITEMS = 8`) for managing popup text notifications.
*   **Memory Model:** Fixed-size storage with no per-notification heap allocation. Actual placement depends on the owning object, not on the queue itself.
*   **Behavior:** Pushes new notifications to the back; drops the oldest if overflow occurs.

#### 3. `MatrixManagerService` (The Core Director)
The main service orchestrating the Matrix logic.
*   **API:** Provides endpoints like `setLayer()`, `clearLayer()`, and `queueNotification()`.
*   **Renderer Control:** Commands `MatrixService` via injected dependencies (Constructor Injection).
*   **Task Loop:** During its `update()` phase, it evaluates the layer timeouts, auto-advances the notification queue based on display time, and pushes the top-most active content down to the hardware renderer.

## Usage Guide (For Consumers)

Instead of talking directly to the `MatrixService`, feature modules should utilize the Matrix Manager to ensure global UI behavior constraints are respected.

### Sending a Notification
```cpp
_matrixManager->queueNotification("Hello Wi-Fi", 0x00FF00, 3000 /* ms */);
```

### Displaying a Modal/Layer
```cpp
LayerContent modal;
modal.type = CommandType::TEXT;
strncpy(modal.text, "Booting...", sizeof(modal.text));

_matrixManager->setLayer(Layer::SYSTEM_MODAL, modal);
// ... later ...
_matrixManager->clearLayer(Layer::SYSTEM_MODAL);
```

## Regression Notes

### 1. LED color order on this board

The Waveshare ESP32-S3 Matrix used by this project expects `RGB` byte order in
`lib/matrix_driver/LedMatrix.cpp`, not the more common `GRB`.

- Keep the board default as `NEO_RGB + NEO_KHZ800`.
- If another panel needs a different order, override `MATRIX_NEOPIXEL_TYPE` at
  build time for that board instead of changing the shared driver default.
- Changing the default to `NEO_GRB` on this board causes a visible channel swap
  where red renders as green.

### 2. Disabling idle effects requires two steps

In layered mode, "effects disabled" is not just a `BACKGROUND` layer concern.

1. Clear the `Layer::BACKGROUND` layer in `MatrixManagerService`.
2. Clear the cached background effect in `MatrixService` / `MatrixState`.

Why both matter:

- `MatrixManagerService` owns which layer is currently visible.
- `MatrixState` separately remembers the last persistent background effect.
- A later `clear(false)` path may restore that remembered effect even after the
  background layer was removed, unless the cached effect state is explicitly
  cleared too.

`MatrixRuntimeApplier` is the canonical place that handles both steps when the
settings payload turns `effectEnabled` off.

### 3. Pending visible content is latest-wins

`MatrixManagerService` resolves layers before it calls `MatrixService`, but the
service may still have to apply a brightness or rotation update before the next
visible frame. `MatrixState` therefore keeps hardware settings as independent
coalesced updates and keeps exactly one pending visible-content command.

A newer text, icon, solid, effect, visualization, or clear request replaces any
older visible command that has not reached the renderer yet. Do not restore
separate dirty bits or a fixed content-type priority inside `MatrixState`: that
can render a newer alarm solid first and then replay an older background effect
on the following MatrixTask tick while the layer manager incorrectly believes
the alarm is still on screen.

Background effect and data-visualization caches remain separate because they
describe what should be restored after a temporary higher layer disappears;
they are not a queue of frames waiting to be rendered. Clearing either cache
also cancels a matching persistent command that has not reached the renderer;
temporary commands with a non-zero duration are left intact.

All access to this multi-field mailbox uses a non-expiring task-context mutex.
Callers must not use `MatrixState` from an ISR.

### 4. Renderer ownership and cache invalidation

When `MatrixManagerService` exists, feature-level visual feedback must be
published as a layer. Direct `MatrixService::show*()` calls can replace a
pending command after the manager has already cached that layer as rendered.
The long-hold gesture opens `MENU` before it reaches the reset threshold, so
factory-reset feedback first closes that menu and then uses `RESET_MODAL`.
That dedicated top layer also prevents an in-flight menu refresh from hiding a
critical reset prompt. After its visible timeout the manager republishes the
next layer, including a latched alarm. The armed message has no timeout because
the confirmation window starts on button release; cancellation replaces it
with a timed message.

`invalidateCache()` forces the active top layer to be republished without
forgetting whether the manager currently owns visible output. If invalidation
follows removal of the final layer, the next update clears the renderer and
cannot leave a disabled background effect running.

### 5. Terminal shutdown blackout

`MatrixTask` is the only runtime owner allowed to commit frames to the physical
matrix. When restart or deep sleep stops that task, its epilogue calls
`MatrixService::blackoutForShutdown()` synchronously before the stop ACK and
before suspending. The ACK therefore means that renderer modes were stopped and
the black frame was submitted to the LED driver. The underlying RMT API returns
no transmission status, so this is a software completion guarantee, not an
electrical confirmation; the release gate still requires a physical LED/current
test on the target board.

Task start and stop share one lifecycle ownership gate. A concurrent start is
rejected, while concurrent stop callers wait for the active owner within the
bounded shutdown budget. This prevents either path from reaping the task,
semaphore, stack, or TCB underneath another caller.

Do not enqueue `MatrixState` brightness or clear commands after `MatrixTask`
has stopped: there is no consumer left to apply them. The shutdown blackout is
terminal and intentionally bypasses `MatrixState`, so it neither changes the
persisted brightness configuration nor serves as a general runtime mute.

Runtime brightness zero has a separate reversible contract. It commits black at
the driver, pauses animation writes, preserves renderer mode plus the logical
8x8 RGB frame, and accepts newer content while muted. A 0 -> non-zero transition
stays black until any brightness/rotation/content commands behind it have been
consumed; this prevents a stale pre-mute frame from flashing before the current
top layer. Static content is then restored from the logical frame, while legacy
effects restart from a cleared transport buffer and submit one fresh frame.

Because the terminal blackout deliberately clears renderer state but does not
clear manager layers, `MatrixTask::start()` invalidates the manager render cache.
The first update after a supported stop -> start therefore republishes even an
unchanged top layer. `MatrixService::blackoutForShutdown()` also clears its icon
dedup and timeout caches so an unchanged icon cannot be skipped after restart.

### 6. Brightness and thermal ownership

Persisted/user brightness is constrained to 2..255. Value 0 is reserved for the
runtime thermal mute and is never written back as user configuration.
`MatrixState` publishes `min(userTarget, thermalLimit)` and coalesces changes
against the last value delivered to the renderer. Thermal bands map to limits
255 (normal), 16 (soft), 2 (hard), and 0 (critical).

`LedMatrix` treats the NeoPixel transport buffer as write-only because global
brightness is premultiplied and repeated rescaling is lossy. Static, text,
native, and data-visualization frames are repainted from a separate 64-pixel
logical buffer on every non-zero brightness change. Legacy effects own the
transport buffer directly; their cap transition clears that buffer without an
intermediate latch, restarts the effect, and forces one frame. If the forced
frame cannot be produced, the driver fails closed by latching black.

The power status endpoint exposes `thermal_matrix_limit` for read-only device
evidence. It proves the governor's requested cap, not optical brightness or RMT
delivery; release acceptance still requires current/light measurement on the
target board.

On a ThermalMonitor task restart, the Matrix cap and previous thermal band are
kept until the first fresh sample, including HARD/SOFT hysteresis. This is a
Matrix safety guarantee only; a complete hot-restart audit of CPU frequency,
WiFi power-save cache, and BLE scan/power actuators remains outside G2.3a.

### 7. Legacy effect entry, exit, and flash safety

Legacy WS2812FX output is an owner of the transport buffer, not a second static
frame source. `LedMatrix::pauseEffect()` only releases that ownership; it must
not call vendor `stop()`, because `stop()` immediately transmits a black frame.
The incoming text, icon, solid, Native3D, data-visualization, or terminal
blackout path is responsible for the next and only visible latch.

Entering or changing a legacy effect first clears all 64 transport pixels in
memory without calling `show()`. This makes partial-update modes independent of
the previous icon, alarm, or effect while preserving an atomic visible owner
swap. `MatrixState` prioritizes brightness already queued before content. If a
thermal update arrives after an effect is paused but before the deferred first
scroll/native/data frame, `LedMatrix` rescales and re-latches the held complete
effect frame rather than exposing its intentionally black logical framebuffer.

Effect runtime must be instance- and segment-owned. Heartbeat timing, default
comets, popcorn kernels, and oscillators are reset on mode entry; function-static
mutable state is forbidden because it leaks history across re-entry and fresh
driver instances. `_triggered` and the PRNG seed also have explicit initial
values. Frame deadlines use signed-delta comparison so equality and `millis()`
rollover are both handled.

All-black Fireworks is valid and fail-dark. Color selection scans the three
palette entries in bounded time and never retries while the MatrixTask holds the
WS2812FX mutex. Full-frame Blink/Strobe modes enforce a minimum 500 ms cycle;
Multi Strobe is a bounded double pulse with a minimum 1000 ms cycle.

Localized high-contrast modes use an explicit conservative product policy:

- Flash Sparkle and Hyper Sparkle render no more than three new frames in any
  one-second window (minimum frame interval 334 ms), even when the saved speed
  requests a faster cadence.
- Chase Flash and Chase Flash Random use two 80 ms lit pulses in a cycle of at
  least 1000 ms; the saved speed controls the full pulse/rest cycle instead of
  leaving a hard-coded 30 ms burst.
- The UI marks every legacy blink/strobe/flash selector with a visible warning;
  none of the four localized modes is present in `recommended` or `calm`.

The frequency boundary follows the conservative no-more-than-three-flashes
technique from [WCAG 2.2 SC 2.3.1][wcag-flashes], but this is an engineering
input rather than a physical-panel conformance claim. Viewing distance,
brightness, illuminated area, supply current, and camera exposure still belong
to the locked-camera/current hardware gate. A host trace test runs the real
70-mode WS2812FX engine with a deterministic clock at speeds 50, 500, 1000, and
65535 ms and verifies the four localized modes against the 64-pixel buffer.

[wcag-flashes]: https://www.w3.org/WAI/WCAG22/Understanding/three-flashes-or-below-threshold

Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Architecture](../README.md#runtime-and-architecture)
