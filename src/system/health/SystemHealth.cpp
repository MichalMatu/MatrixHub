/**
 * @file SystemHealth.cpp
 * @brief System health monitoring facade (delegates to sub-components)
 */

#include "SystemHealth.h"
#include "../../config/System.h"
#include "../health/heap/HeapTrendTracker.h"
#include "../health/wifi/WifiHealthTracker.h"
#include "../health/runtime/RuntimeStatsCollector.h"
#include "../health/diagnostics/SystemDiagnosticsBuilder.h"
#include "heap/HeapMonitor.h"
#include "../logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "SysHealth"

namespace SYSTEM {

using namespace HEALTH;

// Static member initialization
uint32_t SystemHealth::_lastHeapSampleMs = 0;
bool SystemHealth::_initialized = false;

void SystemHealth::begin() {
    if (_initialized) {
        return;
    }
    
    // Initialize all sub-components. The facade itself stays stateless now:
    // heap history owns its own retained samples and WiFi health is derived
    // from the live monitor, so there is no extra duplicate RTC mirror here.
    RuntimeStatsCollector::begin();
    HeapTrendTracker::begin();
    WifiHealthTracker::begin();

    _lastHeapSampleMs = millis();
    _initialized = true;
    
    LOGD("System health monitoring started");
    LOGD("  Initial heap: %u bytes, largest block: %u bytes",
         HeapMonitor::instance().getFreeHeap(), HeapMonitor::instance().getLargestFreeBlock());
         
    // Show initial health report
    logStatus();
}

void SystemHealth::update() {
    if (!_initialized) {
        begin();
    }

    uint32_t now = millis();

    // Update runtime stats
    RuntimeStatsCollector::updateLoop();

    // Update WiFi health tracker
    WifiHealthTracker::update();

    // Update heap min/max
    HeapTrendTracker::updateMinMax();

    // Periodic heap sampling (skip during startup stabilization)
    if (now > SYS_HEALTH::HEAP_SAMPLE_STARTUP_DELAY_MS && now - _lastHeapSampleMs >= kHeapSampleIntervalMs) {
        HeapTrendTracker::sampleHeap();
        _lastHeapSampleMs = now;
        
        // Log status periodically (every 6 samples = 30 minutes)
        const auto& history = HeapTrendTracker::getHistory();
        if (history.count % 6 == 0) {
            logStatus();
        }
    }
}

void SystemHealth::recordSlowLoop() {
    RuntimeStatsCollector::recordSlowLoop();
}

void SystemHealth::recordHttpRequest() {
    RuntimeStatsCollector::recordHttpRequest();
}

void SystemHealth::recordTelegramPoll() {
    RuntimeStatsCollector::recordTelegramPoll();
}

void SystemHealth::recordSensorRead() {
    RuntimeStatsCollector::recordSensorRead();
}

void SystemHealth::recordFsWrite() {
    RuntimeStatsCollector::recordFsWrite();
}

void SystemHealth::onWiFiConnected(const char* ssid, int8_t rssi) {
    WifiHealthTracker::onConnected(ssid, rssi);
}

void SystemHealth::onWiFiDisconnected(uint8_t reason) {
    WifiHealthTracker::onDisconnected(reason);
}

bool SystemHealth::isWiFiHealthy() {
    return WifiHealthTracker::isHealthy(RuntimeStatsCollector::getUptimeMs());
}

bool SystemHealth::requestWiFiRecovery(const char* reason) {
    return WifiHealthTracker::requestRecovery(reason);
}

HEALTH::SystemDiagnostics SystemHealth::getDiagnostics() {
    return SystemDiagnosticsBuilder::build();
}

void SystemHealth::logStatus() {
    SystemDiagnosticsBuilder::logStatus();
}

}  // namespace SYSTEM
