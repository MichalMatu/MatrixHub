#pragma once

#include <cstdint>
#include "../../config/System.h"

namespace AIRMOUSE {

// Immutable component configs consumed by processor/detector.
// Values are filled from RTC config and defaults come from System.h.
struct AirMouseConfig {
    float sensitivityX = CONFIG::AIR_MOUSE::DEFAULT_SENSITIVITY_X;
    float sensitivityY = CONFIG::AIR_MOUSE::DEFAULT_SENSITIVITY_Y;
    float deadzone = CONFIG::AIR_MOUSE::DEFAULT_DEADZONE;
    bool accelerationEnabled = CONFIG::AIR_MOUSE::DEFAULT_ACCELERATION_ENABLED;
    float accelerationFactor = CONFIG::AIR_MOUSE::DEFAULT_ACCELERATION_FACTOR;
    
    // Tap Detection
    float tapThresholdG = CONFIG::AIR_MOUSE::DEFAULT_TAP_THRESHOLD_G;
    uint16_t clickDebounceMs = CONFIG::AIR_MOUSE::DEFAULT_CLICK_DEBOUNCE_MS;
    uint16_t doubleClickWindowMs = CONFIG::AIR_MOUSE::DEFAULT_DOUBLE_CLICK_WINDOW_MS;
};

// Runtime tunable filter parameters (1-Euro filter).
struct FilterConfig {
    float minCutoff = CONFIG::AIR_MOUSE::FILTER::DEFAULT_MIN_CUTOFF;
    float beta = CONFIG::AIR_MOUSE::FILTER::DEFAULT_BETA;
    float dCutoff = CONFIG::AIR_MOUSE::FILTER::DEFAULT_D_CUTOFF;
};

} // namespace AIRMOUSE
