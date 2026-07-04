/**
 * @file PowerSettings.cpp
 * @brief Power configuration - now backed by RTC memory
 * 
 * On cold boot: loads from NVS Preferences → RTC
 * On warm boot: uses RTC directly (no NVS access)
 * On change: writes to RTC + NVS (backup)
 */

#include "PowerSettings.h"
#include "../rtc/RtcConfig.h"
#include "../logging/Logging.h"

namespace POWER {

namespace {
    constexpr const char *kNamespace = "power_cfg";
    constexpr const char *kKeyInact = "inact_ms";
    constexpr const char *kKeyGrace = "grace_ms";
    constexpr const char *kKeySleepEnabled = "sleep_en";
    constexpr const char *kKeyWakeTimerEnabled = "wake_tmr";
    constexpr const char *kKeyWakeButtonEnabled = "wake_btn";
    constexpr const char *kKeyWakeTouchEnabled = "wake_tch";
    constexpr const char *kKeyWakeInterval = "wake_ms";
    constexpr const char *kKeyTimerAwake = "tmr_awake";
    constexpr const char *kKeyButtonAwake = "btn_awake";
    constexpr const char *kKeyTouchGpio = "touch_gpio";
    constexpr const char *kKeyTouchThreshold = "touch_thr";
}

void PowerSettings::begin() {
    // Data is already in RTC::store.power (loaded by RtcConfigLoader)
    // Open prefs for future writes only
    _prefsReady = _prefs.begin(kNamespace, false);
    if (!_prefsReady) {
        LOGE("prefs begin failed");
    }
}

bool PowerSettings::applyConfig(const RTC::PowerData& config) {
    const bool rtcUpdated = RTC::updateConfig([&config](RTC::ConfigStore& store) {
        store.power = config;
    });
    if (!rtcUpdated) {
        LOGE("Failed to update RTC power config");
        return false;
    }
    if (!_prefsReady) {
        LOGE("Preferences unavailable while saving power config");
        return false;
    }

    bool persisted = true;
    persisted &= _prefs.putUInt(kKeyInact, config.inactivityTimeoutMs) > 0;
    persisted &= _prefs.putUInt(kKeyGrace, config.graceAfterBootMs) > 0;
    persisted &= _prefs.putBool(kKeySleepEnabled, config.sleepEnabled) > 0;
    persisted &= _prefs.putBool(kKeyWakeTimerEnabled, config.wakeTimerEnabled) > 0;
    persisted &= _prefs.putBool(kKeyWakeButtonEnabled, config.wakeButtonEnabled) > 0;
    persisted &= _prefs.putBool(kKeyWakeTouchEnabled, config.wakeTouchEnabled) > 0;
    persisted &= _prefs.putUInt(kKeyWakeInterval, config.wakeIntervalMs) > 0;
    persisted &= _prefs.putUInt(kKeyTimerAwake, config.timerWakeAwakeMs) > 0;
    persisted &= _prefs.putUInt(kKeyButtonAwake, config.buttonWakeAwakeMs) > 0;
    persisted &= _prefs.putUChar(kKeyTouchGpio, config.wakeTouchGpio) > 0;
    persisted &= _prefs.putUInt(kKeyTouchThreshold, config.wakeTouchThreshold) > 0;
    if (!persisted) {
        LOGE("Failed to persist power config");
        return false;
    }

    LOGI("Saved power config: sleep=%s timer=%s button=%s touch=%s wake=%lu ms",
         config.sleepEnabled ? "true" : "false",
         config.wakeTimerEnabled ? "true" : "false",
         config.wakeButtonEnabled ? "true" : "false",
         config.wakeTouchEnabled ? "true" : "false",
         static_cast<unsigned long>(config.wakeIntervalMs));
    return true;
}

bool PowerSettings::setInactivityTimeout(uint32_t timeoutMs) {
    RTC::PowerData next = RTC::getConfig().power;
    next.inactivityTimeoutMs = timeoutMs;
    return applyConfig(next);
}

bool PowerSettings::setGracePeriod(uint32_t graceMs) {
    RTC::PowerData next = RTC::getConfig().power;
    next.graceAfterBootMs = graceMs;
    return applyConfig(next);
}

uint32_t PowerSettings::getInactivityTimeout() const {
    return RTC::getConfig().power.inactivityTimeoutMs;
}

uint32_t PowerSettings::getGracePeriod() const {
    return RTC::getConfig().power.graceAfterBootMs;
}

bool PowerSettings::getSleepEnabled() const {
    return RTC::getConfig().power.sleepEnabled;
}

bool PowerSettings::setSleepEnabled(bool enabled) {
    RTC::PowerData next = RTC::getConfig().power;
    next.sleepEnabled = enabled;
    return applyConfig(next);
}

bool PowerSettings::setWakeTimerEnabled(bool enabled) {
    RTC::PowerData next = RTC::getConfig().power;
    next.wakeTimerEnabled = enabled;
    return applyConfig(next);
}

bool PowerSettings::setWakeButtonEnabled(bool enabled) {
    RTC::PowerData next = RTC::getConfig().power;
    next.wakeButtonEnabled = enabled;
    return applyConfig(next);
}

bool PowerSettings::setWakeTouchEnabled(bool enabled) {
    RTC::PowerData next = RTC::getConfig().power;
    next.wakeTouchEnabled = enabled;
    return applyConfig(next);
}

bool PowerSettings::setWakeInterval(uint32_t intervalMs) {
    RTC::PowerData next = RTC::getConfig().power;
    next.wakeIntervalMs = intervalMs;
    return applyConfig(next);
}

bool PowerSettings::setTimerWakeAwakeMs(uint32_t awakeMs) {
    RTC::PowerData next = RTC::getConfig().power;
    next.timerWakeAwakeMs = awakeMs;
    return applyConfig(next);
}

bool PowerSettings::setButtonWakeAwakeMs(uint32_t awakeMs) {
    RTC::PowerData next = RTC::getConfig().power;
    next.buttonWakeAwakeMs = awakeMs;
    return applyConfig(next);
}

bool PowerSettings::setWakeTouchGpio(uint8_t gpio) {
    RTC::PowerData next = RTC::getConfig().power;
    next.wakeTouchGpio = gpio;
    return applyConfig(next);
}

bool PowerSettings::setWakeTouchThreshold(uint32_t threshold) {
    RTC::PowerData next = RTC::getConfig().power;
    next.wakeTouchThreshold = threshold;
    return applyConfig(next);
}

bool PowerSettings::getWakeTimerEnabled() const {
    return RTC::getConfig().power.wakeTimerEnabled;
}

bool PowerSettings::getWakeButtonEnabled() const {
    return RTC::getConfig().power.wakeButtonEnabled;
}

bool PowerSettings::getWakeTouchEnabled() const {
    return RTC::getConfig().power.wakeTouchEnabled;
}

uint32_t PowerSettings::getWakeInterval() const {
    return RTC::getConfig().power.wakeIntervalMs;
}

uint32_t PowerSettings::getTimerWakeAwakeMs() const {
    return RTC::getConfig().power.timerWakeAwakeMs;
}

uint32_t PowerSettings::getButtonWakeAwakeMs() const {
    return RTC::getConfig().power.buttonWakeAwakeMs;
}

uint8_t PowerSettings::getWakeTouchGpio() const {
    return RTC::getConfig().power.wakeTouchGpio;
}

uint32_t PowerSettings::getWakeTouchThreshold() const {
    return RTC::getConfig().power.wakeTouchThreshold;
}

InactivityConfig PowerSettings::getInactivityConfig() const {
    return InactivityConfig{
        RTC::getConfig().power.inactivityTimeoutMs,
        RTC::getConfig().power.graceAfterBootMs,
        RTC::getConfig().power.sleepEnabled
    };
}

WakeSourceConfig PowerSettings::getWakeSourceConfig() const {
    const auto& power = RTC::getConfig().power;
    return WakeSourceConfig{
        power.wakeIntervalMs,
        power.wakeTouchGpio,
        power.wakeTouchThreshold,
        power.wakeTimerEnabled,
        power.wakeButtonEnabled,
        power.wakeTouchEnabled
    };
}

uint32_t PowerSettings::getAwakeWindowForWakeReason(WakeReason reason) const {
    const auto& power = RTC::getConfig().power;
    switch (reason) {
        case WakeReason::Timer:
            return power.timerWakeAwakeMs;
        case WakeReason::Button:
        case WakeReason::Touch:
            return power.buttonWakeAwakeMs;
        default:
            return 0;
    }
}

} // namespace POWER
