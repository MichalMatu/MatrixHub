/**
 * @file PowerActivityTracker.h
 * @brief Activity tracking coordinator (facade)
 * 
 * Coordinates activity tracking through specialized modules:
 * - ActivityPersistenceManager - RTC memory save/restore
 * - ActivityMonitor - Activity tracking and inactivity detection
 * - ActivityLogger - Rate-limited logging
 */

#pragma once

#include <Arduino.h>
#include "activity/ActivityPersistence.h"
#include "activity/ActivityMonitor.h"
#include "activity/ActivityLogger.h"
#include "PowerWakeController.h"

namespace POWER {

class PowerSleepController;
class PowerSettings;

class PowerActivityTracker {
public:
    void begin(WakeReason wakeReason = WakeReason::Unknown);
    void loopTick(PowerSleepController& sleepController, const PowerSettings& settings);
    void notifyActivity(const char *source = nullptr);
    
    uint32_t getLastActivityMs();
    uint32_t getBootMs();
    uint32_t nowMs();
    uint32_t getWakeAwakeWindowMs(const PowerSettings& settings) const;
    uint32_t getWakeAwakeEtaMs(const PowerSettings& settings);
    bool hasActivitySinceBoot() const;

    // Test hooks
    void setTimeProvider(uint32_t (*provider)());
    void setApStationsProvider(int (*provider)());
    void resetTestHooks();

private:
    uint32_t (*_timeProvider)() = nullptr;
    int (*_apStationsProvider)() = nullptr;
    WakeReason _wakeReason = WakeReason::Unknown;
    bool _activitySeenSinceBoot = false;
    uint32_t _lastPostWakeLogMs = 0;
};

} // namespace POWER
