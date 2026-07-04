/**
 * @file PowerActivityTracker.cpp
 * @brief Implementation of activity tracking coordinator (facade)
 */

#include "PowerActivityTracker.h"
#include "PowerSettings.h"
#include "PowerSleepController.h"
#include "../logging/Logging.h"
#include "../../config/Hardware.h"
#include <WiFi.h>

namespace POWER {


namespace {
const char* wakeReasonToSleepReason(WakeReason reason) {
    switch (reason) {
        case WakeReason::Timer:
            return "timer-wake-window";
        case WakeReason::Touch:
            return "touch-wake-window";
        case WakeReason::Button:
            return "button-wake-window";
        default:
            return "post-wake-window";
    }
}

const char* wakeReasonToString(WakeReason reason) {
    switch (reason) {
        case WakeReason::Timer:
            return "timer";
        case WakeReason::Touch:
            return "touch";
        case WakeReason::Button:
            return "button";
        case WakeReason::Other:
            return "other";
        default:
            return "unknown";
    }
}
}  // namespace

uint32_t PowerActivityTracker::nowMs() {
    if (_timeProvider) {
        return _timeProvider();
    }
    return millis();
}

void PowerActivityTracker::begin(WakeReason wakeReason) {
    uint32_t now = nowMs();
    uint32_t lastActivityMs = 0;
    uint32_t bootMs = 0;
    _wakeReason = wakeReason;
    _activitySeenSinceBoot = false;
    _lastPostWakeLogMs = 0;
    
    // Try to restore from RTC memory
    if (!ActivityPersistenceManager::tryRestore(lastActivityMs, bootMs)) {
        // Cold boot or corruption - initialize fresh
        bootMs = now;
        lastActivityMs = now;
        LOGI("[Power] Activity tracking initialized (cold boot)");
    } else {
        // Restored from RTC - adjust for new millis() base
        // Note: millis() resets to 0 after reboot, but RTC data persists
        // We treat restored activity as if it just happened (reset to boot time)
        bootMs = now;
        lastActivityMs = now;
        LOGI("[Power] Activity tracking resumed after reboot");
    }
    
    // Initialize modules
    ActivityMonitor::begin(bootMs, lastActivityMs);
    ActivityLogger::begin();
    
    // Save initial state to RTC
    ActivityPersistenceManager::save(lastActivityMs, bootMs);
}

uint32_t PowerActivityTracker::getLastActivityMs() {
    return ActivityMonitor::getLastActivityMs();
}

uint32_t PowerActivityTracker::getBootMs() {
    return ActivityMonitor::getBootMs();
}

void PowerActivityTracker::notifyActivity(const char *source) {
    uint32_t now = nowMs();
    _activitySeenSinceBoot = true;
    ActivityMonitor::notifyActivity(now);
    ActivityLogger::logActivity(source, now);
}

uint32_t PowerActivityTracker::getWakeAwakeWindowMs(const PowerSettings& settings) const {
    if (!settings.getSleepEnabled() || _activitySeenSinceBoot) {
        return 0;
    }
    return settings.getAwakeWindowForWakeReason(_wakeReason);
}

uint32_t PowerActivityTracker::getWakeAwakeEtaMs(const PowerSettings& settings) {
    const uint32_t windowMs = getWakeAwakeWindowMs(settings);
    if (windowMs == 0) {
        return 0;
    }
    const uint32_t elapsedMs = nowMs() - ActivityMonitor::getBootMs();
    if (elapsedMs >= windowMs) {
        return 0;
    }
    return windowMs - elapsedMs;
}

bool PowerActivityTracker::hasActivitySinceBoot() const {
    return _activitySeenSinceBoot;
}

void PowerActivityTracker::loopTick(PowerSleepController& sleepController, const PowerSettings& settings) {
    // If sleep is already requested, we don't need to check for inactivity
    if (sleepController.isSleepRequested()) {
        return;
    }

    uint32_t now = nowMs();

    // Treat AP station presence as activity to avoid sleeping while user connects
    int apStations = _apStationsProvider ? _apStationsProvider() : WiFi.softAPgetStationNum();
    if (apStations > 0) {
        ActivityLogger::logApClient(apStations);
        // AP presence is a sustained state, not a discrete user action.
        // Refresh the inactivity timer without emitting per-tick activity logs.
        _activitySeenSinceBoot = true;
        ActivityMonitor::notifyActivity(now);
        return;  // do not sleep while AP clients are connected
    } else {
        ActivityLogger::resetApClient();
    }

    // Check if auto-sleep is enabled
    if (!settings.getSleepEnabled()) {
        return;
    }

    const uint32_t wakeWindowMs = getWakeAwakeWindowMs(settings);
    if (wakeWindowMs > 0) {
        const uint32_t elapsedMs = now - ActivityMonitor::getBootMs();
        if (elapsedMs >= wakeWindowMs) {
            LOGI("[Power] Post-wake window elapsed -> sleep (wake=%s window=%lus)",
                 wakeReasonToString(_wakeReason),
                 static_cast<unsigned long>(wakeWindowMs / 1000UL));
            sleepController.requestSleep(wakeReasonToSleepReason(_wakeReason));
            return;
        }

        if (_lastPostWakeLogMs == 0 || now - _lastPostWakeLogMs >= POWER::POST_WAKE_LOG_INTERVAL_MS) {
            _lastPostWakeLogMs = now;
            const uint32_t remainingMs = wakeWindowMs - elapsedMs;
            LOGI("[Power] Post-wake window active (wake=%s remaining=%lus)",
                 wakeReasonToString(_wakeReason),
                 static_cast<unsigned long>(remainingMs / 1000UL));
        }
        return;
    }

    uint32_t gracePeriod = settings.getGracePeriod();
    if (ActivityMonitor::isInGracePeriod(now, gracePeriod)) {
        uint32_t remainingMs = gracePeriod - (now - ActivityMonitor::getBootMs());
        ActivityLogger::logGracePeriod(remainingMs);
        return;
    }

    uint32_t timeoutMs = settings.getInactivityTimeout();
    if (timeoutMs == 0) {
        return;
    }

    if (ActivityMonitor::isInactive(now, timeoutMs)) {
        uint32_t idleMs = ActivityMonitor::getIdleMs(now);
        LOGI("[Power] Inactivity timeout -> sleep (idle=%lus tmo=%lus)",
             static_cast<unsigned long>(idleMs / 1000UL),
             static_cast<unsigned long>(timeoutMs / 1000UL));
        sleepController.requestSleep("inactivity");
        return;
    }

    // Periodic countdown log
    uint32_t remainingMs = ActivityMonitor::getRemainingMs(now, timeoutMs);
    uint32_t idleMs = ActivityMonitor::getIdleMs(now);
    ActivityLogger::logCountdown(now, remainingMs / 1000UL, idleMs / 1000UL, timeoutMs / 1000UL);
}

void PowerActivityTracker::setTimeProvider(uint32_t (*provider)()) {
    _timeProvider = provider;
}

void PowerActivityTracker::setApStationsProvider(int (*provider)()) {
    _apStationsProvider = provider;
}

void PowerActivityTracker::resetTestHooks() {
    _timeProvider = nullptr;
    _apStationsProvider = nullptr;
    _wakeReason = WakeReason::Unknown;
    _activitySeenSinceBoot = false;
    _lastPostWakeLogMs = 0;
}

} // namespace POWER
