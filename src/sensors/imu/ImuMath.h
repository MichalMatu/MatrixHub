#pragma once

#include "ImuTypes.h"

#include <cstdint>

namespace IMU::MATH {

float magnitude(float x, float y, float z);
float accelDeltaG(float ax, float ay, float az);
bool normalize(const ImuVector3& input, ImuVector3& output);
float tiltDegrees(const ImuVector3& baseline, const ImuVector3& current);
bool isAccelMagnitudeStable(float magnitudeG);
uint32_t elapsedMs(uint32_t nowMs, uint32_t thenMs);
const char* calibrationStatusToString(OrientationCalibrationStatus status);
const char* alarmReasonToString(ImuAlarmReason reason);

}  // namespace IMU::MATH
