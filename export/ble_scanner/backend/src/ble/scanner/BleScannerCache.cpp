#include "BleScanner.h"
#include "BleWhitelistReconciler.h"

#include "../../config/System.h"
#include "BleDeviceProcessor.h"

#include <cstring>

namespace BLE {

void BleScanner::processDevice(const NimBLEAdvertisedDevice* device) {
    if (!_bleStats) {
        return;
    }

    DiscoveryCallback discoveryCb = nullptr;
    if (_callbackMutex) {
        if (xSemaphoreTake(_callbackMutex, 0) == pdTRUE) {
            discoveryCb = _discoveryCallback;
            xSemaphoreGive(_callbackMutex);
        }
    } else {
        discoveryCb = _discoveryCallback;
    }

    bool processed = BleDeviceProcessor::process(
        device,
        *this,
        _whitelist,
        _callback,
        _discoveryMode.load(std::memory_order_acquire),
        _targetedMode.load(std::memory_order_acquire),
        *_bleStats
    );

    // Only broadcast if the device was actually processed and accepted.
    if (!processed || !discoveryCb || !device) {
        return;
    }

    // Use stack buffer instead of std::string allocation in callback path.
    char macStr[18];
    const uint8_t* nativeMac = device->getAddress().getVal();
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             nativeMac[5], nativeMac[4], nativeMac[3], nativeMac[2], nativeMac[1], nativeMac[0]);

    float temp;
    float humid;
    uint8_t batt;
    int8_t rssi;
    uint32_t lastSeen;

    bool found = getCachedDeviceData(macStr, temp, humid, batt, rssi, lastSeen);
    if (!found) {
        size_t count = getDiscoverySlotCount();
        const char* dMac = nullptr;

        for (size_t i = 0; i < count; i++) {
            if (!getDiscoveryEntryAt(i, dMac, temp, humid, batt, rssi, lastSeen)) {
                continue;
            }
            if (dMac && strcasecmp(dMac, macStr) == 0) {
                found = true;
                break;
            }
        }
    }

    if (found) {
        discoveryCb(macStr, temp, humid, batt, rssi);
        return;
    }

    // Should rarely happen if 'processed' is true.
    static uint32_t lastLog = 0;
    if (millis() - lastLog > TASK_MONITOR::BLE_WARNING_THROTTLE_MS) {
        LOGW("Device processed but missing from cache: %s (throttled)", macStr);
        lastLog = millis();
    }
}

bool BleScanner::shouldReport(
    const char* mac,
    float temperature,
    float humidity,
    uint8_t battery,
    int8_t rssi,
    uint32_t nowMs
) {
    if (_mutex) {
        // Called from NimBLE callback path: do not block host task.
        if (xSemaphoreTake(_mutex, 0) != pdTRUE) {
            static uint32_t lastLockWarn = 0;
            if (millis() - lastLockWarn > TASK_MONITOR::BLE_WARNING_THROTTLE_MS) {
                LOGW("Mutex busy in shouldReport - allowing pass-through (throttled)");
                lastLockWarn = millis();
            }
            return true;
        }
    }

    int idx = findOrAllocateSensorIndex(mac);
    if (idx < 0) {
        if (_mutex) {
            xSemaphoreGive(_mutex);
        }
        return true; // Not in whitelist slot map, pass-through.
    }

    auto& reading = _state.readings[idx];

    // SIMPLE PASS-THROUGH (no throttling): always update runtime cache.
    reading.lastSeenTime = nowMs;
    reading.temperature = temperature;
    reading.humidity = humidity;
    reading.battery = battery;
    reading.rssi = rssi;
    _runtimeStateVersion.fetch_add(1, std::memory_order_relaxed);

    if (_mutex) {
        xSemaphoreGive(_mutex);
    }
    return true; // Always report to WebSocket path.
}

void BleScanner::sanitizeLocked() {
    const uint32_t now = millis();
    bool changed = false;

    for (size_t i = 0; i < RTC::kMaxBleSensors; i++) {
        auto& reading = _state.readings[i];
        if (reading.lastSeenTime > now) {
            reading.lastSeenTime = 0;
            changed = true;
        }
    }

    if (changed) {
        _runtimeStateVersion.fetch_add(1, std::memory_order_relaxed);
    }
}

int BleScanner::findOrAllocateSensorIndex(const char* mac) {
    // Readings are indexed same as sensors[] (whitelist).
    for (size_t i = 0; i < _state.sensorCount; i++) {
        if (strncasecmp(_state.sensors[i].mac, mac, 17) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool BleScanner::getCachedDeviceData(
    const char* mac,
    float& outTemp,
    float& outHumid,
    uint8_t& outBatt,
    int8_t& outRssi,
    uint32_t& outLastSeen
) const {
    if (_mutex) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
            LOGW("Mutex timeout in getCachedDeviceData");
            return false;
        }
    }

    for (size_t i = 0; i < _state.sensorCount; i++) {
        if (strncasecmp(_state.sensors[i].mac, mac, 17) == 0) {
            const auto& reading = _state.readings[i];
            if (!detail::hasValidReading(reading)) {
                if (_mutex) {
                    xSemaphoreGive(_mutex);
                }
                return false;
            }
            outTemp = reading.temperature;
            outHumid = reading.humidity;
            outBatt = reading.battery;
            outRssi = reading.rssi;
            outLastSeen = reading.lastSeenTime;
            if (_mutex) {
                xSemaphoreGive(_mutex);
            }
            return true;
        }
    }

    if (_mutex) {
        xSemaphoreGive(_mutex);
    }
    return false;
}

bool BleScanner::getCachedDeviceDataAt(
    size_t slotIndex,
    const char*& outMac,
    float& outTemp,
    float& outHumid,
    uint8_t& outBatt,
    int8_t& outRssi,
    uint32_t& outLastSeen
) const {
    if (slotIndex >= _state.sensorCount) {
        return false;
    }

    if (_mutex) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
            LOGW("Mutex timeout in getCachedDeviceDataAt");
            return false;
        }
    }

    const auto& sensor = _state.sensors[slotIndex];
    const auto& reading = _state.readings[slotIndex];
    if (sensor.mac[0] == '\0') {
        if (_mutex) {
            xSemaphoreGive(_mutex);
        }
        return false;
    }

    outMac = sensor.mac;
    if (!detail::hasValidReading(reading)) {
        if (_mutex) {
            xSemaphoreGive(_mutex);
        }
        return false;
    }
    outTemp = reading.temperature;
    outHumid = reading.humidity;
    outBatt = reading.battery;
    outRssi = reading.rssi;
    outLastSeen = reading.lastSeenTime;
    if (_mutex) {
        xSemaphoreGive(_mutex);
    }
    return true;
}

