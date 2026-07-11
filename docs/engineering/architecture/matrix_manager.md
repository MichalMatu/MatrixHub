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
persisted brightness configuration nor serves as a general runtime mute. A
runtime zero-brightness transition destroys renderer state and requires the
manager to invalidate and replay the top layer when output becomes visible
again.

Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Architecture](../README.md#runtime-and-architecture)
