/**
 * @file WifiSensingService.cpp
 * @brief Ultra-thin facade implementation (Phase 3.2)
 * 
 * All logic delegated to:
 *  - WifiSensingTaskRunner: Task lifecycle, loop, motion detection, alarms
 *  - RssiSampler: Data storage
 *  - RssiVarianceAnalyzer: Stats calculation
 */

#include "WifiSensingService.h"
#include "../system/logging/Logging.h"
#include "../system/utils/ScopeLock.h"

#undef LOG_TAG
#define LOG_TAG "WifiSensing"

namespace WIFISENSING {

namespace {

uint32_t normalizeSampleIntervalMs(uint32_t sampleIntervalMs) {
    if (sampleIntervalMs < LIMITS::WIFI_SENSING::MIN_INTERVAL_MS) {
        return LIMITS::WIFI_SENSING::MIN_INTERVAL_MS;
    }
    if (sampleIntervalMs > LIMITS::WIFI_SENSING::MAX_INTERVAL_MS) {
        return LIMITS::WIFI_SENSING::MAX_INTERVAL_MS;
    }
    return sampleIntervalMs;
}

}  // namespace

WifiSensingService::WifiSensingService(ALARMS::AlarmService* alarmService) 
    : _taskRunner(_sampler, 4.0f) {
    
    if (alarmService) {
        _taskRunner.setAlarmService(alarmService);
    }
    
    _ssidMutex = xSemaphoreCreateMutex();
    _lifecycleMutex = xSemaphoreCreateMutex();
    if (!_lifecycleMutex) {
        LOGE("Failed to create lifecycle mutex");
    }
}

WifiSensingService::~WifiSensingService() {
    // Keep the class safe under any RAII owner, not only ServiceRegistry. The
    // worker stores callbacks and an AlarmService pointer, so its task must be
    // fully reaped before this facade starts deleting lifecycle state or member
    // destruction reaches WifiSensingTaskRunner/RssiSampler.
    if (_lifecycleMutex) {
        bool retryLogged = false;
        while (!shutdown()) {
            if (!retryLogged) {
                LOGW("Waiting for WiFi sensing worker before destruction");
                retryLogged = true;
            }
            vTaskDelay(TIMEOUT::TASK_SHUTDOWN_POLL_TICKS);
        }
    } else {
        // Partial construction cannot start through begin(), but keep sampler
        // ownership conservative in case init() was exercised directly.
        while (!_taskRunner.stop()) {
            vTaskDelay(TIMEOUT::TASK_SHUTDOWN_POLL_TICKS);
        }
        (void)_sampler.deinit(portMAX_DELAY);
    }

    if (_lifecycleMutex) {
        vSemaphoreDelete(_lifecycleMutex);
        _lifecycleMutex = nullptr;
    }
    if (_ssidMutex) {
        vSemaphoreDelete(_ssidMutex);
        _ssidMutex = nullptr;
    }
}

/*
// Legacy (single) - replaced by addSensingCallback (multicast)
void WifiSensingService::setSensingCallback(SensingCallback cb) {
    _taskRunner.setCallback(cb);
}
*/

void WifiSensingService::addSensingCallback(SensingCallback cb) {
    _taskRunner.addCallback(cb);
}

bool WifiSensingService::begin(uint32_t sampleIntervalMs, float varianceThreshold) {
    SYSTEM::ScopeLock lifecycleLock(_lifecycleMutex, portMAX_DELAY);
    if (!lifecycleLock.isLocked()) {
        LOGE("Lifecycle mutex unavailable during start");
        return false;
    }

    return beginLocked(sampleIntervalMs, varianceThreshold);
}

bool WifiSensingService::beginLocked(uint32_t sampleIntervalMs,
                                     float varianceThreshold) {
    if (_shuttingDown.load(std::memory_order_acquire)) {
        LOGW("Rejecting start during terminal shutdown");
        return false;
    }

    const uint32_t appliedSampleIntervalMs =
        normalizeSampleIntervalMs(sampleIntervalMs);
    if (_taskRunner.isRunning()) {
        if (!_appliedRuntimeConfig.needsReconcile(
                true,
                appliedSampleIntervalMs,
                varianceThreshold,
                true,
                _taskRunner.hasTaskResources())) {
            LOGW("Already running");
            return true;
        }

        LOGW("Applied RSSI runtime config drifted; restarting worker");
        if (!stopLocked()) {
            return false;
        }
    }

    // A previous stop can time out while the task is still unwinding.  Finish
    // that hand-off before init() resets the sampler; otherwise an
    // immediate settings rollback can race the old worker and then fail to
    // restart even though the worker suspends a moment later.
    if (!_taskRunner.finishPendingStop(TIMEOUT::TASK_SHUTDOWN_TICKS)) {
        if (!_taskRunner.isRunning()) {
            _appliedRuntimeConfig.markDisabled();
        }
        LOGE("Previous task is still stopping");
        return false;
    }

    // At this point no worker is using the previous configuration. Record that
    // transition before allocating/starting the replacement so an init failure
    // cannot leave the applied snapshot claiming that a stale worker is live.
    _appliedRuntimeConfig.markDisabled();
    
    // Initialize dynamic buffer (Lazy Load)
    if (!_sampler.init()) {
        LOGE("Failed to alloc sampler memory");
        return false;
    }
    
    // Update threshold before starting
    _taskRunner.setVarianceThreshold(varianceThreshold);
    
    // Start task (delegates all loop logic to TaskRunner)
    if (!_taskRunner.start(sampleIntervalMs)) {
        LOGE("Failed to start task runner");
        if (!_sampler.deinit()) {
            LOGW("Sampler cleanup deferred after task start failure");
        }
        _appliedRuntimeConfig.markDisabled();
        return false;
    }

    _appliedRuntimeConfig.markEnabled(
        appliedSampleIntervalMs, varianceThreshold);
    
    LOGI("Started with %ums interval, buffer=%u samples", 
         appliedSampleIntervalMs, RSSI_BUFFER_SIZE);
    return true;
}

bool WifiSensingService::stop() {
    SYSTEM::ScopeLock lifecycleLock(_lifecycleMutex, portMAX_DELAY);
    if (!lifecycleLock.isLocked()) {
        LOGE("Lifecycle mutex unavailable during stop");
        return false;
    }

    return stopLocked();
}

bool WifiSensingService::stopLocked() {
    // The task runner can fail to reach its suspended/deletable state within
    // the shutdown budget. In that case keep the sampler allocation alive:
    // freeing it here would race the still-unwinding worker task.
    if (!_taskRunner.stop()) {
        if (!_taskRunner.isRunning()) {
            _appliedRuntimeConfig.markDisabled();
        }
        LOGW("Stop requested but worker still owns sampler resources");
        return false;
    }

    _appliedRuntimeConfig.markDisabled();

    if (!_sampler.deinit()) {
        LOGW("Worker stopped; sampler cleanup will be retried");
        return false;
    }

    LOGI("Stopped");
    return true;
}

bool WifiSensingService::shutdown() {
    _shuttingDown.store(true, std::memory_order_release);
    return stop();
}

bool WifiSensingService::isRunning() const {
    return _taskRunner.isRunning();
}

bool WifiSensingService::needsRuntimeReconcile(
    bool desiredEnabled,
    uint32_t desiredSampleIntervalMs,
    float desiredVarianceThreshold) const {
    if (_shuttingDown.load(std::memory_order_acquire)) {
        return false;
    }
    SYSTEM::ScopeLock lifecycleLock(_lifecycleMutex, pdMS_TO_TICKS(50));
    if (!lifecycleLock.isLocked()) {
        return true;
    }

    return _appliedRuntimeConfig.needsReconcile(
        desiredEnabled,
        normalizeSampleIntervalMs(desiredSampleIntervalMs),
        desiredVarianceThreshold,
        _taskRunner.isRunning(),
        _taskRunner.hasTaskResources() || _sampler.isInitialized());
}

bool WifiSensingService::reconcileRuntime(
    bool desiredEnabled,
    uint32_t desiredSampleIntervalMs,
    float desiredVarianceThreshold) {
    SYSTEM::ScopeLock lifecycleLock(_lifecycleMutex, portMAX_DELAY);
    if (!lifecycleLock.isLocked()) {
        LOGE("Lifecycle mutex unavailable during runtime reconciliation");
        return false;
    }

    if (desiredEnabled) {
        return beginLocked(desiredSampleIntervalMs, desiredVarianceThreshold);
    }
    return stopLocked();
}

WifiAppliedRuntimeConfigSnapshot
WifiSensingService::getAppliedRuntimeConfig() const {
    SYSTEM::ScopeLock lifecycleLock(_lifecycleMutex, pdMS_TO_TICKS(50));
    if (!lifecycleLock.isLocked()) {
        WifiAppliedRuntimeConfigSnapshot unavailable{};
        unavailable.known = false;
        return unavailable;
    }

    return _appliedRuntimeConfig.snapshot();
}

bool WifiSensingService::isActive() const {
    const wifi_mode_t mode = WiFi.getMode();
    const bool apHasClient =
        (mode == WIFI_AP || mode == WIFI_AP_STA) && WiFi.softAPgetStationNum() > 0;
    return _taskRunner.isRunning() && (WiFi.isConnected() || apHasClient);
}

bool WifiSensingService::isMotionDetected() const {
    return _taskRunner.isMotionDetected();
}

RssiStats WifiSensingService::getStats() const {
    // Delegate to RssiVarianceAnalyzer with explicit locking
    SYSTEM::ScopeLock lock(_sampler.getMutex(), pdMS_TO_TICKS(50));
    if (lock.isLocked()) {
        RssiStats stats = RssiVarianceAnalyzer::calculateStats(
            _sampler.getBufferUnsafe(),
            _sampler.getCountUnsafe(),
            _sampler.getHeadUnsafe(),
            RSSI_BUFFER_SIZE
        );
        return stats;
    }
    return {};
}

uint16_t WifiSensingService::getSamples(RssiSample* outBuffer, uint16_t maxCount) const {
    // Delegate to RssiSampler
    return _sampler.getSamples(outBuffer, maxCount);
}

const char* WifiSensingService::getConnectedSSID() const {
    SYSTEM::ScopeLock lock(_ssidMutex);
    
    if (lock.isLocked()) {
        if (WiFi.isConnected()) {
            wifi_config_t conf;
            if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
                strlcpy(_ssidCache, (const char*)conf.sta.ssid, sizeof(_ssidCache));
                return _ssidCache;
            }
        } 
        
        // Fallback: Check AP Mode
        wifi_mode_t mode = WiFi.getMode();
        if ((mode == WIFI_AP || mode == WIFI_AP_STA) && WiFi.softAPgetStationNum() > 0) {
            wifi_config_t conf;
            if (esp_wifi_get_config(WIFI_IF_AP, &conf) == ESP_OK) {
                strlcpy(_ssidCache, (const char*)conf.ap.ssid, sizeof(_ssidCache));
                return _ssidCache;
            }
        }

        _ssidCache[0] = '\0';
    }
    
    return _ssidCache;
}

uint8_t WifiSensingService::getConnectedChannel() const {
    if (WiFi.isConnected()) {
        return WiFi.channel();
    }
    
    // Fallback: Check AP Mode
    wifi_mode_t mode = WiFi.getMode();
     if ((mode == WIFI_AP || mode == WIFI_AP_STA) && WiFi.softAPgetStationNum() > 0) {
        // Primary channel
        wifi_second_chan_t second;
        uint8_t ch;
        esp_wifi_get_channel(&ch, &second);
        return ch;
    }
    
    return 0;
}






}  // namespace WIFISENSING