bool BleScanner::updateDiscoveryCache(
    const char* mac,
    float temp,
    float humid,
    uint8_t batt,
    int8_t rssi,
    uint32_t nowMs
) {
    if (_mutex) {
        // Called from NimBLE callback path: do not block host task.
        if (xSemaphoreTake(_mutex, 0) != pdTRUE) {
            static uint32_t lastLockWarn = 0;
            if (millis() - lastLockWarn > TASK_MONITOR::BLE_WARNING_THROTTLE_MS) {
                LOGW("Mutex busy in updateDiscoveryCache - skipping packet (throttled)");
                lastLockWarn = millis();
            }
            return false;
        }
    }

    // Skip if already in whitelist.
    for (size_t i = 0; i < _state.sensorCount; i++) {
        if (strncasecmp(_state.sensors[i].mac, mac, 17) == 0) {
            if (_mutex) {
                xSemaphoreGive(_mutex);
            }
            return false;
        }
    }

    int idx = findOrAllocateDiscoverySlot(mac, nowMs);
    if (idx < 0) {
        if (_mutex) {
            xSemaphoreGive(_mutex);
        }
        return false;
    }

    // Protect valid data from being overwritten by partial advertising packets.
    const bool newValid = (temp > -100.0f);
    auto& entry = _discovered[idx];
    const bool oldValid = (entry.mac[0] != '\0' && entry.temperature > -100.0f);

    strncpy(entry.mac, mac, sizeof(entry.mac) - 1);
    entry.mac[sizeof(entry.mac) - 1] = '\0';
    entry.lastSeenTime = nowMs;
    entry.rssi = rssi;

    if (newValid || !oldValid) {
        entry.temperature = temp;
        entry.humidity = humid;
        entry.battery = batt;
    }

    if (static_cast<size_t>(idx) >= _discoveryCount) {
        _discoveryCount = idx + 1;
    }

    if (_mutex) {
        xSemaphoreGive(_mutex);
    }
    return true;
}

bool BleScanner::getDiscoveryEntryAt(
    size_t idx,
    const char*& outMac,
    float& outTemp,
    float& outHumid,
    uint8_t& outBatt,
    int8_t& outRssi,
    uint32_t& outLastSeen
) const {
    if (idx >= RTC::kMaxBleDiscovered) {
        return false;
    }

    if (_mutex) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
            LOGW("Mutex timeout in getDiscoveryEntryAt");
            return false;
        }
    }

    const auto& entry = _discovered[idx];
    if (entry.isEmpty()) {
        if (_mutex) {
            xSemaphoreGive(_mutex);
        }
        return false;
    }

    outMac = entry.mac;
    outTemp = entry.temperature;
    outHumid = entry.humidity;
    outBatt = entry.battery;
    outRssi = entry.rssi;
    outLastSeen = entry.lastSeenTime;
    if (_mutex) {
        xSemaphoreGive(_mutex);
    }
    return true;
}

void BleScanner::clearDiscoveryCache() {
    if (_mutex) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
            LOGW("Mutex timeout in clearDiscoveryCache");
            return;
        }
    }

    for (size_t i = 0; i < RTC::kMaxBleDiscovered; i++) {
        _discovered[i].clear();
    }
    _discoveryCount = 0;

    if (_mutex) {
        xSemaphoreGive(_mutex);
    }
}

int BleScanner::findOrAllocateDiscoverySlot(const char* mac, uint32_t nowMs) {
    (void)nowMs;

    for (size_t i = 0; i < RTC::kMaxBleDiscovered; i++) {
        if (!_discovered[i].isEmpty() &&
            strncasecmp(_discovered[i].mac, mac, 17) == 0) {
            return static_cast<int>(i);
        }
    }

    for (size_t i = 0; i < RTC::kMaxBleDiscovered; i++) {
        if (_discovered[i].isEmpty()) {
            return static_cast<int>(i);
        }
    }

    // Evict oldest entry (LRU).
    size_t oldestIdx = 0;
    uint32_t oldestTime = _discovered[0].lastSeenTime;
    for (size_t i = 1; i < RTC::kMaxBleDiscovered; i++) {
        if (_discovered[i].lastSeenTime < oldestTime) {
            oldestTime = _discovered[i].lastSeenTime;
            oldestIdx = i;
        }
    }

    _discovered[oldestIdx].clear();
    return static_cast<int>(oldestIdx);
}

} // namespace BLE
