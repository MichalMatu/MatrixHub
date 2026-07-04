/**
 * @file ButtonHandler.cpp
 * @brief User button handling implementation
 */

#include "ButtonHandler.h"

#include <Arduino.h>
#include <driver/gpio.h>

#include "../../config/App.h"
#include "../../config/Hardware.h"
#include "../logging/Logging.h"
#include "../../config/System.h"

#undef LOG_TAG
#define LOG_TAG "Button"

void ButtonHandler::begin() {
    if (HW::USER_BUTTON == HW::PIN_DISABLED) {
        return;
    }

    if (HW::USER_BUTTON_ACTIVE_LOW) {
        pinMode(HW::USER_BUTTON, INPUT_PULLUP);
    } else {
        pinMode(HW::USER_BUTTON, INPUT);
    }

    // Keep all gesture state local to this instance so the application owns
    // exactly one runtime button engine instead of relying on hidden globals.
    _lastStableState = false;
    _lastReading = false;
    _lastChangeTime = 0;
    _pressStartTime = 0;
    _holdStartTime = 0;
    _holdLastLogTime = 0;
    _longPressTriggered = false;
    _mediumPressTriggered = false;
    _resetWarningTriggered = false;
    _resetArmed = false;
    _resetArmedDeadline = 0;
    _resetArmedClicks = 0;
    _clickCount = 0;
    _lastClickTime = 0;

    LOGI("Button initialized on pin %d (active %s)",
         HW::USER_BUTTON,
         HW::USER_BUTTON_ACTIVE_LOW ? "LOW" : "HIGH");
}

void ButtonHandler::setBindings(Bindings bindings) {
    // Phase 7 can safely rewire button side effects without reconstructing the
    // handler. That keeps gesture state and ownership stable while still
    // allowing services like AirMouse to hook in later during boot.
    _bindings = std::move(bindings);
}

void ButtonHandler::dispatchPendingClicks() {
    if (_clickCount == 0) {
        return;
    }

    if (_bindings.onMultiClick) {
        LOGI("Click gesture: %d click(s)", _clickCount);
        _bindings.onMultiClick(_clickCount);
    }
    _clickCount = 0;
    _lastClickTime = 0;
}

void ButtonHandler::cancelPendingClicks() {
    _clickCount = 0;
    _lastClickTime = 0;
}

void ButtonHandler::resetHoldTracking() {
    _pressStartTime = 0;
    _holdStartTime = 0;
    _holdLastLogTime = 0;
    _longPressTriggered = false;
    _mediumPressTriggered = false;
    _resetWarningTriggered = false;
}

