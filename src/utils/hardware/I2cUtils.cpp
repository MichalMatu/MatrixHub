#include "I2cUtils.h"
#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../../system/logging/Logging.h"
#include "../../system/utils/ScopeLock.h"
#include <cstdlib>

#undef LOG_TAG
#define LOG_TAG "I2cUtils"

namespace UTILS {
namespace HARDWARE {

namespace {

constexpr uint32_t kRecoveryLogIntervalMs = 60000;
uint32_t g_lastRecoveryLogMs = 0;
uint32_t g_suppressedRecoveryLogs = 0;

bool shouldLogRecovery(uint32_t nowMs) {
    if (g_lastRecoveryLogMs == 0 || nowMs - g_lastRecoveryLogMs >= kRecoveryLogIntervalMs) {
        g_lastRecoveryLogMs = nowMs;
        return true;
    }

    if (g_suppressedRecoveryLogs < UINT32_MAX) {
        ++g_suppressedRecoveryLogs;
    }
    return false;
}

uint32_t consumeSuppressedRecoveryLogs() {
    const uint32_t suppressed = g_suppressedRecoveryLogs;
    g_suppressedRecoveryLogs = 0;
    return suppressed;
}

}  // namespace

// Global I2C bus lock for coordinated access
static SemaphoreHandle_t _i2cBusLock = nullptr;

void I2cUtils::createBusLock() {
    if (!_i2cBusLock) {
        _i2cBusLock = xSemaphoreCreateMutex();
        if (!_i2cBusLock) {
            LOGE("Failed to create I2C bus lock");
            std::abort();
        }
    }
}

SemaphoreHandle_t I2cUtils::getBusLock() {
    return _i2cBusLock;
}

void I2cUtils::recoverBus(int sda_pin, int scl_pin) {
    // [FIX] DO NOT acquire mutex here - this is an emergency recovery function.
    // If bus is stuck, the task holding the mutex is likely frozen waiting on I2C.
    // Acquiring mutex here would cause DEADLOCK.
    // This function should be called:
    //   1. Before any I2C transaction starts (safe), or
    //   2. After a timeout when we know no valid transaction is in progress
    
    const bool logRecovery = shouldLogRecovery(millis());
    if (logRecovery) {
        const uint32_t suppressed = consumeSuppressedRecoveryLogs();
        if (suppressed > 0) {
            LOGW("Performing I2C bus recovery on SDA=%d, SCL=%d (suppressed %lu repeats)",
                 sda_pin,
                 scl_pin,
                 static_cast<unsigned long>(suppressed));
        } else {
            LOGW("Performing I2C bus recovery on SDA=%d, SCL=%d", sda_pin, scl_pin);
        }
    }
    
    // Prepare pins
    pinMode(sda_pin, INPUT_PULLUP);
    pinMode(scl_pin, OUTPUT);
    
    // Toggle SCL 9 times to clear any stuck slaves
    for (int i = 0; i < 9; i++) {
        digitalWrite(scl_pin, HIGH);
        delayMicroseconds(10);
        digitalWrite(scl_pin, LOW);
        delayMicroseconds(10);
    }
    
    // Send STOP condition (SCL High)
    digitalWrite(scl_pin, HIGH);
    
    // Release bus by setting SCL to Input
    pinMode(scl_pin, INPUT_PULLUP);
    
    if (logRecovery) {
        LOGI("I2C bus recovery complete");
    }
}

} // namespace HARDWARE
} // namespace UTILS
