#include "ImuMath.h"

#include <algorithm>
#include <cmath>

namespace IMU::MATH {

namespace {
constexpr float kMinVectorMagnitude = 0.001f;
constexpr float kRadToDeg = 57.29577951308232f;
constexpr float kStableAccelMinG = 0.85f;
constexpr float kStableAccelMaxG = 1.15f;
}  // namespace

float magnitude(float x, float y, float z) {
    return sqrtf((x * x) + (y * y) + (z * z));
}

float accelDeltaG(float ax, float ay, float az) {
    return fabsf(magnitude(ax, ay, az) - 1.0f);
}

bool normalize(const ImuVector3& input, ImuVector3& output) {
    const float mag = magnitude(input.x, input.y, input.z);
    if (!std::isfinite(mag) || mag < kMinVectorMagnitude) {
        output = {};
        return false;
    }

    output.x = input.x / mag;
    output.y = input.y / mag;
    output.z = input.z / mag;
    return true;
}

float tiltDegrees(const ImuVector3& baseline, const ImuVector3& current) {
    ImuVector3 normBase;
    ImuVector3 normCurrent;
    if (!normalize(baseline, normBase) || !normalize(current, normCurrent)) {
        return NAN;
    }

    float dot = (normBase.x * normCurrent.x) +
                (normBase.y * normCurrent.y) +
                (normBase.z * normCurrent.z);
    dot = (std::max)(-1.0f, (std::min)(1.0f, dot));
    return acosf(dot) * kRadToDeg;
}

bool isAccelMagnitudeStable(float magnitudeG) {
    return std::isfinite(magnitudeG) &&
           magnitudeG >= kStableAccelMinG &&
           magnitudeG <= kStableAccelMaxG;
}

uint32_t elapsedMs(uint32_t nowMs, uint32_t thenMs) {
    return nowMs >= thenMs ? nowMs - thenMs : (0xFFFFFFFFu - thenMs) + nowMs + 1u;
}

const char* calibrationStatusToString(OrientationCalibrationStatus status) {
    switch (status) {
        case OrientationCalibrationStatus::Success: return "success";
        case OrientationCalibrationStatus::NotRunning: return "not_running";
        case OrientationCalibrationStatus::NoFreshSamples: return "no_fresh_samples";
        case OrientationCalibrationStatus::Unstable: return "unstable";
        case OrientationCalibrationStatus::InvalidVector: return "invalid_vector";
        default: return "unknown";
    }
}

const char* alarmReasonToString(ImuAlarmReason reason) {
    switch (reason) {
        case ImuAlarmReason::None: return "none";
        case ImuAlarmReason::Tilt: return "tilt";
        case ImuAlarmReason::Shock: return "shock";
        case ImuAlarmReason::Stale: return "stale";
        case ImuAlarmReason::NoBaseline: return "no_baseline";
        case ImuAlarmReason::Unavailable: return "unavailable";
        default: return "unknown";
    }
}

}  // namespace IMU::MATH
