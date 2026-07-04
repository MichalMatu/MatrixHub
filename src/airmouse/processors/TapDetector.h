#pragma once

#include <cstdint>
#include "../config/AirMouseConfig.h"

namespace AIRMOUSE {

enum class TapGesture : uint8_t {
    NONE = 0,
    SINGLE = 1,
    DOUBLE = 2,
    TRIPLE = 3
};

class TapDetector {
public:
    TapDetector();

    void updateConfig(const AirMouseConfig& config);
    void reset();
    
    // Returns tap gesture count after window expires (NONE, SINGLE, DOUBLE, TRIPLE)
    TapGesture process(float ax, float ay, float az, uint32_t now);

    float getLastDeltaG() const;

private:
    // Thresholds and windows (from config).
    float _tapThresholdG = CONFIG::AIR_MOUSE::DEFAULT_TAP_THRESHOLD_G;
    uint32_t _clickDebounceMs = CONFIG::AIR_MOUSE::DEFAULT_CLICK_DEBOUNCE_MS;
    uint32_t _doubleClickWindowMs = CONFIG::AIR_MOUSE::DEFAULT_DOUBLE_CLICK_WINDOW_MS;

    float _lastTotalG2 = 0;
    float _tapThresholdLow2 = 0;
    float _tapThresholdHigh2 = 0;
    uint32_t _lastClickTime = 0;
    uint32_t _lastTapTime = 0;
    uint8_t _tapCount = 0;

    // Helper to update internal tap counter
    void handleTap(uint32_t now, float deltaG);

    void updateThresholds();
};

} // namespace AIRMOUSE
