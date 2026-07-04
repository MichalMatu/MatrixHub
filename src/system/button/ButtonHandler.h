/**
 * @file ButtonHandler.h
 * @brief Runtime button gesture engine for the physical BOOT/USER button.
 *
 * This class intentionally owns only input-side state:
 * - GPIO reading and debounce
 * - press duration tracking
 * - short / medium / long / multi-click gesture detection
 *
 * Product actions are injected through callbacks so the button logic can stay
 * under explicit Application ownership instead of reaching into menu/power/reset
 * services directly.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <utility>

class ButtonHandler {
public:
    struct Bindings {
        // Activity fires on the physical press edge and mirrors the previous
        // PowerManager::notifyActivity("button") behavior without coupling this
        // input module to power/runtime services directly.
        std::function<void()> onActivity;
        std::function<bool()> isMenuActive;
        std::function<bool()> isMenuEnabled;
        std::function<void()> onMenuNext;
        std::function<void()> onMenuEnter;
        std::function<void()> onMenuSelect;
        // Fires while the button is still held and the user is approaching the
        // factory-reset threshold. Use it to flash a heads-up on the matrix so
        // a user holding by accident knows what's about to happen.
        std::function<void()> onResetWarning;
        // Fires when the user crosses the 10s hold mark. Reset is now ARMED
        // but not executed — onFactoryReset only fires after the confirmation
        // double-click within RESET_CONFIRM_WINDOW_MS of release.
        std::function<void()> onResetArmed;
        // Fires if the armed state expired without a confirmation double-click,
        // or was canceled by other input. Use to clear matrix warning state.
        std::function<void()> onResetCancelled;
        std::function<void()> onFactoryReset;
        std::function<void(uint8_t clickCount)> onMultiClick;
    };

    ButtonHandler() = default;
    ButtonHandler(const ButtonHandler&) = delete;
    ButtonHandler& operator=(const ButtonHandler&) = delete;

    /**
     * @brief Initialize button GPIO and internal state.
     *
     * GPIO setup stays separate from callback wiring so Application can own one
     * ButtonHandler instance and Phase 7 can bind runtime actions afterwards.
     */
    void begin();

    /**
     * @brief Install runtime callbacks for button side effects.
     *
     * This keeps ButtonHandler as an input component. Menu navigation, power
     * activity logging, AirMouse multi-click actions, and factory reset are all
     * external actions supplied by the system layer.
     */
    void setBindings(Bindings bindings);

    /**
     * @brief Process button state - call from main loop
     */
    void update();

    /**
     * @brief Check if button is currently pressed (debounced)
     */
    bool isPressed() const;

    /**
     * @brief Get duration of current press in milliseconds
     */
    uint32_t pressDurationMs() const;

    /**
     * @brief Update the double-click window timeout (from AirMouse config)
     */
    void setDoubleClickWindowMs(uint32_t ms);

private:
    // Helpers kept private so state-machine side effects stay in one file.
    void dispatchPendingClicks();
    void cancelPendingClicks();
    void resetHoldTracking();

    Bindings _bindings{};

    // Debounce and hold tracking now live entirely on the instance. If a
    // future bug depends on power cycles or warm restarts, inspect these fields
    // first instead of hunting for hidden static state.
    bool _lastStableState = false;
    bool _lastReading = false;
    uint32_t _lastChangeTime = 0;
    uint32_t _pressStartTime = 0;
    uint32_t _holdStartTime = 0;
    uint32_t _holdLastLogTime = 0;
    bool _longPressTriggered = false;
    // Tracks whether the menu enter/select was already fired during this hold
    // (i.e. the user crossed the 2s mark while still pressing). Prevents a
    // duplicate menu action when the button is finally released.
    bool _mediumPressTriggered = false;
    // Tracks whether the 7s warning hook already fired during this hold.
    bool _resetWarningTriggered = false;

    // Factory-reset confirmation window. Set when a 10s+ hold is released; we
    // then count two quick clicks to confirm. Until confirmation or timeout,
    // multi-clicks are routed to confirmation logic and *not* to AirMouse.
    bool _resetArmed = false;
    uint32_t _resetArmedDeadline = 0;
    uint8_t _resetArmedClicks = 0;

    // Multi-click tracking
    uint8_t _clickCount = 0;
    uint32_t _lastClickTime = 0;
    uint32_t _doubleClickWindowMs = 400; // Default, overridden by AirMouse config
};
