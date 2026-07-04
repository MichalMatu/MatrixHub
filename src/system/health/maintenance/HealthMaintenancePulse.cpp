/**
 * @file HealthMaintenancePulse.cpp
 * @brief Implementation of periodic health maintenance checks
 */

#include "HealthMaintenancePulse.h"

#include "../heap/HeapMonitor.h"
#include "../../logging/Logging.h"
#include "../../restart/RuntimeRestart.h"

#undef LOG_TAG
#define LOG_TAG "HealthMaint"

namespace SYSTEM {

uint32_t HealthMaintenancePulse::_lastHeapProbePollMs = 0;
uint32_t HealthMaintenancePulse::_lastHeapCheckMs = 0;
uint8_t HealthMaintenancePulse::_criticalHeapFaultCount = 0;
bool HealthMaintenancePulse::_initialized = false;

namespace {

constexpr uint8_t kCriticalHeapRestartConsecutiveChecks = 2;

void restartForLowMemory(uint32_t freeHeap, uint8_t faultCount) {
    char message[96];
    snprintf(message,
             sizeof(message),
             "CRITICAL HEAP persisted for %u checks: %u bytes < threshold. Restarting!",
             faultCount,
             freeHeap);

    // Low-memory recovery shares the same direct restart policy as the main
    // loop watchdog fault: keep the marker, skip graceful teardown, restart.
    SYSTEM::RESTART::emergencyRestart(ShutdownReason::LOW_MEMORY, LOG_TAG, message);
}

}  // namespace

void HealthMaintenancePulse::begin() {
    if (_initialized) {
        return;
    }

    const uint32_t now = millis();
    _lastHeapProbePollMs = now;
    _lastHeapCheckMs = now;
    _criticalHeapFaultCount = 0;
    _initialized = true;

    LOGD("Health maintenance pulse initialized");
}

void HealthMaintenancePulse::update() {
    if (!_initialized) {
        begin();
    }

    const uint32_t now = millis();

    if (now - _lastHeapProbePollMs > kHeapProbePollMs) {
        HeapMonitor::instance().poll();
        _lastHeapProbePollMs = now;
    }

    if (now - _lastHeapCheckMs > kHeapCheckMs) {
        HeapMonitor::instance().check();

        const uint32_t freeHeap = HeapMonitor::instance().getFreeHeap();
        if (freeHeap < kCriticalHeapRestartBytes) {
            // A single low sample can happen during transient bursts. Only force
            // a reboot when the critical condition survives consecutive checks.
            if (_criticalHeapFaultCount < UINT8_MAX) {
                ++_criticalHeapFaultCount;
            }
            LOGW("CRITICAL HEAP sample %u/%u: %u bytes < threshold",
                 _criticalHeapFaultCount,
                 kCriticalHeapRestartConsecutiveChecks,
                 freeHeap);
            if (_criticalHeapFaultCount >= kCriticalHeapRestartConsecutiveChecks) {
                restartForLowMemory(freeHeap, _criticalHeapFaultCount);
            }
        } else {
            _criticalHeapFaultCount = 0;
        }

        HeapMonitor::instance().checkHygieneConditions();

        _lastHeapCheckMs = now;
    }
}

void HealthMaintenancePulse::reset() {
    const uint32_t now = millis();
    _lastHeapProbePollMs = now;
    _lastHeapCheckMs = now;
    _criticalHeapFaultCount = 0;

    // WiFi reconnect/backoff ownership lives in WiFiSettingsService now, so
    // maintenance reset only needs to restore the heap-related timers here.
    LOGD("Health maintenance pulse timers reset");
}

}  // namespace SYSTEM
