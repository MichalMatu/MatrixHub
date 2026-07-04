/**
 * @file SystemHealth.h
 * @brief System health monitoring facade (delegates to sub-components)
 * 
 * Main entry point for system health monitoring. Coordinates:
 * - HeapTrendTracker (heap sampling history)
 * - WifiHealthTracker (WiFi connection stability)
 * - RuntimeStatsCollector (loop counters, activity stats)
 * - SystemDiagnosticsBuilder (snapshot generation)
 */

#pragma once

#include <Arduino.h>
#include "diagnostics/SystemDiagnosticsBuilder.h"

namespace SYSTEM {

/**
 * SystemHealth - Main facade for health monitoring
 */
class SystemHealth {
public:
    /**
     * Initialize health monitoring
     * Call early in setup()
     */
    static void begin();
    
    /**
     * Update health metrics
     * Call in loop() - handles its own timing
     */
    static void update();
    
    /**
     * Record a slow loop (for tracking main loop performance)
     */
    static void recordSlowLoop();
    
    /**
     * Record various activities (for statistics)
     */
    static void recordHttpRequest();
    static void recordTelegramPoll();
    static void recordSensorRead();
    static void recordFsWrite();
    
    /**
     * WiFi event callbacks - register these with WiFi
     */
    static void onWiFiConnected(const char* ssid, int8_t rssi);
    static void onWiFiDisconnected(uint8_t reason);
    
    /**
     * Get current diagnostics snapshot
     */
    static HEALTH::SystemDiagnostics getDiagnostics();
    
    /**
     * Check if WiFi is healthy
     */
    static bool isWiFiHealthy();
    
    /**
     * Get human-readable status summary
     */
    static void logStatus();
    
    /**
     * Request coordinated WiFi recovery through the owner service.
     * This is intentionally non-blocking and only reports whether the request
     * was accepted/queued, not whether connectivity was restored immediately.
     * 
     * @return true if recovery started or was already unnecessary
     */
    static bool requestWiFiRecovery(const char* reason = "health");
    
    // Configuration
    static constexpr uint32_t kHeapSampleIntervalMs = 300000;  // 5 minutes
    
private:
    // This facade no longer keeps its own RTC persistence blob. Heap history
    // and WiFi health each own the state they really need, which avoids a
    // second, stale mirror that looked recoverable but restored nothing.
    static uint32_t _lastHeapSampleMs;
    static bool _initialized;
};

}  // namespace SYSTEM
