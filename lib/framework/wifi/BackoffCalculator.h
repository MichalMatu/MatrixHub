/**
 * @file BackoffCalculator.h
 * @brief Pure function for exponential backoff calculation
 * 
 * Extracted for testability - no dependencies on FreeRTOS or WiFi.
 */

#pragma once

#include <cstdint>

namespace WIFI_UTILS {

/**
 * Calculate exponential backoff delay
 * 
 * @param level Current backoff level (0, 1, 2, ...)
 * @param baseDelayMs Base delay in milliseconds (e.g., 30000 for 30s)
 * @param maxDelayMs Maximum delay cap in milliseconds (e.g., 300000 for 5min)
 * @return Delay in milliseconds: min(baseDelay * 2^level, maxDelay)
 */
inline uint32_t calculateBackoffDelay(uint8_t level, uint32_t baseDelayMs, uint32_t maxDelayMs) {
    if (baseDelayMs == 0) return 0;
    if (maxDelayMs == 0) return baseDelayMs;
    
    // Calculate: baseDelay * 2^level using bit shift
    // Guard against overflow: if level >= 32, result would overflow
    if (level >= 32) {
        return maxDelayMs;
    }
    
    uint32_t delay = baseDelayMs << level;  // baseDelay * 2^level
    
    // Check for overflow (if shift caused wrap-around)
    if (delay < baseDelayMs) {
        return maxDelayMs;
    }
    
    // Cap at maximum
    return (delay > maxDelayMs) ? maxDelayMs : delay;
}

/**
 * Calculate next backoff level (capped to prevent overflow)
 * 
 * @param currentLevel Current level
 * @param maxLevel Maximum level (default: 4)
 * @return Next level, capped at maxLevel
 */
inline uint8_t nextBackoffLevel(uint8_t currentLevel, uint8_t maxLevel = 4) {
    if (currentLevel >= maxLevel) {
        return maxLevel;
    }
    return currentLevel + 1;
}

}  // namespace WIFI_UTILS
