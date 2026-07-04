/**
 * @file ActivityLogger.h
 * @brief Activity and countdown logging
 */

#pragma once

#include <cstdint>

namespace POWER {

/**
 * Activity logger
 * 
 * Handles rate-limited logging of activity events and sleep countdown.
 * Avoids log spam from frequent API calls.
 */
class ActivityLogger {
public:
    /**
     * Initialize logger
     */
    static void begin();
    
    /**
     * Log activity (rate-limited)
     * 
     * @param source Activity source description
     * @param nowMs Current timestamp
     */
    static void logActivity(const char* source, uint32_t nowMs);
    
    /**
     * Log sleep countdown (periodic)
     * 
     * @param nowMs Current timestamp
     * @param remainingSec Seconds remaining until sleep
     * @param idleSec Seconds idle
     * @param timeoutSec Timeout in seconds
     * @return true if logged (rate limit allows)
     */
    static bool logCountdown(uint32_t nowMs, uint32_t remainingSec, uint32_t idleSec, uint32_t timeoutSec);
    
    /**
     * Log grace period status (once)
     * 
     * @param remainingMs Grace period milliseconds remaining
     * @return true if logged (first call)
     */
    static bool logGracePeriod(uint32_t remainingMs);
    
    /**
     * Log AP client detected (once per connection)
     * 
     * @param stationCount Number of connected stations
     * @return true if logged (state changed)
     */
    static bool logApClient(int stationCount);
    
    /**
     * Reset AP client logged flag (when no clients)
     */
    static void resetApClient();

private:
    ActivityLogger() = delete; // Static-only
    
    static uint32_t _lastCountdownLog;
    static bool _apClientLogged;
    static bool _loggedGrace;
    static uint32_t _lastActivityLogMs;
    static char _lastActivitySource[32];
};

}  // namespace POWER
