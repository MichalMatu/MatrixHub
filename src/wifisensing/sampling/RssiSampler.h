/**
 * @file RssiSampler.h
 * @brief WiFi RSSI sampling with thread-safe ring buffer
 * 
 * Manages continuous RSSI sampling from WiFi connection and stores
 * samples in a fixed-size ring buffer with mutex protection.
 * 
 * Created: 2 Jan 2026 (extracted from WifiSensingService)
 */

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"
#include "esp_attr.h"

namespace WIFISENSING {

// Use local struct to decouple from RTC limits
struct RssiSample {
    int8_t rssi;
    uint8_t _pad[3];
    uint32_t timestampMs;
};

// PSRAM optimized buffer size (1024 samples = ~20s @ 50Hz)
static constexpr uint16_t RSSI_BUFFER_SIZE = 1024;

/**
 * @class RssiSampler
 * @brief Thread-safe RSSI sampler using PSRAM Ring Buffer
 * 
 * Stores high-frequency samples in PSRAM.
 * Uses Mutex instead of Spinlock to safely handle large block copies.
 */
class RssiSampler {
public:
    RssiSampler();
    ~RssiSampler();

    /**
     * @brief Take a new RSSI sample and store in ring buffer
     * Validates RSSI range (-120 to 0 dBm).
     * @return The sample taken (or empty if failed)
     */
    RssiSample takeSample();

    /**
     * @brief Get raw samples (thread-safe copy)
     * @param outBuffer Output buffer
     * @param maxCount Max samples to copy
     * @return Number of samples copied
     */
    uint16_t getSamples(RssiSample* outBuffer, uint16_t maxCount) const;

    /**
     * @brief Initialize buffer (allocate PSRAM)
     */
    bool init();

    /**
     * @brief Deinitialize buffer (free PSRAM)
     */
    void deinit();

    /**
     * @brief Clear the ring buffer
     */
    void clear();

    /**
     * @brief Get current sample count
     */
    uint16_t getCount() const;

    /**
     * @brief Get direct access to buffer for zero-copy analysis
     * @warning Must call acquireLock() before and releaseLock() after!
     */
    const RssiSample* getBufferUnsafe() const;
    uint16_t getHeadUnsafe() const;
    uint16_t getCountUnsafe() const;

    /**
     * @brief Get buffer mutex for zero-copy access
     * @warning Must wrap in SYSTEM::ScopeLock
     */
    SemaphoreHandle_t getMutex() const { return _mutex; }

private:
    // PSRAM Buffer (Dynamic)
    RssiSample* _buffer = nullptr;
    // Managed internally here, no longer dependent on RTC
    
    // Internal state (protected by mutex)
    volatile uint16_t _head = 0;
    volatile uint16_t _count = 0;
    
    SemaphoreHandle_t _mutex = nullptr;
};

}  // namespace WIFISENSING
