/**
 * @file ActivityLogger.cpp
 * @brief Implementation of activity logging
 */

#include "ActivityLogger.h"
#include "../../logging/Logging.h"
#include "../../../config/App.h"
#include <cstring>

namespace POWER {

namespace {

bool isNoisyActivitySource(const char* source) {
    if (!source || !*source) {
        return false;
    }

    return (strncmp(source, "api/", 4) == 0) ||
           (strncmp(source, "rest/", 5) == 0) ||
           (strncmp(source, "ws/", 3) == 0) ||
           (strcmp(source, "ap-station") == 0);
}

uint32_t minLogIntervalForSource(const char* source) {
    if (!source || !*source) {
        return 10000UL;
    }

    if ((strncmp(source, "api/", 4) == 0) || (strncmp(source, "rest/", 5) == 0)) {
        return 60000UL;
    }

    if (strncmp(source, "ws/", 3) == 0) {
        return 60000UL;
    }

    if (strcmp(source, "ap-station") == 0) {
        return 30000UL;
    }

    return 10000UL;
}

}  // namespace

uint32_t ActivityLogger::_lastCountdownLog = 0;
bool ActivityLogger::_apClientLogged = false;
bool ActivityLogger::_loggedGrace = false;
uint32_t ActivityLogger::_lastActivityLogMs = 0;
char ActivityLogger::_lastActivitySource[32] = {0};

void ActivityLogger::begin() {
    _lastCountdownLog = 0;
    _apClientLogged = false;
    _loggedGrace = false;
    _lastActivityLogMs = 0;
    _lastActivitySource[0] = '\0';
}

void ActivityLogger::logActivity(const char* source, uint32_t nowMs) {
    if (!source || !*source) {
        return;
    }

    // Avoid self-spam: HTTP polling, WebSocket keep-alives, and AP presence can
    // legitimately produce very frequent activity updates while the UI is open.
    const bool noisySource = isNoisyActivitySource(source);
    const uint32_t minLogIntervalMs = minLogIntervalForSource(source);

    const bool sourceChanged = (strcmp(_lastActivitySource, source) != 0);
    const bool allowSourceChangedLog = !noisySource;
    const bool firstLoggedActivity = (_lastActivityLogMs == 0) && (_lastActivitySource[0] == '\0');

    if (firstLoggedActivity || (allowSourceChangedLog && sourceChanged) || (nowMs - _lastActivityLogMs >= minLogIntervalMs)) {
        if (noisySource) {
            LOGD("[Power] Activity: %s", source);
        } else {
            LOGI("[Power] Activity: %s", source);
        }
        _lastActivityLogMs = nowMs;
        strncpy(_lastActivitySource, source, sizeof(_lastActivitySource) - 1);
        _lastActivitySource[sizeof(_lastActivitySource) - 1] = '\0';
    }
}

bool ActivityLogger::logCountdown(uint32_t nowMs, uint32_t remainingSec, uint32_t idleSec, uint32_t timeoutSec) {
    if (nowMs - _lastCountdownLog < POWER::COUNTDOWN_LOG_INTERVAL_MS) {
        return false;
    }
    
    LOGI("[Power] Sleep in %lus (idle=%lus/%lus)",
         static_cast<unsigned long>(remainingSec),
         static_cast<unsigned long>(idleSec),
         static_cast<unsigned long>(timeoutSec));
    _lastCountdownLog = nowMs;
    return true;
}

bool ActivityLogger::logGracePeriod(uint32_t remainingMs) {
    if (_loggedGrace) {
        return false;
    }
    
    LOGI("[Power] Grace period active (%lu ms left)", static_cast<unsigned long>(remainingMs));
    _loggedGrace = true;
    return true;
}

bool ActivityLogger::logApClient(int stationCount) {
    if (_apClientLogged) {
        return false;
    }
    
    LOGI("[Power] AP client detected (%d). Resetting inactivity timer.", stationCount);
    _apClientLogged = true;
    return true;
}

void ActivityLogger::resetApClient() {
    _apClientLogged = false;
}

}  // namespace POWER
