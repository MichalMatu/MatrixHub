#pragma once

#include <Arduino.h>
#include <esp_sleep.h>
#include "../../config/App.h"
#include "../rtc/types/RtcSystemTypes.h"
#include "PowerSettings.h"
#include "PowerWakeController.h"
#include "PowerSleepController.h"
#include "PowerActivityTracker.h"

namespace POWER {

class PowerManager {
public:
    PowerManager() = default;
    PowerManager(const PowerManager&) = delete;
    PowerManager& operator=(const PowerManager&) = delete;

    void begin();
    
    // Configuration
    bool setInactivityTimeout(uint32_t timeoutMs);
    bool setGracePeriod(uint32_t graceMs);
    uint32_t getInactivityTimeout() const;
    uint32_t getGracePeriod() const;
    
    bool setSleepEnabled(bool enabled);
    bool getSleepEnabled() const;
    bool applyConfig(const RTC::PowerData& config);
    
    // Runtime
    void notifyActivity(const char *source = nullptr);
    void loopTick();
    void requestSleep(const char *reason, uint32_t delayMs = 0);
    bool isSleepRequested();
    WakeReason wakeReason();
    esp_sleep_wakeup_cause_t getWakeupCauseRaw();
    uint64_t getGpioWakeupMask();
    uint64_t getExt1WakeupMask();
    
    // Status
    InactivityConfig inactivityConfig();
    RTC::PowerData powerConfig();
    uint32_t lastActivityMs();
    uint32_t bootMs();
    uint32_t wakeIntervalMs();
    uint32_t sleepEtaMs();
    uint32_t wakeAwakeWindowMs();
    uint32_t wakeAwakeEtaMs();

    void setPreSleepHook(void (*hook)());

    // Test hooks (no effect in production if unset)
    void setTimeProvider(uint32_t (*provider)());
    void setApStationsProvider(int (*provider)());
    void setSleepCallback(void (*callback)(const char*));
    void setConfigureWakeSourcesCallback(void (*callback)());
    void resetTestHooks();

    void setWakeInterval(uint32_t intervalMs);
    const char* getSleepReason();

private:
    PowerWakeController _wakeController;
    PowerSleepController _sleepController;
    PowerActivityTracker _activityTracker;
    PowerSettings _settings;

    uint32_t nowMs();
};

}  // namespace POWER
