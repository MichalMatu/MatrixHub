/**
 * @file RssiSampler.cpp
 * @brief Implementation of WiFi RSSI sampling with thread-safe ring buffer
 */

#include "RssiSampler.h"
#include "../../system/logging/Logging.h"
#include "../../config/App.h"
#include <esp_wifi.h>  // For AP mode station list access
#include "../../system/utils/ScopeLock.h"

#undef LOG_TAG
#define LOG_TAG "RssiSampler"

namespace WIFISENSING {

// Allocate large buffer in PSRAM (Dynamic)
// This avoids consuming valuable internal RAM
// EXT_RAM_BSS_ATTR RssiSample _psramBuffer[RSSI_BUFFER_SIZE]; // Removed static

RssiSampler::RssiSampler() {
    _mutex = xSemaphoreCreateMutex();
    if (!_mutex) {
        LOGE("Failed to create mutex!");
    }
}

bool RssiSampler::init() {
    if (_buffer) return true; // Already init

    _buffer = (RssiSample*)heap_caps_malloc(RSSI_BUFFER_SIZE * sizeof(RssiSample), MALLOC_CAP_SPIRAM);
    if (!_buffer) {
        LOGE("Failed to allocate RssiSampler buffer in PSRAM!");
        return false;
    }
    clear();
    LOGI("RssiSampler allocated in PSRAM (%u bytes)", RSSI_BUFFER_SIZE * sizeof(RssiSample));
    return true;
}

void RssiSampler::deinit() {
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
    if (lock.isLocked()) {
        if (_buffer) {
            heap_caps_free(_buffer);
            _buffer = nullptr;
        }
        LOGI("RssiSampler freed");
    } else {
        LOGE("Failed to lock mutex for deinit, forcing free");
        if (_buffer) {
            heap_caps_free(_buffer);
            _buffer = nullptr;
        }
    }
}

RssiSampler::~RssiSampler() {
    if (_mutex) {
        vSemaphoreDelete(_mutex);
    }
}

RssiSample RssiSampler::takeSample() {
    RssiSample sample = {0, {0,0,0}, 0};
    
    wifi_mode_t mode = WiFi.getMode();
    
    int8_t rssi = 0;
    
    if (mode == WIFI_AP || (mode == WIFI_AP_STA && !WiFi.isConnected())) {
        if (WiFi.softAPgetStationNum() > 0) {
            wifi_sta_list_t stations;
            if (esp_wifi_ap_get_sta_list(&stations) == ESP_OK && stations.num > 0) {
                // Use the first client's RSSI for sensing
                rssi = stations.sta[0].rssi;
            } else {
                 return sample; // Error reading stations
            }
        } else {
            return sample; // No stations
        }
    } else {
         if (!WiFi.isConnected()) {
            return sample;
        }
        rssi = WiFi.RSSI();
    }

    sample.rssi = rssi;
    
    // Validate RSSI range
    if (sample.rssi > 0 || sample.rssi < -120) {
        LOGW("Unusual RSSI: %d dBm", sample.rssi);
    }
    
    sample.timestampMs = millis();
    // Initialize padding to zero to prevent uninitialized data warnings/issues
    sample._pad[0] = 0; 
    sample._pad[1] = 0; 
    sample._pad[2] = 0; 
    
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(10));
    if (_buffer && lock.isLocked()) {
        _buffer[_head] = sample;
        _head = (_head + 1) % RSSI_BUFFER_SIZE;
        
        if (_count < RSSI_BUFFER_SIZE) {
            _count = _count + 1;
        }
    } else {
        // Drop sample if mutex busy (rare)
    }
    
    return sample;
}

uint16_t RssiSampler::getSamples(RssiSample* outBuffer, uint16_t maxCount) const {
    if (outBuffer == nullptr || maxCount == 0 || !_mutex) {
        return 0;
    }
    
    uint16_t copied = 0;
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(50));
    if (lock.isLocked()) {
        uint16_t currentCount = _count;
        uint16_t currentHead = _head;
        
        uint16_t toCopy = (currentCount < maxCount) ? currentCount : maxCount;
        
        // Copy most recent samples (reverse order usually easier for consumers, 
        // but ring buffer copy preserves chronological order here?)
        // Let's copy chronological: Oldest -> Newest
        // Or Newest -> Oldest?
        // Standard "getSamples" usually implies "get History".
        // Let's copy from Oldest to Newest (Chronological).
        
        // Calculate start index (Oldest sample among 'toCopy' recent samples)
        // If buffer is full, head is Oldest.
        // Wait, logic:
        // Head is where NEXT write goes. so Head-1 is Newest.
        // Head is Oldest (if full).
        
        // Let's copy in REVERSE order (Newest at index 0, Oldest at index N) 
        // because that simplifies "get last N samples".
        
        for (uint16_t i = 0; i < toCopy; i++) {
             // idx = (head - 1 - i)
             int32_t idx = (int32_t)currentHead - 1 - i;
             if (idx < 0) idx += RSSI_BUFFER_SIZE;
             
             outBuffer[i] = _buffer[idx];
        }
        
        copied = toCopy;
    }
    return copied;
}

void RssiSampler::clear() {
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(50));
    if (lock.isLocked()) {
        _head = 0;
        _count = 0;
    }
}

uint16_t RssiSampler::getCount() const {
    uint16_t c = 0;
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(20));
    if (lock.isLocked()) {
        c = _count;
    }
    return c;
}

const RssiSample* RssiSampler::getBufferUnsafe() const {
    return _buffer;
}

uint16_t RssiSampler::getHeadUnsafe() const {
    return _head;
}

uint16_t RssiSampler::getCountUnsafe() const {
    return _count;
}


}  // namespace WIFISENSING
