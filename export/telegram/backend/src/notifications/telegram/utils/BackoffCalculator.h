#pragma once

#include <stdint.h>
#include "../../../config/App.h"

namespace TELEGRAM {

/**
 * @brief Calculates exponential backoff for network retries
 */
class BackoffCalculator {
public:
    /**
     * @brief Calculate backoff delay based on consecutive errors
     * 
     * @param consecutiveErrors Number of consecutive errors
     * @param baseIntervalMs The base polling interval in ms (used as multiplier)
     * @return uint32_t The calculated backoff delay to add, in milliseconds
     */
    static uint32_t calculateMs(uint32_t consecutiveErrors, uint32_t baseIntervalMs) {
        if (consecutiveErrors == 0) return 0;
        
        uint32_t exp = consecutiveErrors < APP::NOTIFICATIONS::TELEGRAM_MAX_BACKOFF_EXPONENT 
                     ? consecutiveErrors : APP::NOTIFICATIONS::TELEGRAM_MAX_BACKOFF_EXPONENT;
        
        uint32_t backoffMs = baseIntervalMs * ((1u << exp) - 1);
        
        if (backoffMs > APP::NOTIFICATIONS::TELEGRAM_MAX_BACKOFF_MS) {
            backoffMs = APP::NOTIFICATIONS::TELEGRAM_MAX_BACKOFF_MS;
        }
        
        return backoffMs;
    }
};

} // namespace TELEGRAM
