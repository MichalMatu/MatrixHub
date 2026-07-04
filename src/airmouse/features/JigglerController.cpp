#include "JigglerController.h"

namespace AIRMOUSE {

JigglerController::JigglerController(IMouseMover& mouse, ITimeProvider& time, IKeyboardSender* keyboard)
    : _mouse(mouse), _time(time), _keyboard(keyboard) {
}

void JigglerController::update(RTC::JigglerData config) {
    // Mode OFF: reset state and exit fast.
    if (config.mode == RTC::MouseJigglerMode::JIGGLER_OFF) {
        _isStealthReturning = false;
        _stealthMoveStartTime = 0;
        _leftoverX = 0;
        _leftoverY = 0;
        _bezier.active = false;
        _lastConfig = config;
        return;
    }

    // Smart Jiggler Pause: suspend while user activity is detected.
    if (isPaused()) {
        // If we were waiting to return, cancel it so we don't jerk back when resuming
        _isStealthReturning = false;
        _stealthMoveStartTime = 0;
        _leftoverX = 0;
        _leftoverY = 0;
        _bezier.active = false;
        return; 
    }

    // Process leftovers first (async, non-blocking movement chunking).
    if (_leftoverX != 0 || _leftoverY != 0) {
        const int32_t maxStep = CONFIG::AIR_MOUSE::JIGGLER::MAX_MOUSE_STEP;
        int8_t sx = (_leftoverX > maxStep)
            ? static_cast<int8_t>(maxStep)
            : ((_leftoverX < -maxStep) ? static_cast<int8_t>(-maxStep) : static_cast<int8_t>(_leftoverX));
        int8_t sy = (_leftoverY > maxStep)
            ? static_cast<int8_t>(maxStep)
            : ((_leftoverY < -maxStep) ? static_cast<int8_t>(-maxStep) : static_cast<int8_t>(_leftoverY));
        
        _mouse.move(sx, sy);
        
        _leftoverX -= sx;
        _leftoverY -= sy;
        return; // Process one chunk per loop to avoid blocking IMU reading.
    }

    // Reset loop if config changed significantly (e.g. interval).
    if (config.interval != _lastConfig.interval || 
        config.randomInterval != _lastConfig.randomInterval) {
        // If enabling random, set immediate new interval
        if (config.randomInterval) {
           updateInterval(config);
        } else {
           _currentInterval = config.interval;
        }
    }
    _lastConfig = config;

    // Initialize current interval if 0.
    if (_currentInterval == 0) {
        _currentInterval = config.interval;
    }

    uint32_t now = _time.getMillis();

    // PHASE 2: Return movement (Stealth only).
    if (_isStealthReturning) {
        if (now - _stealthMoveStartTime >= 100) {
            if (config.mode == RTC::MouseJigglerMode::JIGGLER_STEALTH) {
                // Move back by the same distance
                queueMovement(-(int16_t)config.moveDistance, 0);
            }
            _isStealthReturning = false;
        }
        return;
    }
    
    // HUMAN MODE: Continuous Bezier motion.
    if (config.mode == RTC::MouseJigglerMode::JIGGLER_HUMAN) {
        updateBezier(config);
        return;
    }

    // PHASE 1: Initial movement (STEALTH/ACTIVE/KEYBOARD).
    // Check if enough time passed
    // Handle overflow wrap-around if needed (millis wraps in ~50 days), simple subtraction usually safe for <50d intervals
    if (now - _lastTriggerTime > (_currentInterval * 1000)) {
        if (config.mode == RTC::MouseJigglerMode::JIGGLER_KEYBOARD) {
            triggerKeyboard();
        } else {
            triggerMove(config);
        }
        _lastTriggerTime = now;
        updateInterval(config);
    }
}

void JigglerController::triggerMove(const RTC::JigglerData& config) {
    int16_t dist = (int16_t)config.moveDistance;
    
    // Safety clamp (min 1)
    if (dist < CONFIG::AIR_MOUSE::JIGGLER::MIN_MOVE_DISTANCE) {
        dist = CONFIG::AIR_MOUSE::JIGGLER::MIN_MOVE_DISTANCE;
    }

    if (config.mode == RTC::MouseJigglerMode::JIGGLER_STEALTH) {
        // Stealth: move and return after a short delay.
        queueMovement(dist, 0); 
        _stealthMoveStartTime = _time.getMillis();
        _isStealthReturning = true;
    } else {
        // ACTIVE: random move inside a radius.
        // Use dist as a radius/range
        // Use full int range for random then clamp to int16_t
        int16_t rx = (int16_t)_time.getRandomRange(-dist, dist);
        int16_t ry = (int16_t)_time.getRandomRange(-dist, dist);
        
        // Ensure at least some movement if range is small but dist > 0
        if (rx == 0 && ry == 0 && dist > 0) {
            rx = CONFIG::AIR_MOUSE::JIGGLER::MIN_MOVE_DISTANCE;
        }

        queueMovement(rx, ry);
        // No return movement for Active mode
    }
}

void JigglerController::updateInterval(const RTC::JigglerData& config) {
    if (config.randomInterval) {
        // Randomize +/- 50%
        uint32_t half = config.interval / CONFIG::AIR_MOUSE::JIGGLER::RANDOM_INTERVAL_SPREAD_DIVISOR;
        if (half < CONFIG::AIR_MOUSE::JIGGLER::MIN_INTERVAL_S) {
            half = CONFIG::AIR_MOUSE::JIGGLER::MIN_INTERVAL_S;
        }
        int32_t offset = _time.getRandomRange(-(int)half, (int)half);
        
        // Use int64 to prevent underflow before assignment
        int64_t next = (int64_t)config.interval + offset;
        if (next < CONFIG::AIR_MOUSE::JIGGLER::MIN_INTERVAL_S) {
            next = CONFIG::AIR_MOUSE::JIGGLER::MIN_INTERVAL_S;
        }
        
        _currentInterval = (uint32_t)next;
    } else {
        _currentInterval = config.interval;
    }
}

void JigglerController::notifyUserActivity() {
    _lastUserActivityTime.store(_time.getMillis(), std::memory_order_release);
}

bool JigglerController::isPaused() const {
    const uint32_t lastActivity = _lastUserActivityTime.load(std::memory_order_acquire);
    if (lastActivity == 0) return false;
    // Unsigned subtraction handles millis() wrap-around automatically
    uint32_t now = _time.getMillis();
    uint32_t diff = now - lastActivity;
    return diff < PAUSE_DURATION_MS;
}



void JigglerController::queueMovement(int16_t x, int16_t y) {
    // Accumulate leftover movement; it will be chunked in update().
    _leftoverX += x;
    _leftoverY += y;
}

void JigglerController::updateBezier(const RTC::JigglerData& config) {
    uint32_t now = _time.getMillis();

    // 1. If idle, check if it's time to start a new curve
    if (!_bezier.active) {
        if (now - _lastTriggerTime > (_currentInterval * 1000)) {
             _bezier.active = true;
             _bezier.t = 0;
             _bezier.accX = 0;
             _bezier.accY = 0;
             
             // Define Curve
             // Start is visually (0,0) relative to current cursor position
             _bezier.startX = 0; 
             _bezier.startY = 0;
             
             // End point: Random point within radius
             int16_t dist = (int16_t)config.moveDistance;
             _bezier.endX = (int16_t)_time.getRandomRange(-dist, dist);
             _bezier.endY = (int16_t)_time.getRandomRange(-dist, dist);
             
             // Control Points: Random points between start and end to create arc
             _bezier.control1X = (int16_t)_time.getRandomRange(-dist, dist);
             _bezier.control1Y = (int16_t)_time.getRandomRange(-dist, dist);
             _bezier.control2X = (int16_t)_time.getRandomRange(-dist, dist);
             _bezier.control2Y = (int16_t)_time.getRandomRange(-dist, dist);
             
             // Duration: 1-2 seconds for smooth movement
             // Calculate step based on update rate (approx 100ms task delay from Service)
             // Let's target 1.5s duration. 15 steps @ 100ms.
             _bezier.tStep = CONFIG::AIR_MOUSE::JIGGLER::HUMAN_BEZIER_T_STEP;
             
             _lastTriggerTime = now;
             updateInterval(config);
        }
        return;
    }

    // 2. Process Bezier step.
    _bezier.t += _bezier.tStep;
    if (_bezier.t > 1.0f) _bezier.t = 1.0f;

    float t = _bezier.t;
    float u = 1 - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    // Standard Cubic Bezier Formula
    // B(t) = (1-t)^3*P0 + 3(1-t)^2*t*P1 + 3(1-t)t^2*P2 + t^3*P3
    float pX = (uuu * _bezier.startX) + (3 * uu * t * _bezier.control1X) + (3 * u * tt * _bezier.control2X) + (ttt * _bezier.endX);
    float pY = (uuu * _bezier.startY) + (3 * uu * t * _bezier.control1Y) + (3 * u * tt * _bezier.control2Y) + (ttt * _bezier.endY);
    
    // Calculate delta from accumulated dispatched position
    // We track theoretical position in pX/pY, but we need to send relative moves
    float deltaX = pX - _bezier.accX;
    float deltaY = pY - _bezier.accY;

    // Send movement (rounded) as relative deltas.
    int16_t moveX = (int16_t)round(deltaX);
    int16_t moveY = (int16_t)round(deltaY);

    if (moveX != 0 || moveY != 0) {
        queueMovement(moveX, moveY);
        // Only add effectively dispatched movement back into the accumulator
        _bezier.accX += moveX;
        _bezier.accY += moveY;
    }

    // 3. Check for end.
    if (_bezier.t >= 1.0f) {
        _bezier.active = false;
    }
}

void JigglerController::triggerKeyboard() {
    if (_keyboard) {
        // Send F13 (invisible to most apps, commonly used for jiggler).
        // Key code configured in CONFIG::AIR_MOUSE::JIGGLER::KEYBOARD_F13.
        _keyboard->sendKey(CONFIG::AIR_MOUSE::JIGGLER::KEYBOARD_F13);
    }
}

} // namespace AIRMOUSE
