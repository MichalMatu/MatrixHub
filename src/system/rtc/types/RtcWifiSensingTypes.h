#pragma once

#include <Arduino.h>
#include "../RtcDefaultValues.h"

namespace RTC {

/**
 * WiFi Sensing configuration
 */
struct __attribute__((packed)) WifiSensingData {
    bool enabled = Defaults::WifiSensing::Enabled;
    uint16_t sampleIntervalMs = Defaults::WifiSensing::SampleIntervalMs;
    float varianceThreshold = Defaults::WifiSensing::VarianceThreshold;
    bool csiAlarmEnabled = Defaults::WifiSensing::CsiAlarmEnabled;
    uint8_t csiAlarmBandCount = 0;
    uint16_t csiAlarmBandStart[4] = {};
    uint16_t csiAlarmBandEnd[4] = {};
    uint16_t csiBaselineFrames = Defaults::WifiSensing::CsiBaselineFrames;
    uint8_t csiTopK = Defaults::WifiSensing::CsiTopK;
    float csiEnterThreshold = Defaults::WifiSensing::CsiEnterThreshold;
    float csiClearThreshold = Defaults::WifiSensing::CsiClearThreshold;
    uint16_t csiHoldMs = Defaults::WifiSensing::CsiHoldMs;
    uint16_t csiClearHoldMs = Defaults::WifiSensing::CsiClearHoldMs;
    float csiMinNoise = Defaults::WifiSensing::CsiMinNoise;
    float csiMinEnergy = Defaults::WifiSensing::CsiMinEnergy;
    float csiNoisyThreshold = Defaults::WifiSensing::CsiNoisyThreshold;
    bool csiAutoRecalibration = Defaults::WifiSensing::CsiAutoRecalibration;
    uint8_t csiSensitivity = Defaults::WifiSensing::CsiSensitivity;
};

} // namespace RTC
