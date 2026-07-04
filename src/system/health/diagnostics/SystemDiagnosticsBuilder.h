/**
 * @file SystemDiagnosticsBuilder.h
 * @brief Build comprehensive system diagnostics snapshot
 */

#pragma once

#include <Arduino.h>

namespace SYSTEM {
namespace HEALTH {

struct SystemDiagnostics {
    // Heap
    uint32_t heapFree;
    uint32_t heapMin;
    uint32_t heapLargest;
    uint8_t heapFragmentation;
    
    // WiFi
    uint32_t wifiUptimeMs;
    uint16_t wifiReconnects;
    uint8_t wifiDisconnectReason;
    int8_t wifiRssi;
    bool wifiHealthy;
    
    // Runtime
    uint32_t uptimeMs;
    uint32_t loopCount;
    uint32_t slowLoops;
    uint16_t maintenanceSleepCount;
    bool maintenanceSleepActive;
    
};

class SystemDiagnosticsBuilder {
public:
    /**
     * Build complete diagnostics snapshot
     */
    static SystemDiagnostics build();
    
    /**
     * Log comprehensive status report
     */
    static void logStatus();
};

}  // namespace HEALTH
}  // namespace SYSTEM
