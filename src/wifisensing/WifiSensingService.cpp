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

WifiSensingService::WifiSensingService(ALARMS::AlarmService* alarmService) 
    : _taskRunner(_sampler, 4.0f) {
    
    if (alarmService) {
        _taskRunner.setAlarmService(alarmService);
    }
    
    _ssidMutex = xSemaphoreCreateMutex();
}

WifiSensingService::~WifiSensingService() {
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
    if (_taskRunner.isRunning()) {
        LOGW("Already running");
        return true;
    }
    
    // Initialize dynamic buffer (Lazy Load)
    if (!_sampler.init()) {
        LOGE("Failed to alloc sampler memory");
        return false;
    }
    
    // Clear sampler buffer
    _sampler.clear();
    
    // Update threshold before starting
    _taskRunner.setVarianceThreshold(varianceThreshold);
    
    // Start task (delegates all loop logic to TaskRunner)
    if (!_taskRunner.start(sampleIntervalMs)) {
        LOGE("Failed to start task runner");
        _sampler.deinit();
        return false;
    }
    
    LOGI("Started with %ums interval, buffer=%u samples", 
         sampleIntervalMs, RSSI_BUFFER_SIZE);
    return true;
}

bool WifiSensingService::stop() {
    // The task runner can fail to reach its suspended/deletable state within
    // the shutdown budget. In that case keep the sampler allocation alive:
    // freeing it here would race the still-unwinding worker task.
    if (!_taskRunner.stop()) {
        LOGW("Stop requested but worker still owns sampler resources");
        return false;
    }

    _sampler.clear();
    _sampler.deinit();

    LOGI("Stopped");
    return true;
}

bool WifiSensingService::isRunning() const {
    return _taskRunner.isRunning();
}

bool WifiSensingService::isActive() const {
    return _taskRunner.isRunning() && WiFi.isConnected();
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
