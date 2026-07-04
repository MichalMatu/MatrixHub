/**
 * @file RuntimeStatsCollector.h
 * @brief Collect runtime statistics (loop counters, activity counts)
 */

#pragma once

#include <Arduino.h>

namespace SYSTEM {
namespace HEALTH {

struct RuntimeStats {
    uint32_t bootMs;
    uint32_t lastLoopMs;
    uint32_t loopCount;
    uint32_t slowLoopCount;        // Loops taking > 100ms
    uint16_t httpRequestCount;
    uint16_t telegramPollCount;
    uint16_t sensorReadCount;
    uint16_t fsWriteCount;
};

class RuntimeStatsCollector {
public:
    /**
     * Initialize runtime stats
     */
    static void begin();
    
    /**
     * Update loop tracking (call in main loop)
     */
    static void updateLoop();
    
    /**
     * Record various activities
     */
    static void recordSlowLoop();
    static void recordHttpRequest();
    static void recordTelegramPoll();
    static void recordSensorRead();
    static void recordFsWrite();
    
    /**
     * Get current runtime stats
     */
    static const RuntimeStats& getStats();
    
    /**
     * Get system uptime in milliseconds
     */
    static uint32_t getUptimeMs();
    
private:
    static RuntimeStats _stats;
};

}  // namespace HEALTH
}  // namespace SYSTEM
