/**
 * @file Hardware.h
 * @brief Power configuration interface - backed by RTC memory
 */

#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "../../config/App.h"
#include "../rtc/types/RtcSystemTypes.h"
#include "PowerWakeController.h"

namespace POWER {

struct InactivityConfig {
    uint32_t timeoutMs;
    uint32_t graceAfterBootMs;
    bool sleepEnabled;
};

class PowerSettings {
public:
    void begin();
    bool applyConfig(const RTC::PowerData& config);
    bool setInactivityTimeout(uint32_t timeoutMs);
    bool setGracePeriod(uint32_t graceMs);
    bool setSleepEnabled(bool enabled);
    bool setWakeTimerEnabled(bool enabled);
    bool setWakeButtonEnabled(bool enabled);
    bool setWakeTouchEnabled(bool enabled);
    bool setWakeInterval(uint32_t intervalMs);
    bool setTimerWakeAwakeMs(uint32_t awakeMs);
    bool setButtonWakeAwakeMs(uint32_t awakeMs);
    bool setWakeTouchGpio(uint8_t gpio);
    bool setWakeTouchThreshold(uint32_t threshold);
    uint32_t getInactivityTimeout() const;
    uint32_t getGracePeriod() const;
    bool getSleepEnabled() const;
    bool getWakeTimerEnabled() const;
    bool getWakeButtonEnabled() const;
    bool getWakeTouchEnabled() const;
    uint32_t getWakeInterval() const;
    uint32_t getTimerWakeAwakeMs() const;
    uint32_t getButtonWakeAwakeMs() const;
    uint8_t getWakeTouchGpio() const;
    uint32_t getWakeTouchThreshold() const;
    InactivityConfig getInactivityConfig() const;
    WakeSourceConfig getWakeSourceConfig() const;
    uint32_t getAwakeWindowForWakeReason(WakeReason reason) const;

private:
    Preferences _prefs;
    bool _prefsReady = false;
    // Note: _cfg removed - now using RTC::store.power directly
};

} // namespace POWER
