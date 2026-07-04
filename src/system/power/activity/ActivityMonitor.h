/**
 * @file ActivityMonitor.h
 * @brief Activity tracking and inactivity detection
 */

#pragma once

#include <cstdint>

namespace POWER {

/**
 * Activity monitor
 * 
 * Tracks user/system activity and checks for inactivity timeout.
 * Does not handle logging - only monitoring logic.
 */
class ActivityMonitor {
public:
    /**
     * Initialize monitor
     * 
     * @param bootMs Boot timestamp
     * @param lastActivityMs Initial last activity timestamp
     */
    static void begin(uint32_t bootMs, uint32_t lastActivityMs);
    
    /**
     * Update last activity timestamp
     * 
     * @param nowMs Current timestamp
     */
    static void notifyActivity(uint32_t nowMs);
    
    /**
     * Check if inactivity timeout has been reached
     * 
     * @param nowMs Current timestamp
     * @param timeoutMs Inactivity timeout in milliseconds (0 = disabled)
     * @return true if inactive for >= timeoutMs
     */
    static bool isInactive(uint32_t nowMs, uint32_t timeoutMs);
    
    /**
     * Check if still in grace period after boot
     * 
     * @param nowMs Current timestamp
     * @param gracePeriodMs Grace period duration
     * @return true if still in grace period
     */
    static bool isInGracePeriod(uint32_t nowMs, uint32_t gracePeriodMs);
    
    /**
     * Get idle time in milliseconds
     * 
     * @param nowMs Current timestamp
     * @return Milliseconds since last activity
     */
    static uint32_t getIdleMs(uint32_t nowMs);
    
    /**
     * Get remaining time until sleep
     * 
     * @param nowMs Current timestamp
     * @param timeoutMs Timeout value
     * @return Milliseconds remaining (0 if already exceeded)
     */
    static uint32_t getRemainingMs(uint32_t nowMs, uint32_t timeoutMs);
    
    static uint32_t getLastActivityMs() { return _lastActivityMs; }
    static uint32_t getBootMs() { return _bootMs; }

private:
    ActivityMonitor() = delete; // Static-only
    
    static uint32_t _bootMs;
    static uint32_t _lastActivityMs;
};

}  // namespace POWER
