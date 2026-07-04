#include "ImuConfigJson.h"

#include "../../system/rtc/RtcConfig.h"
#include "../App.h"

#include <algorithm>

namespace CONFIG {
namespace JSON {

namespace {

float clampFloat(float value, float minValue, float maxValue) {
    return (std::max)(minValue, (std::min)(value, maxValue));
}

uint16_t clampU16(uint32_t value, uint16_t minValue, uint16_t maxValue) {
    return static_cast<uint16_t>((std::max)(static_cast<uint32_t>(minValue),
                                           (std::min)(value, static_cast<uint32_t>(maxValue))));
}

void readBaseline(JsonObject& obj, RTC::ImuData& data) {
    if (!obj[Keys::kOrientationBaseline].is<JsonObject>()) {
        return;
    }

    JsonObject baseline = obj[Keys::kOrientationBaseline].as<JsonObject>();
    if (baseline[Keys::kX].is<float>() || baseline[Keys::kX].is<int>()) {
        data.orientationBaselineX = baseline[Keys::kX].as<float>();
    }
    if (baseline[Keys::kY].is<float>() || baseline[Keys::kY].is<int>()) {
        data.orientationBaselineY = baseline[Keys::kY].as<float>();
    }
    if (baseline[Keys::kZ].is<float>() || baseline[Keys::kZ].is<int>()) {
        data.orientationBaselineZ = baseline[Keys::kZ].as<float>();
    }
}

void writeBaseline(JsonObject& obj, const RTC::ImuData& data) {
    JsonObject baseline = obj[Keys::kOrientationBaseline].to<JsonObject>();
    baseline[Keys::kX] = data.orientationBaselineX;
    baseline[Keys::kY] = data.orientationBaselineY;
    baseline[Keys::kZ] = data.orientationBaselineZ;
}

}  // namespace

void deserializeImu(JsonObject& obj, RTC::ImuData& data) {
    if (obj[Keys::kUiMonitorEnabled].is<bool>()) {
        data.uiMonitorEnabled = obj[Keys::kUiMonitorEnabled].as<bool>();
    }
    if (obj[Keys::kAlarmMonitorEnabled].is<bool>()) {
        data.alarmMonitorEnabled = obj[Keys::kAlarmMonitorEnabled].as<bool>();
    }
    if (obj[Keys::kOrientationBaselineValid].is<bool>()) {
        data.orientationBaselineValid = obj[Keys::kOrientationBaselineValid].as<bool>();
    }

    readBaseline(obj, data);

    if (obj[Keys::kBaselineCalibratedAt].is<uint32_t>()) {
        data.baselineCalibratedAt = obj[Keys::kBaselineCalibratedAt].as<uint32_t>();
    }
    if (obj[Keys::kCalibrationRevision].is<uint32_t>()) {
        data.calibrationRevision = obj[Keys::kCalibrationRevision].as<uint32_t>();
    }
    if (obj[Keys::kTiltThresholdDeg].is<float>() || obj[Keys::kTiltThresholdDeg].is<int>()) {
        data.tiltThresholdDeg = clampFloat(obj[Keys::kTiltThresholdDeg].as<float>(), 1.0f, 180.0f);
    }
    if (obj[Keys::kTiltHysteresisDeg].is<float>() || obj[Keys::kTiltHysteresisDeg].is<int>()) {
        data.tiltHysteresisDeg = clampFloat(obj[Keys::kTiltHysteresisDeg].as<float>(), 0.0f, 60.0f);
    }
    if (obj[Keys::kTiltHoldMs].is<uint32_t>() || obj[Keys::kTiltHoldMs].is<int>()) {
        data.tiltHoldMs = clampU16(obj[Keys::kTiltHoldMs].as<uint32_t>(), 0, 60000);
    }
    if (obj[Keys::kTiltClearHoldMs].is<uint32_t>() || obj[Keys::kTiltClearHoldMs].is<int>()) {
        data.tiltClearHoldMs = clampU16(obj[Keys::kTiltClearHoldMs].as<uint32_t>(), 0, 60000);
    }
    if (obj[Keys::kAccelDeltaThresholdG].is<float>() || obj[Keys::kAccelDeltaThresholdG].is<int>()) {
        data.accelDeltaThresholdG = clampFloat(obj[Keys::kAccelDeltaThresholdG].as<float>(), 0.01f, 4.0f);
    }
}

void loadImu(JsonObject& obj) {
    if (obj.isNull()) {
        return;
    }

    RTC::updateConfigSection(&RTC::ConfigStore::imu, [&](RTC::ImuData& imu) {
        deserializeImu(obj, imu);
    });
}

void saveImu(JsonObject& obj) {
    const RTC::ImuData imu = RTC::copyConfigSection(&RTC::ConfigStore::imu);
    obj[Keys::kUiMonitorEnabled] = imu.uiMonitorEnabled;
    obj[Keys::kAlarmMonitorEnabled] = imu.alarmMonitorEnabled;
    obj[Keys::kOrientationBaselineValid] = imu.orientationBaselineValid;
    writeBaseline(obj, imu);
    obj[Keys::kBaselineCalibratedAt] = imu.baselineCalibratedAt;
    obj[Keys::kCalibrationRevision] = imu.calibrationRevision;
    obj[Keys::kTiltThresholdDeg] = imu.tiltThresholdDeg;
    obj[Keys::kTiltHysteresisDeg] = imu.tiltHysteresisDeg;
    obj[Keys::kTiltHoldMs] = imu.tiltHoldMs;
    obj[Keys::kTiltClearHoldMs] = imu.tiltClearHoldMs;
    obj[Keys::kAccelDeltaThresholdG] = imu.accelDeltaThresholdG;
}

}  // namespace JSON
}  // namespace CONFIG
