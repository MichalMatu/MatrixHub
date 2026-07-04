#include "BleDeviceWhitelist.h"
#include "../../../system/logging/Logging.h"
#include <Arduino.h>
#include <cstring>
#include <esp_heap_caps.h>

#undef LOG_TAG
#define LOG_TAG "BLEWhitelist"

namespace BLE {

BleDeviceWhitelist::BleDeviceWhitelist() {
    // Allocate whitelist in PSRAM (required configuration)
    size_t size = sizeof(BleSensorConfig) * RTC::kMaxBleSensors;
    _whitelist = (BleSensorConfig*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!_whitelist) {
        LOGE("Failed to allocate whitelist in PSRAM");
    }
    
    if (_whitelist) {
        memset(_whitelist, 0, size);
    } else {
        LOGE("Failed to allocate whitelist memory!");
    }
}

BleDeviceWhitelist::~BleDeviceWhitelist() {
    if (_whitelist) {
        heap_caps_free(_whitelist);
        _whitelist = nullptr;
    }
}

void BleDeviceWhitelist::update(const BleSensorConfig* sensors, size_t count) {
    if (!_whitelist) {
        return;
    }

    // Safety cap
    if (count > RTC::kMaxBleSensors) {
        LOGW("Truncating whitelist to %d", RTC::kMaxBleSensors);
        count = RTC::kMaxBleSensors;
    }
    
    // Protect against race conditions with isWhitelisted()
    bool expected = false;
    while (!_updating.compare_exchange_weak(expected, true)) {
        expected = false;
        vTaskDelay(pdMS_TO_TICKS(1)); // Yield if busy
    }
    
    // Copy whitelist
    _count = count;
    for (size_t i = 0; i < count; i++) {
        _whitelist[i] = sensors[i];
    }
    
    _updating = false;
    
    LOGI("Whitelist updated: %u devices", static_cast<unsigned>(count));
}

bool BleDeviceWhitelist::isWhitelisted(const char* mac) const {
    if (!mac || mac[0] == '\0') {
        return false;
    }

    if (!_whitelist) {
        return false;
    }

    // Lock-free skip if updating (better to miss one packet than block)
    if (_updating.load(std::memory_order_relaxed)) {
        return false;
    }
    
    size_t cnt = _count.load(std::memory_order_relaxed);
    for (size_t i = 0; i < cnt; i++) {
        if (strncasecmp(_whitelist[i].mac, mac, 17) == 0) {
            return true;
        }
    }
    
    return false;
}

}  // namespace BLE
