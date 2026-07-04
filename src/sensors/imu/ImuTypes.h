#pragma once

#include <cmath>
#include <cstdint>

namespace IMU {

struct ImuVector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct ImuSample {
    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    float gx = 0.0f;
    float gy = 0.0f;
    float gz = 0.0f;
    uint32_t timestampMs = 0;
    bool valid = false;
};

struct ImuMetrics {
    ImuSample sample;
    uint32_t sampleAgeMs = 0;
    bool sampleTimestampKnown = false;
    bool sampleFresh = false;
    float accelMagnitudeG = 0.0f;
    float accelDeltaG = 0.0f;
    float gyroMagnitudeDps = 0.0f;
    bool baselineValid = false;
    ImuVector3 baseline;
    float tiltDeg = 0.0f;
};

enum class ImuAlarmReason : uint8_t {
    None = 0,
    Tilt,
    Shock,
    Stale,
    NoBaseline,
    Unavailable
};

struct ImuAlarmConfig {
    bool enabled = false;
    bool baselineValid = false;
    float tiltThresholdDeg = 30.0f;
    float tiltHysteresisDeg = 5.0f;
    uint16_t tiltHoldMs = 750;
    uint16_t tiltClearHoldMs = 1500;
    float accelDeltaThresholdG = 0.35f;
};

struct ImuAlarmStatus {
    bool enabled = false;
    bool sampleFresh = false;
    bool baselineValid = false;
    bool triggered = false;
    bool pendingTrigger = false;
    bool pendingClear = false;
    ImuAlarmReason reason = ImuAlarmReason::None;
    float currentTiltDeg = NAN;
    float accelDeltaG = 0.0f;
    float triggerValue = 0.0f;
    uint32_t triggerHoldElapsedMs = 0;
    uint32_t clearHoldElapsedMs = 0;
};

enum class OrientationCalibrationStatus : uint8_t {
    Success = 0,
    NotRunning,
    NoFreshSamples,
    Unstable,
    InvalidVector
};

struct OrientationCalibrationResult {
    OrientationCalibrationStatus status = OrientationCalibrationStatus::NoFreshSamples;
    ImuVector3 baseline;
    float accelMagnitudeMean = 0.0f;
    float accelMagnitudeVariance = 0.0f;
    uint16_t sampleCount = 0;
};

}  // namespace IMU