void ButtonHandler::update() {
    if (HW::USER_BUTTON == HW::PIN_DISABLED) {
        return;
    }

    const uint32_t now = millis();

    // Expire the factory-reset confirmation window first. A timeout here
    // produces a cancellation hook so the matrix can clear its warning even
    // when the user does nothing after a long hold.
    if (_resetArmed && static_cast<int32_t>(now - _resetArmedDeadline) >= 0) {
        LOGI("Factory reset confirmation window expired (no double-click)");
        _resetArmed = false;
        _resetArmedClicks = 0;
        if (_bindings.onResetCancelled) {
            _bindings.onResetCancelled();
        }
    }

    // The click window must expire even while the button is idle, so dispatch
    // pending AirMouse clicks before any early returns below. Skip dispatch
    // while we're waiting for a factory-reset confirmation — those presses
    // belong to the confirmation gesture, not to AirMouse.
    if (!_resetArmed && _clickCount > 0 && (now - _lastClickTime > _doubleClickWindowMs)) {
        dispatchPendingClicks();
    }

    const int level = gpio_get_level(static_cast<gpio_num_t>(HW::USER_BUTTON));
    const bool currentReading = HW::USER_BUTTON_ACTIVE_LOW ? (level == LOW) : (level == HIGH);

    if (currentReading != _lastReading) {
        _lastChangeTime = now;
        _lastReading = currentReading;
        LOGD("Pin bouncing: new reading=%d", currentReading);
    }

    if (now - _lastChangeTime < FACTORY::SHORT_PRESS_DEBOUNCE_MS) {
        return;
    }

    const bool pressed = currentReading;
    const bool wasPressed = _lastStableState;

    // ── While button is held ────────────────────────────────────────────────
    // We now fire all "long-enough" gestures immediately when their threshold
    // is crossed, so the user gets feedback without releasing. The release
    // edge below then suppresses duplicate menu actions via _mediumPressTriggered.
    if (pressed && _holdStartTime > 0) {
        const uint32_t heldMs = now - _holdStartTime;

        if (FACTORY::HOLD_LOG_INTERVAL_MS > 0 &&
            now - _holdLastLogTime >= FACTORY::HOLD_LOG_INTERVAL_MS) {
            _holdLastLogTime = now;
            LOGI("Holding BOOT: %lu/%lu ms",
                 static_cast<unsigned long>(heldMs),
                 static_cast<unsigned long>(FACTORY::LONG_PRESS_MS));
        }

        // 2s threshold -> open the matrix menu or select the active menu item.
        // The menu owns EXIT as an explicit item, so active-menu holds are no
        // longer hidden close gestures.
        if (!_mediumPressTriggered && heldMs >= FACTORY::MEDIUM_PRESS_MS) {
            _mediumPressTriggered = true;
            cancelPendingClicks();
            const bool menuActive = _bindings.isMenuActive ? _bindings.isMenuActive() : false;
            const bool menuEnabled = _bindings.isMenuEnabled ? _bindings.isMenuEnabled() : false;
            if (menuActive) {
                if (_bindings.onMenuSelect) {
                    _bindings.onMenuSelect();
                }
                LOGI("Medium hold (%lu ms) - Select Menu Item (while held)",
                     static_cast<unsigned long>(heldMs));
            } else if (menuEnabled) {
                if (_bindings.onMenuEnter) {
                    _bindings.onMenuEnter();
                }
                LOGI("Medium hold (%lu ms) - Enter Menu (while held)",
                     static_cast<unsigned long>(heldMs));
            }
        }

        // 7s threshold → fire a visual warning so a user holding by accident
        // (or with a physically stuck button) is told that a reset is brewing.
        if (!_resetWarningTriggered && heldMs >= FACTORY::RESET_WARNING_MS) {
            _resetWarningTriggered = true;
            LOGW("Factory reset warning at %lu ms - release now to cancel",
                 static_cast<unsigned long>(heldMs));
            if (_bindings.onResetWarning) {
                _bindings.onResetWarning();
            }
        }

        // 10s threshold → ARM, do not execute. Confirmation requires release
        // followed by a double-click inside the window.
        if (!_longPressTriggered && heldMs >= FACTORY::LONG_PRESS_MS) {
            _longPressTriggered = true;
            cancelPendingClicks();
            LOGW("Factory reset ARMED at %lu ms - awaiting confirmation",
                 static_cast<unsigned long>(heldMs));
            if (_bindings.onResetArmed) {
                _bindings.onResetArmed();
            }
        }
    }

    if (pressed == wasPressed) {
        return;
    }

    _lastStableState = pressed;

    // ── PRESS edge ─────────────────────────────────────────────────────────
    if (pressed && !wasPressed) {
        LOGI("PRESSED (edge detected)");
        _pressStartTime = now;
        _holdStartTime = now;
        _holdLastLogTime = now;
        _longPressTriggered = false;
        _mediumPressTriggered = false;
        _resetWarningTriggered = false;
        if (_bindings.onActivity) {
            _bindings.onActivity();
        }
        return;
    }

    // ── RELEASE edge ───────────────────────────────────────────────────────
    if (!pressed && wasPressed) {
        LOGI("RELEASED (edge detected)");
        if (_pressStartTime == 0) {
            resetHoldTracking();
            return;
        }

        const uint32_t pressDuration = now - _pressStartTime;

        // Release after a 10s+ hold → enter the confirmation window. The
        // matrix shows "release + 2× click"; we count two presses inside
        // RESET_CONFIRM_WINDOW_MS to actually wipe config.
        if (_longPressTriggered) {
            _resetArmed = true;
            _resetArmedDeadline = now + FACTORY::RESET_CONFIRM_WINDOW_MS;
            _resetArmedClicks = 0;
            LOGW("Long press released - awaiting double-click confirmation "
                 "within %lu ms", static_cast<unsigned long>(FACTORY::RESET_CONFIRM_WINDOW_MS));
            resetHoldTracking();
            return;
        }

        // We're currently in the confirmation window. Each release counts as a
        // confirmation click; two clicks fire the reset.
        if (_resetArmed) {
            _resetArmedClicks++;
            LOGI("Reset confirmation click %u/2", _resetArmedClicks);
            if (_resetArmedClicks >= 2) {
                _resetArmed = false;
                _resetArmedClicks = 0;
                LOGW("Factory reset CONFIRMED via double-click");
                if (_bindings.onFactoryReset) {
                    _bindings.onFactoryReset();
                }
            }
            resetHoldTracking();
            return;
        }

        const bool menuActive = _bindings.isMenuActive ? _bindings.isMenuActive() : false;

        // The medium-press menu action already fired while held; release is a
        // no-op except to clear state.
        if (_mediumPressTriggered) {
            resetHoldTracking();
            return;
        }

        // Short press: menu-next when menu is open, else accumulate for
        // AirMouse multi-click handling.
        if (pressDuration < FACTORY::SHORT_PRESS_MAX_MS) {
            if (menuActive) {
                if (_bindings.onMenuNext) {
                    _bindings.onMenuNext();
                }
                LOGI("Short press (%lu ms) - Next Screen (Menu)",
                     static_cast<unsigned long>(pressDuration));
            } else {
                _clickCount++;
                if (_clickCount > 3) {
                    _clickCount = 3;
                }
                _lastClickTime = now;
                LOGD("Click accumulated on release: count=%d", _clickCount);
            }
        }

        resetHoldTracking();
        return;
    }
}

bool ButtonHandler::isPressed() const {
    return _lastStableState;
}

uint32_t ButtonHandler::pressDurationMs() const {
    if (!_lastStableState || _pressStartTime == 0) {
        return 0;
    }
    return millis() - _pressStartTime;
}

void ButtonHandler::setDoubleClickWindowMs(uint32_t ms) {
    const uint32_t clamped = constrain(
        ms,
        static_cast<uint32_t>(LIMITS::AIR_MOUSE::MIN_DOUBLE_CLICK_MS),
        static_cast<uint32_t>(LIMITS::AIR_MOUSE::MAX_DOUBLE_CLICK_MS));

    if (clamped != ms) {
        LOGW("Double-click window clamped: %lu -> %lu ms",
             static_cast<unsigned long>(ms),
             static_cast<unsigned long>(clamped));
    }
    _doubleClickWindowMs = clamped;
}
