/**
 * @file WifiSensingService.h
 * @brief Ultra-thin facade for WiFi-based motion detection (Phase 3.2)
 * 
 * Delegates to WifiSensingTaskRunner (task lifecycle) + RssiSampler (data storage).
 */

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "sampling/RssiSampler.h"
#include "analysis/RssiVarianceAnalyzer.h"
#include "core/WifiAppliedRuntimeConfig.h"
#include "core/WifiSensingTaskRunner.h"
#include <atomic>

namespace ALARMS { class AlarmService; }

namespace WIFISENSING {

class WifiSensingService {
public:
    using SensingCallback = WifiSensingTaskRunner::SensingCallback;

    WifiSensingService(ALARMS::AlarmService* alarmService = nullptr);
    ~WifiSensingService();
    
    // Lifecycle (delegates to TaskRunner)
    bool begin(uint32_t sampleIntervalMs = 1000, float varianceThreshold = 4.0f);
    bool stop();
    bool shutdown();
    bool isRunning() const;
    bool needsRuntimeReconcile(bool desiredEnabled,
                               uint32_t desiredSampleIntervalMs,
                               float desiredVarianceThreshold) const;
    bool reconcileRuntime(bool desiredEnabled,
                          uint32_t desiredSampleIntervalMs,
                          float desiredVarianceThreshold);
    WifiAppliedRuntimeConfigSnapshot getAppliedRuntimeConfig() const;
    bool isActive() const;

    void addSensingCallback(SensingCallback cb);

    const char* getConnectedSSID() const;
    uint8_t getConnectedChannel() const;
    uint16_t getSamples(RssiSample* outBuffer, uint16_t maxCount) const;
    
    RssiSampler _sampler;
    WifiSensingTaskRunner _taskRunner;
    mutable char _ssidCache[33] = {0};
    mutable SemaphoreHandle_t _ssidMutex;
    mutable SemaphoreHandle_t _lifecycleMutex = nullptr;
    std::atomic<bool> _shuttingDown{false};

    // Get current stats (thread-safe)
    RssiStats getStats() const;

    // Check motion detection status
    bool isMotionDetected() const;

private:
    bool beginLocked(uint32_t sampleIntervalMs, float varianceThreshold);
    bool stopLocked();
    WifiAppliedRuntimeConfig _appliedRuntimeConfig;
};

}  // namespace WIFISENSING
