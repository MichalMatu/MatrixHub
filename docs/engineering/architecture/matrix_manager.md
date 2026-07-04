Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Architecture](../README.md#runtime-and-architecture)

# Matrix Manager Service Reference

## Overview
The `MatrixManagerService` acts as the "Director" for the Matrix display. It decouples the business logic (queues, layers, and content priority) from the "dumb" hardware rendering driver (`MatrixService`).

**Location:** `src/system/matrix_manager/`

## Architecture

The Matrix Manager subsystem is responsible for determining *what* to display and *when*. It manages:
1. **Z-Index Layering:** Deciding which feature has priority to be shown (e.g., Alarms override Menus, Menus override Notifications).
2. **Notification Queueing:** A PSRAM-allocated FIFO queue that handles incoming messages so they don't block logic or get instantly overwritten.

### Core Components

#### 1. `MatrixLayerManager`
Manages the Z-index priority using an internal layer stack.
*   **Layers (Highest Priority to Lowest):**
    *   `ALARM` (Top priority - overrides everything)
    *   `SYSTEM_MODAL` (Critical system feedback such as booting or recovery notices)
    *   `NOTIFICATION` (Popup messages queued by runtime or API events)
    *   `MENU` (User interactive configuration menu)
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

Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Architecture](../README.md#runtime-and-architecture)
