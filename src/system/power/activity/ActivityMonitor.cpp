/**
 * @file ActivityMonitor.cpp
 * @brief Implementation of activity monitoring
 */

#include "ActivityMonitor.h"

namespace POWER {

uint32_t ActivityMonitor::_bootMs = 0;
uint32_t ActivityMonitor::_lastActivityMs = 0;

void ActivityMonitor::begin(uint32_t bootMs, uint32_t lastActivityMs) {
    _bootMs = bootMs;
    _lastActivityMs = lastActivityMs;
}

void ActivityMonitor::notifyActivity(uint32_t nowMs) {
    _lastActivityMs = nowMs;
}

bool ActivityMonitor::isInactive(uint32_t nowMs, uint32_t timeoutMs) {
    if (timeoutMs == 0) {
        return false;  // Timeout disabled
    }
    return getIdleMs(nowMs) >= timeoutMs;
}

bool ActivityMonitor::isInGracePeriod(uint32_t nowMs, uint32_t gracePeriodMs) {
    return (nowMs - _bootMs) < gracePeriodMs;
}

uint32_t ActivityMonitor::getIdleMs(uint32_t nowMs) {
    return nowMs - _lastActivityMs;
}

uint32_t ActivityMonitor::getRemainingMs(uint32_t nowMs, uint32_t timeoutMs) {
    uint32_t idleMs = getIdleMs(nowMs);
    if (idleMs >= timeoutMs) {
        return 0;
    }
    return timeoutMs - idleMs;
}

}  // namespace POWER
