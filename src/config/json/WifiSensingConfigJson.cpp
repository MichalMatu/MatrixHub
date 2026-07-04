#include "WifiSensingConfigJson.h"
#include "../App.h"
#include "ConfigKeys.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../wifisensing/csi/algo/CsiMotionTypes.h"
#include <algorithm>
#include <cmath>

namespace CONFIG {
namespace JSON {
namespace {

template <typename T>
T clampValue(T value, T minValue, T maxValue) {
    return (std::max)(minValue, (std::min)(value, maxValue));
}

float clampFloat(float value, float minValue, float maxValue, float fallback) {
    if (!std::isfinite(value)) {
        value = fallback;
    }
    return clampValue(value, minValue, maxValue);
}

void normalizeCsiAlarmConfig(RTC::WifiSensingData& data) {
    data.csiAlarmBandCount = clampValue<uint8_t>(data.csiAlarmBandCount, 0, 4);
    for (uint8_t i = 0; i < data.csiAlarmBandCount; ++i) {
        data.csiAlarmBandStart[i] = clampValue<uint16_t>(data.csiAlarmBandStart[i], 0, 255);
        data.csiAlarmBandEnd[i] = clampValue<uint16_t>(data.csiAlarmBandEnd[i], 0, 255);
        if (data.csiAlarmBandStart[i] > data.csiAlarmBandEnd[i]) {
            const uint16_t tmp = data.csiAlarmBandStart[i];
            data.csiAlarmBandStart[i] = data.csiAlarmBandEnd[i];
            data.csiAlarmBandEnd[i] = tmp;
        }
    }
    for (uint8_t i = data.csiAlarmBandCount; i < 4; ++i) {
        data.csiAlarmBandStart[i] = 0;
        data.csiAlarmBandEnd[i] = 0;
    }

    data.csiBaselineFrames = clampValue<uint16_t>(data.csiBaselineFrames, 30, 1000);
    data.csiTopK = clampValue<uint8_t>(data.csiTopK, 1, 32);
    data.csiSensitivity = WIFISENSING::CSI::normalizeCsiMotionSensitivity(data.csiSensitivity);
    const WIFISENSING::CSI::CsiMotionSensitivityPreset sensitivityPreset =
        WIFISENSING::CSI::csiMotionSensitivityPreset(data.csiSensitivity);
    data.csiEnterThreshold = sensitivityPreset.enterThreshold;
    data.csiClearThreshold = sensitivityPreset.clearThreshold;
    data.csiHoldMs = clampValue<uint16_t>(data.csiHoldMs, 100, 10000);
    data.csiClearHoldMs = clampValue<uint16_t>(data.csiClearHoldMs, 100, 30000);
    data.csiMinNoise = clampFloat(data.csiMinNoise, 0.1f, 1000.0f, 4.0f);
    data.csiMinEnergy = clampFloat(data.csiMinEnergy, 0.0f, 10000.0f, 4.0f);
    data.csiNoisyThreshold = clampFloat(data.csiNoisyThreshold, data.csiEnterThreshold, 500.0f, 80.0f);
}

void deserializeCsiAlarm(JsonObject& obj, RTC::WifiSensingData& data) {
    JsonVariant alarmVariant = obj[Keys::kCsiAlarm];
    if (!alarmVariant.is<JsonObject>()) {
        normalizeCsiAlarmConfig(data);
        return;
    }

    JsonObject alarm = alarmVariant.as<JsonObject>();
    if (alarm[Keys::kEnabled].is<bool>()) {
        data.csiAlarmEnabled = alarm[Keys::kEnabled].as<bool>();
    }

    JsonVariant bandsVariant = alarm[Keys::kBands];
    if (bandsVariant.is<JsonArray>()) {
        JsonArray bands = bandsVariant.as<JsonArray>();
        uint8_t count = 0;
        for (JsonVariant bandVariant : bands) {
            if (count >= 4 || !bandVariant.is<JsonObject>()) {
                continue;
            }
            JsonObject band = bandVariant.as<JsonObject>();
            const uint32_t startRaw = band[Keys::kStart].is<uint32_t>()
                                          ? band[Keys::kStart].as<uint32_t>()
                                          : 0;
            const uint32_t endRaw = band[Keys::kEnd].is<uint32_t>()
                                        ? band[Keys::kEnd].as<uint32_t>()
                                        : startRaw;
            uint16_t start = static_cast<uint16_t>(clampValue<uint32_t>(startRaw, 0, 255));
            uint16_t end = static_cast<uint16_t>(clampValue<uint32_t>(endRaw, 0, 255));
            if (start > end) {
                const uint16_t tmp = start;
                start = end;
                end = tmp;
            }
            data.csiAlarmBandStart[count] = start;
            data.csiAlarmBandEnd[count] = end;
            count++;
        }
        data.csiAlarmBandCount = count;
    }

    if (alarm[Keys::kBaselineFrames].is<uint32_t>()) {
        data.csiBaselineFrames =
            static_cast<uint16_t>(clampValue<uint32_t>(alarm[Keys::kBaselineFrames].as<uint32_t>(), 30, 1000));
    }
    if (alarm[Keys::kTopK].is<uint32_t>()) {
        data.csiTopK = static_cast<uint8_t>(clampValue<uint32_t>(alarm[Keys::kTopK].as<uint32_t>(), 1, 32));
    }
    if (alarm[Keys::kEnterThreshold].is<float>()) {
        data.csiEnterThreshold = alarm[Keys::kEnterThreshold].as<float>();
    }
    if (alarm[Keys::kClearThreshold].is<float>()) {
        data.csiClearThreshold = alarm[Keys::kClearThreshold].as<float>();
    }
    if (alarm[Keys::kHoldMs].is<uint32_t>()) {
        data.csiHoldMs =
            static_cast<uint16_t>(clampValue<uint32_t>(alarm[Keys::kHoldMs].as<uint32_t>(), 100, 10000));
    }
    if (alarm[Keys::kClearHoldMs].is<uint32_t>()) {
        data.csiClearHoldMs =
            static_cast<uint16_t>(clampValue<uint32_t>(alarm[Keys::kClearHoldMs].as<uint32_t>(), 100, 30000));
    }
    if (alarm[Keys::kMinNoise].is<float>()) {
        data.csiMinNoise = alarm[Keys::kMinNoise].as<float>();
    }
    if (alarm[Keys::kMinEnergy].is<float>()) {
        data.csiMinEnergy = alarm[Keys::kMinEnergy].as<float>();
    }
    if (alarm[Keys::kNoisyThreshold].is<float>()) {
        data.csiNoisyThreshold = alarm[Keys::kNoisyThreshold].as<float>();
    }
    if (alarm[Keys::kAutoRecalibration].is<bool>()) {
        data.csiAutoRecalibration = alarm[Keys::kAutoRecalibration].as<bool>();
    }
    if (alarm[Keys::kSensitivity].is<uint32_t>()) {
        data.csiSensitivity =
            static_cast<uint8_t>(clampValue<uint32_t>(alarm[Keys::kSensitivity].as<uint32_t>(), 0, 2));
    }

    normalizeCsiAlarmConfig(data);
}

} // namespace

void serializeWifiSensing(JsonObject& obj, const RTC::WifiSensingData& data) {
    RTC::WifiSensingData w = data;
    normalizeCsiAlarmConfig(w);

    obj[Keys::kEnabled] = w.enabled;
    obj[Keys::kSampleIntervalMs] = w.sampleIntervalMs;
    obj[Keys::kVarianceThreshold] = w.varianceThreshold;

    JsonObject alarm = obj[Keys::kCsiAlarm].to<JsonObject>();
    alarm[Keys::kEnabled] = w.csiAlarmEnabled;
    JsonArray bands = alarm[Keys::kBands].to<JsonArray>();
    for (uint8_t i = 0; i < w.csiAlarmBandCount; ++i) {
        JsonObject band = bands.add<JsonObject>();
        band[Keys::kStart] = w.csiAlarmBandStart[i];
        band[Keys::kEnd] = w.csiAlarmBandEnd[i];
    }
    alarm[Keys::kBaselineFrames] = w.csiBaselineFrames;
    alarm[Keys::kTopK] = w.csiTopK;
    alarm[Keys::kEnterThreshold] = w.csiEnterThreshold;
    alarm[Keys::kClearThreshold] = w.csiClearThreshold;
    alarm[Keys::kHoldMs] = w.csiHoldMs;
    alarm[Keys::kClearHoldMs] = w.csiClearHoldMs;
    alarm[Keys::kMinNoise] = w.csiMinNoise;
    alarm[Keys::kMinEnergy] = w.csiMinEnergy;
    alarm[Keys::kNoisyThreshold] = w.csiNoisyThreshold;
    alarm[Keys::kAutoRecalibration] = w.csiAutoRecalibration;
    alarm[Keys::kSensitivity] = w.csiSensitivity;
}

void deserializeWifiSensing(JsonObject& obj, RTC::WifiSensingData& data) {
    if (obj[Keys::kEnabled].is<bool>()) {
        bool v = obj[Keys::kEnabled].as<bool>();
        data.enabled = v;
    }
    
    if (obj[Keys::kSampleIntervalMs].is<uint32_t>()) {
        uint32_t interval = obj[Keys::kSampleIntervalMs].as<uint32_t>();
        data.sampleIntervalMs = (std::max)((uint32_t)LIMITS::WIFI_SENSING::MIN_INTERVAL_MS, 
                                    (std::min)(interval, (uint32_t)LIMITS::WIFI_SENSING::MAX_INTERVAL_MS));
    }
                                
    if (obj[Keys::kVarianceThreshold].is<float>()) {
        float variance = obj[Keys::kVarianceThreshold].as<float>();
        data.varianceThreshold = (std::max)(LIMITS::WIFI_SENSING::MIN_VARIANCE, 
                                     (std::min)(variance, LIMITS::WIFI_SENSING::MAX_VARIANCE));
    }

    deserializeCsiAlarm(obj, data);
}

void loadWifiSensing(JsonObject& obj) {
    RTC::updateConfigSection(&RTC::ConfigStore::wifiSensing, [&](RTC::WifiSensingData& wifiSensing) {
        deserializeWifiSensing(obj, wifiSensing);
    });
}

void saveWifiSensing(JsonObject& obj) {
    RTC::WifiSensingData w{};
    RTC::withConfig([&](const RTC::ConfigStore& cfg) {
        w = cfg.wifiSensing;
    });
    serializeWifiSensing(obj, w);
}

} // namespace JSON
} // namespace CONFIG
