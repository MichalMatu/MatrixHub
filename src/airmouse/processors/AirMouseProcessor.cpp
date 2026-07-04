#include "../../system/logging/Logging.h"
#include "../../config/System.h"
#include "AirMouseProcessor.h"
#include <cmath>
#include <algorithm> // std::clamp

#undef LOG_TAG
#define LOG_TAG "AirMouseProc"

namespace AIRMOUSE {

AirMouseProcessor::AirMouseProcessor() 
    : _filterX(
          CONFIG::AIR_MOUSE::FILTER::DEFAULT_MIN_CUTOFF,
          CONFIG::AIR_MOUSE::FILTER::DEFAULT_BETA,
          CONFIG::AIR_MOUSE::FILTER::DEFAULT_D_CUTOFF),
      _filterY(
          CONFIG::AIR_MOUSE::FILTER::DEFAULT_MIN_CUTOFF,
          CONFIG::AIR_MOUSE::FILTER::DEFAULT_BETA,
          CONFIG::AIR_MOUSE::FILTER::DEFAULT_D_CUTOFF) {
}

void AirMouseProcessor::updateConfig(const AirMouseConfig& mouseConfig, const FilterConfig& filterConfig) {
    _config = mouseConfig;
    
    // Update filters if params changed
    _filterX = UTILS::MATH::OneEuroFilter(filterConfig.minCutoff, filterConfig.beta, filterConfig.dCutoff);
    _filterY = UTILS::MATH::OneEuroFilter(filterConfig.minCutoff, filterConfig.beta, filterConfig.dCutoff);
}

void AirMouseProcessor::calibrate(float gx, float gy, float gz) {
    _gyroOffsetX = gx;
    _gyroOffsetY = gy;
    _gyroOffsetZ = gz;
}

void AirMouseProcessor::resetFilters() {
    _filterX.reset();
    _filterY.reset();
    _lastProcessMs = 0;
    _lastRoll = 0.0f;
}

MouseMovement AirMouseProcessor::process(float gx, float gy, float gz, float ax, float ay, float az, uint32_t now) {
    // Compute time delta (seconds) for stable per-sample scaling.
    float dtSec = CONFIG::TASKS::AIR_MOUSE_INTERVAL_MS / 1000.0f;
    if (_lastProcessMs != 0) {
        uint32_t dtMs = (now >= _lastProcessMs)
            ? (now - _lastProcessMs)
            : CONFIG::TASKS::AIR_MOUSE_INTERVAL_MS; // millis() wrap
        dtSec = std::clamp(dtMs / 1000.0f, 0.001f, 0.05f);
    }
    _lastProcessMs = now;

    // Apply offsets
    float cleanGx = gx - _gyroOffsetX;
    float cleanGy = gy - _gyroOffsetY;
    float cleanGz = gz - _gyroOffsetZ;

    _lastGyroX = cleanGx;
    _lastGyroY = cleanGy;
    _lastGyroZ = cleanGz;
    _lastAccX = ax;
    _lastAccY = ay;
    _lastAccZ = az;

    // Apply 1-Euro filter (noise reduction with minimal lag).
    float smoothZ = _filterX.filter(cleanGz, now);
    float smoothY = _filterY.filter(cleanGy, now);

    // ================================================================
    // ROLL COMPENSATION
    // ================================================================
    // Assuming X is forward (pointing to screen):
    // Roll is rotation around X. 
    // We use AccY and AccZ to determine the roll angle.
    const float accelMag = sqrtf(ax*ax + ay*ay + az*az);
    const bool accelStable = fabsf(accelMag - CONFIG::AIR_MOUSE::PROCESSOR::GRAVITY_BASELINE_G) <=
        CONFIG::AIR_MOUSE::PROCESSOR::ROLL_GATING_G;
    const float gyroSat = CONFIG::AIR_MOUSE::PROCESSOR::GYRO_RANGE_DPS *
        CONFIG::AIR_MOUSE::PROCESSOR::GYRO_SAT_FRACTION;
    const bool gyroSaturated = (fabsf(cleanGy) > gyroSat) || (fabsf(cleanGz) > gyroSat);

    float roll = _lastRoll;
    if (accelStable && !gyroSaturated) {
        roll = atan2f(ay, az);
        _lastRoll = roll;
    }
    
    float cosRoll = cosf(roll);
    float sinRoll = sinf(roll);
    
    // De-rotate gyro readings (compensate for roll).
    float compGyroY = smoothY * cosRoll - smoothZ * sinRoll;
    float compGyroZ = smoothY * sinRoll + smoothZ * cosRoll;
    
    // Map to screen axes:
    // Z-rotation (Yaw) -> X movement, Y-rotation (Pitch) -> Y movement

    // ================================================================
    // DEADZONE & MOVEMENT
    // ================================================================
    float logicZ = fabsf(compGyroZ) < _config.deadzone ? 0.0f : compGyroZ;
    float logicY = fabsf(compGyroY) < _config.deadzone ? 0.0f : compGyroY;

    if (logicZ == 0 && logicY == 0) {
        return {0, 0};
    }

    float rawSpeed = sqrtf(logicZ*logicZ + logicY*logicY);
    float accelMultiplier = calculateAcceleration(rawSpeed);

    // Scale to mouse units (sensitivity + acceleration).
    float vx = -logicZ * accelMultiplier * dtSec * _config.sensitivityX *
               CONFIG::AIR_MOUSE::PROCESSOR::OUTPUT_GAIN /
               CONFIG::AIR_MOUSE::PROCESSOR::SENSITIVITY_NORMALIZATION;
    float vy = -logicY * accelMultiplier * dtSec * _config.sensitivityY *
               CONFIG::AIR_MOUSE::PROCESSOR::OUTPUT_GAIN /
               CONFIG::AIR_MOUSE::PROCESSOR::SENSITIVITY_NORMALIZATION;

    return { vx, vy };
}

float AirMouseProcessor::calculateAcceleration(float rawSpeed) {
    const float neutral = CONFIG::AIR_MOUSE::PROCESSOR::ACCELERATION_NEUTRAL_MULTIPLIER;
    if (!_config.accelerationEnabled || _config.accelerationFactor <= neutral) {
        return neutral;
    }

    if (rawSpeed < CONFIG::AIR_MOUSE::PROCESSOR::ACCELERATION_MIN_SPEED ||
        rawSpeed <= CONFIG::AIR_MOUSE::PROCESSOR::ACCELERATION_THRESHOLD) {
        return neutral; // 1:1 below threshold
    }

    const float speedRatio = rawSpeed / CONFIG::AIR_MOUSE::PROCESSOR::ACCELERATION_THRESHOLD;
    const float exponent = _config.accelerationFactor - neutral;
    float multiplier = powf(speedRatio, exponent);
    if (multiplier < neutral) multiplier = neutral;

    return std::clamp(
        multiplier,
        neutral,
        CONFIG::AIR_MOUSE::PROCESSOR::ACCELERATION_MAX_MULTIPLIER);
}


void AirMouseProcessor::getOffsets(float& x, float& y, float& z) const {
    x = _gyroOffsetX;
    y = _gyroOffsetY;
    z = _gyroOffsetZ;
}

void AirMouseProcessor::getStats(float& lastGx, float& lastGy, float& lastGz, float& lastAx, float& lastAy, float& lastAz) const {
    lastGx = _lastGyroX;
    lastGy = _lastGyroY;
    lastGz = _lastGyroZ;
    lastAx = _lastAccX;
    lastAy = _lastAccY;
    lastAz = _lastAccZ;
}

} // namespace AIRMOUSE
