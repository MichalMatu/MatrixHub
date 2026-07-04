#pragma once

#include <functional>
#include <atomic>
#include "../../config/System.h"
#include "../../system/rtc/types/RtcAirMouseTypes.h"
#include "../interfaces/IAirMouseInterfaces.h"

namespace AIRMOUSE {

/**
 * @brief Controller for Mouse Jiggler logic
 * Decoupled from hardware for easier testing
 */
class JigglerController {
public:
    JigglerController(IMouseMover& mouse, ITimeProvider& time, IKeyboardSender* keyboard = nullptr);

    // Main entry point called from the AirMouse task loop.
    void update(RTC::JigglerData config);
    
    // State accessors for testing
    bool isWaitingForReturn() const { return _isStealthReturning; }
    uint32_t getCurrentInterval() const { return _currentInterval; }
    uint32_t getLastTriggerTime() const { return _lastTriggerTime; }

    /**
     * @brief Signal that user is actively moving the device.
     * Pauses the Jiggler for PAUSE_DURATION_MS
     */
    void notifyUserActivity();
    
    /**
     * @brief Check if Jiggler is currently paused due to user activity
     */
    bool isPaused() const;

private:
    IMouseMover& _mouse;
    ITimeProvider& _time;
    IKeyboardSender* _keyboard = nullptr;

    struct BezierState {
        bool active = false;
        float t = 0;       // 0.0 to 1.0
        float tStep = 0;   // Step per update
        
        // Control points
        int16_t startX, startY;
        int16_t endX, endY;
        int16_t control1X, control1Y;
        int16_t control2X, control2Y;
        
        // Accumulator for fractional movement
        float accX = 0;
        float accY = 0;
    } _bezier;

    // State
    uint32_t _stealthMoveStartTime = 0;
    bool _isStealthReturning = false;
    uint32_t _lastTriggerTime = 0;
    uint32_t _currentInterval = 0;
    std::atomic<uint32_t> _lastUserActivityTime{0};

    // Time to wait after last user movement before resuming (in ms)
    static constexpr uint32_t PAUSE_DURATION_MS = CONFIG::AIR_MOUSE::JIGGLER::PAUSE_AFTER_ACTIVITY_MS;

    
    // Previous config to detect changes
    RTC::JigglerData _lastConfig;
    
    void triggerMove(const RTC::JigglerData& config);
    void updateInterval(const RTC::JigglerData& config);

    // Leftover movement is queued to avoid large HID deltas.
    int32_t _leftoverX = 0;
    int32_t _leftoverY = 0;

    void queueMovement(int16_t x, int16_t y);
    void updateBezier(const RTC::JigglerData& config);
    void triggerKeyboard();
};

} // namespace AIRMOUSE
