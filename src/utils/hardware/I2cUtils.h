#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace UTILS {
namespace HARDWARE {

/**
 * @brief Utilities for hardware bus management
 */
class I2cUtils {
public:
    /**
     * @brief Create the I2C bus lock (call in setup before scheduler)
     */
    static void createBusLock();
    
    /**
     * @brief Get the I2C bus lock for external use
     */
    static SemaphoreHandle_t getBusLock();
    
    /**
     * @brief Attempts to recover a stuck I2C bus by toggling SCL
     * @param sda_pin SDA Pin number
     * @param scl_pin SCL Pin number
     */
    static void recoverBus(int sda_pin, int scl_pin);
};

} // namespace HARDWARE
} // namespace UTILS
