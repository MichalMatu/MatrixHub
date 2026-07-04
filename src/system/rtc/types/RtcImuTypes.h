/**
 * @file RtcImuTypes.h
 * @brief RTC-persistent IMU runtime/settings state
 */

#pragma once

#include "../RtcDefaultValues.h"
#include <cstdint>

namespace RTC {

struct __attribute__((packed)) ImuData {
    bool uiMonitorEnabled = Defaults::Imu::UiMonitorEnabled;
    bool alarmMonitorEnabled = Defaults::Imu::AlarmMonitorEnabled;
    bool orientationBaselineValid = false;
    uint8_t _pad = 0;

    float orientationBaselineX = Defaults::Imu::BaselineX;
    float orientationBaselineY = Defaults::Imu::BaselineY;
    float orientationBaselineZ = Defaults::Imu::BaselineZ;

    uint32_t baselineCalibratedAt = 0;
    uint32_t calibrationRevision = 0;

    float tiltThresholdDeg = Defaults::Imu::TiltThresholdDeg;
    float tiltHysteresisDeg = Defaults::Imu::TiltHysteresisDeg;
    uint16_t tiltHoldMs = Defaults::Imu::TiltHoldMs;
    uint16_t tiltClearHoldMs = Defaults::Imu::TiltClearHoldMs;
    float accelDeltaThresholdG = Defaults::Imu::AccelDeltaThresholdG;
};

}  // namespace RTC
