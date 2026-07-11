#include "core/config/serialization/ConfigLoaders.h"

#include "config/json/AirMouseConfigJson.h"
#include "config/json/AlarmConfigJson.h"
#include "config/json/BleConfigJson.h"
#include "config/json/CompensationConfigJson.h"
#include "config/json/ConfigKeys.h"
#include "config/json/GpioConfigJson.h"
#include "config/json/KeyboardConfigJson.h"
#include "config/json/ImuConfigJson.h"
#include "config/json/MacroConfigJson.h"
#include "config/json/MatrixConfigJson.h"
#include "config/json/NotificationSettingsJson.h"
#include "config/json/PowerSettingsJson.h"
#include "config/json/ShellyConfigJson.h"
#include "config/json/SystemConfigJson.h"
#include "config/json/UsbTerminalConfigJson.h"
#include "config/json/WifiSensingConfigJson.h"
#include "system/memory/PsramAllocator.h"
#include "alarms/types/AlarmEnums.h"
#include <ArduinoJson.h>
#include <cstring>

namespace {

template <typename LoaderFn>
void loadIfObject(SYSTEM::SpiRamJsonDocument& doc, const char* key, LoaderFn loader) {
    if (!doc[key].is<JsonObject>()) {
        return;
    }
    JsonObject obj = doc[key].as<JsonObject>();
    loader(obj);
}

bool loadAlarmSection(SYSTEM::SpiRamJsonDocument& doc) {
    JsonVariant section = doc[CONFIG::Keys::kAlarms];
    if (section.isNull()) {
        // A present config document is authoritative. Silently accepting a
        // missing safety-critical section would turn a truncated/partial file
        // into an empty alarm set on cold boot. A genuinely missing config file
        // is handled separately by ConfigManager and still uses factory
        // defaults.
        return false;
    }
    if (!section.is<JsonObject>()) {
        return false;
    }

    JsonObject alarms = section.as<JsonObject>();
    return CONFIG::JSON::loadAlarms(alarms);
}

struct AlarmSourceDependencies {
    bool wifiRssi = false;
    bool wifiCsi = false;
    bool imu = false;
    bool gpio = false;
};

bool sourceMatches(JsonVariant source,
                   ALARMS::AlarmSource expected,
                   const char* expectedName) {
    if (source.is<int>()) {
        return source.as<int>() == static_cast<int>(expected);
    }
    const char* value = source | static_cast<const char*>(nullptr);
    return value && std::strcmp(value, expectedName) == 0;
}

AlarmSourceDependencies collectAlarmSourceDependencies(
    SYSTEM::SpiRamJsonDocument& doc) {
    AlarmSourceDependencies dependencies;
    JsonArray rules = doc[CONFIG::Keys::kAlarms][CONFIG::Keys::kRules]
                          .as<JsonArray>();
    for (JsonObject rule : rules) {
        if (!rule[CONFIG::Keys::kEnabled].is<bool>() ||
            !rule[CONFIG::Keys::kEnabled].as<bool>()) {
            continue;
        }
        JsonVariant source = rule[CONFIG::Keys::kSource];
        dependencies.wifiRssi = dependencies.wifiRssi ||
            sourceMatches(source, ALARMS::AlarmSource::WifiMotion, "wifi_motion");
        dependencies.wifiCsi = dependencies.wifiCsi ||
            sourceMatches(source, ALARMS::AlarmSource::WifiCsiMotion, "wifi_csi_motion");
        dependencies.imu = dependencies.imu ||
            sourceMatches(source, ALARMS::AlarmSource::ImuTamper, "imu_tamper");
        dependencies.gpio = dependencies.gpio ||
            sourceMatches(source, ALARMS::AlarmSource::GpioDigital, "gpio_digital");
    }
    return dependencies;
}

bool isNumber(JsonVariant value) {
    return value.is<float>() || value.is<double>() ||
           value.is<int>() || value.is<unsigned int>();
}

bool validateCsiConfig(JsonObject wifi) {
    JsonVariant alarmVariant = wifi[CONFIG::Keys::kCsiAlarm];
    if (!alarmVariant.is<JsonObject>()) {
        return false;
    }
    JsonObject alarm = alarmVariant.as<JsonObject>();
    if (!alarm[CONFIG::Keys::kEnabled].is<bool>() ||
        !alarm[CONFIG::Keys::kBands].is<JsonArray>() ||
        !alarm[CONFIG::Keys::kBaselineFrames].is<uint32_t>() ||
        !alarm[CONFIG::Keys::kTopK].is<uint32_t>() ||
        !isNumber(alarm[CONFIG::Keys::kEnterThreshold]) ||
        !isNumber(alarm[CONFIG::Keys::kClearThreshold]) ||
        !alarm[CONFIG::Keys::kHoldMs].is<uint32_t>() ||
        !alarm[CONFIG::Keys::kClearHoldMs].is<uint32_t>() ||
        !isNumber(alarm[CONFIG::Keys::kMinNoise]) ||
        !isNumber(alarm[CONFIG::Keys::kMinEnergy]) ||
        !isNumber(alarm[CONFIG::Keys::kNoisyThreshold]) ||
        !alarm[CONFIG::Keys::kAutoRecalibration].is<bool>() ||
        !alarm[CONFIG::Keys::kSensitivity].is<uint32_t>()) {
        return false;
    }

    for (JsonVariant bandVariant : alarm[CONFIG::Keys::kBands].as<JsonArray>()) {
        if (!bandVariant.is<JsonObject>()) {
            return false;
        }
        JsonObject band = bandVariant.as<JsonObject>();
        if (!band[CONFIG::Keys::kStart].is<uint32_t>() ||
            !band[CONFIG::Keys::kEnd].is<uint32_t>()) {
            return false;
        }
    }
    return true;
}

bool validateWifiDependency(SYSTEM::SpiRamJsonDocument& doc,
                            const AlarmSourceDependencies& dependencies) {
    if (!dependencies.wifiRssi && !dependencies.wifiCsi) {
        return true;
    }
    JsonVariant section = doc[CONFIG::Keys::kWifiSensing];
    if (!section.is<JsonObject>()) {
        return false;
    }
    JsonObject wifi = section.as<JsonObject>();
    if (!wifi[CONFIG::Keys::kEnabled].is<bool>() ||
        !wifi[CONFIG::Keys::kSampleIntervalMs].is<uint32_t>() ||
        !isNumber(wifi[CONFIG::Keys::kVarianceThreshold])) {
        return false;
    }
    return !dependencies.wifiCsi || validateCsiConfig(wifi);
}

bool validateImuDependency(SYSTEM::SpiRamJsonDocument& doc,
                           const AlarmSourceDependencies& dependencies) {
    if (!dependencies.imu) {
        return true;
    }
    JsonVariant section = doc[CONFIG::Keys::kImu];
    if (!section.is<JsonObject>()) {
        return false;
    }
    JsonObject imu = section.as<JsonObject>();
    if (!imu[CONFIG::Keys::kAlarmMonitorEnabled].is<bool>() ||
        !imu[CONFIG::Keys::kOrientationBaselineValid].is<bool>() ||
        !isNumber(imu[CONFIG::Keys::kTiltThresholdDeg]) ||
        !isNumber(imu[CONFIG::Keys::kTiltHysteresisDeg]) ||
        !imu[CONFIG::Keys::kTiltHoldMs].is<uint32_t>() ||
        !imu[CONFIG::Keys::kTiltClearHoldMs].is<uint32_t>() ||
        !isNumber(imu[CONFIG::Keys::kAccelDeltaThresholdG])) {
        return false;
    }
    if (!imu[CONFIG::Keys::kOrientationBaselineValid].as<bool>()) {
        return true;
    }
    JsonVariant baselineVariant = imu[CONFIG::Keys::kOrientationBaseline];
    if (!baselineVariant.is<JsonObject>()) {
        return false;
    }
    JsonObject baseline = baselineVariant.as<JsonObject>();
    return isNumber(baseline[CONFIG::Keys::kX]) &&
           isNumber(baseline[CONFIG::Keys::kY]) &&
           isNumber(baseline[CONFIG::Keys::kZ]);
}

bool validateGpioDependency(SYSTEM::SpiRamJsonDocument& doc,
                            const AlarmSourceDependencies& dependencies) {
    if (!dependencies.gpio) {
        return true;
    }
    JsonVariant section = doc[CONFIG::Keys::kGpio];
    if (!section.is<JsonObject>()) {
        return false;
    }
    JsonObject gpio = section.as<JsonObject>();
    if (!gpio[CONFIG::Keys::kChannels].is<JsonArray>()) {
        return false;
    }

    JsonArray channels = gpio[CONFIG::Keys::kChannels].as<JsonArray>();
    for (JsonObject rule : doc[CONFIG::Keys::kAlarms][CONFIG::Keys::kRules]
                               .as<JsonArray>()) {
        if (!rule[CONFIG::Keys::kEnabled].is<bool>() ||
            !rule[CONFIG::Keys::kEnabled].as<bool>() ||
            !sourceMatches(rule[CONFIG::Keys::kSource],
                           ALARMS::AlarmSource::GpioDigital,
                           "gpio_digital")) {
            continue;
        }

        const char* selector =
            rule[CONFIG::Keys::kGpioId] | static_cast<const char*>(nullptr);
        bool found = false;
        for (JsonObject channel : channels) {
            const char* id =
                channel[CONFIG::Keys::kId] | static_cast<const char*>(nullptr);
            if (selector && id && std::strcmp(selector, id) == 0) {
                found = channel[CONFIG::Keys::kPin].is<int>() &&
                        !channel[CONFIG::Keys::kMode].isNull() &&
                        !channel[CONFIG::Keys::kPull].isNull() &&
                        channel[CONFIG::Keys::kInverted].is<bool>() &&
                        channel[CONFIG::Keys::kDebounceMs].is<int>() &&
                        channel[CONFIG::Keys::kInitialOutput].is<bool>();
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

bool validateAlarmSourceDependencies(SYSTEM::SpiRamJsonDocument& doc) {
    const AlarmSourceDependencies dependencies =
        collectAlarmSourceDependencies(doc);
    return validateWifiDependency(doc, dependencies) &&
           validateImuDependency(doc, dependencies) &&
           validateGpioDependency(doc, dependencies);
}

bool loadGpioIfPresent(SYSTEM::SpiRamJsonDocument& doc) {
    JsonVariant section = doc[CONFIG::Keys::kGpio];
    if (section.isNull()) {
        return true;
    }
    if (!section.is<JsonObject>()) {
        return false;
    }
    JsonObject gpio = section.as<JsonObject>();
    return CONFIG::JSON::loadGpio(gpio);
}

}  // namespace

namespace CONFIG::Serialization {

bool loadConfigSections(SYSTEM::SpiRamJsonDocument& doc) {
    if (!loadAlarmSection(doc) || !validateAlarmSourceDependencies(doc)) {
        return false;
    }
    loadIfObject(doc, Keys::kNotification, JSON::loadNotification);
    loadIfObject(doc, Keys::kWifiSensing, JSON::loadWifiSensing);
    loadIfObject(doc, Keys::kBle, JSON::loadBle);
    loadIfObject(doc, Keys::kShelly, JSON::loadShelly);
    loadIfObject(doc, Keys::kHeartbeat, JSON::loadHeartbeat);
    loadIfObject(doc, Keys::kUdpPusher, JSON::loadUdpPusher);
    loadIfObject(doc, Keys::kAirMouse, JSON::loadAirMouse);
    loadIfObject(doc, Keys::kImu, JSON::loadImu);
    loadIfObject(doc, Keys::kMatrix, JSON::loadMatrix);
    loadIfObject(doc, Keys::kMacros, JSON::loadMacros);
    loadIfObject(doc, Keys::kKeyboard, JSON::loadKeyboard);
    loadIfObject(doc, Keys::kLogging, JSON::loadLogging);
    loadIfObject(doc, Keys::kPower, JSON::loadPower);
    loadIfObject(doc, Keys::kCompensation, JSON::loadCompensation);
    loadIfObject(doc, Keys::kUsbTerminal, JSON::loadUsbTerminal);
    return loadGpioIfPresent(doc);
}

bool loadPsramOnlyConfigSections(SYSTEM::SpiRamJsonDocument& doc) {
    if (!loadAlarmSection(doc) || !validateAlarmSourceDependencies(doc)) {
        return false;
    }
    loadIfObject(doc, Keys::kMatrix, JSON::loadMatrixPsram);
    loadIfObject(doc, Keys::kNotification, JSON::loadNotification);
    loadIfObject(doc, Keys::kShelly, JSON::loadShelly);
    loadIfObject(doc, Keys::kHeartbeat, JSON::loadHeartbeat);
    return loadGpioIfPresent(doc);
}

}  // namespace CONFIG::Serialization
