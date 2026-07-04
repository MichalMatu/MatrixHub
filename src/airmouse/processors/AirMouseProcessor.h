#pragma once

#include <cstdint>
#include "../config/AirMouseConfig.h"
#include "../../utils/math/OneEuroFilter.h"

namespace AIRMOUSE {

struct MouseMovement {
    float x;
    float y;
};

class AirMouseProcessor {
public:
    AirMouseProcessor();
    
    // Update runtime config and filter parameters.
    void updateConfig(const AirMouseConfig& mouseConfig, const FilterConfig& filterConfig);
    void calibrate(float gx, float gy, float gz);
    void resetFilters();
    
    // Returns movement vector
    MouseMovement process(float gx, float gy, float gz, float ax, float ay, float az, uint32_t now);

    void getOffsets(float& x, float& y, float& z) const;
    void getStats(float& lastGx, float& lastGy, float& lastGz, float& lastAx, float& lastAy, float& lastAz) const;

private:
    AirMouseConfig _config;
    
    // 1-Euro filters for gyro smoothing.
    UTILS::MATH::OneEuroFilter _filterX;
    UTILS::MATH::OneEuroFilter _filterY;

    // Offsets
    float _gyroOffsetX = 0;
    float _gyroOffsetY = 0;
    float _gyroOffsetZ = 0;

    // State for debugging
    float _lastGyroX = 0, _lastGyroY = 0, _lastGyroZ = 0;
    float _lastAccX = 0, _lastAccY = 0, _lastAccZ = 0;
    uint32_t _lastProcessMs = 0;
    float _lastRoll = 0.0f;

    float calculateAcceleration(float rawSpeed);
};

} // namespace AIRMOUSE
