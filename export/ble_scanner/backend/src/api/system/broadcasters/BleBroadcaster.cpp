#include "BleBroadcaster.h"
#include "../../../ble/BleService.h"
#include "../../../system/logging/Logging.h"
#include <Arduino.h>

#undef LOG_TAG
#define LOG_TAG "BleBcast"

namespace API {

BleBroadcaster::BleBroadcaster() {}

BleBroadcaster::~BleBroadcaster() {
    setStreamingEnabled(false);
    if (_throttleCache) {
        heap_caps_free(_throttleCache);
        _throttleCache = nullptr;
    }
}

void BleBroadcaster::begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, PsychicHttpServer* server) {
    _systemWs = systemWs;
    _channels = channels;
    _server = server;
    syncSubscriptionState();
}

void BleBroadcaster::syncSubscriptionState() {
    const bool shouldStream = _channels && _channels->hasSubscribers(ChannelSubscriptions::BLE);
    if (shouldStream == _streamingEnabled) {
        return;
    }
    setStreamingEnabled(shouldStream);
}

bool BleBroadcaster::ensureThrottleCache() {
    if (!_throttleCache) {
        _throttleCache = (ThrottleEntry*)heap_caps_malloc(kMaxThrottle * sizeof(ThrottleEntry), MALLOC_CAP_SPIRAM);
        if (_throttleCache) {
            memset(_throttleCache, 0, kMaxThrottle * sizeof(ThrottleEntry));
            LOGI("Throttle Cache allocated in PSRAM");
        } else {
            LOGE("Failed to allocate Throttle Cache in PSRAM");
            return false;
        }
    }
    return true;
}

void BleBroadcaster::setStreamingEnabled(bool enabled) {
    if (!_bleService) {
        if (enabled) {
            LOGW("BleService not set in BleBroadcaster");
        }
        _streamingEnabled = false;
        return;
    }

    if (!enabled) {
        _bleService->setDiscoveryCallback(nullptr);
        _streamingEnabled = false;
        return;
    }

    if (!ensureThrottleCache()) {
        _streamingEnabled = false;
        return;
    }

    _bleService->setDiscoveryCallback(
        [this](const char* macStr, float temp, float humid, uint8_t batt, int8_t rssi) {
            this->onBleDiscovery(macStr, temp, humid, batt, rssi);
        }
    );
    _streamingEnabled = true;
}

void BleBroadcaster::onBleDiscovery(const char* macStr, float temp, float humid, uint8_t batt, int8_t rssi) {
    // Only send if someone subscribed to BLE channel
    if (!_channels || !_channels->hasSubscribers(ChannelSubscriptions::BLE)) return;
    
    // Safety check
    if (!_throttleCache) return;

    // Check throttling
    uint32_t now = millis();
    int slot = -1;
    
    // 1. Find existing
    for (size_t i = 0; i < kMaxThrottle; i++) {
        if (_throttleCache[i].mac[0] != '\0' && strcasecmp(_throttleCache[i].mac, macStr) == 0) {
            slot = i;
            break;
        }
    }
    
    if (slot >= 0) {
        // Found existing
        if (now - _throttleCache[slot].lastBroadcast < 5000) {
            return; // Throttle (5s)
        }
        _throttleCache[slot].lastBroadcast = now;
    } else {
        // 2. Find empty slot
        for (size_t i = 0; i < kMaxThrottle; i++) {
                if (_throttleCache[i].mac[0] == '\0') {
                    slot = i;
                    break;
                }
        }
        
        // 3. Fallback: Overwrite random/first slot if full (Simple Ring or index 0)
        if (slot < 0) {
            slot = (now / 100) % kMaxThrottle; 
        }
        
        // Save new entry
        strncpy(_throttleCache[slot].mac, macStr, 17);
        _throttleCache[slot].mac[17] = '\0';
        _throttleCache[slot].lastBroadcast = now;
    }

    // Convert MAC string to 6 bytes
    // "AA:BB:CC:DD:EE:FF" -> [AA, BB, CC, DD, EE, FF]
    uint8_t macBytes[6];
    sscanf(macStr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
        &macBytes[0], &macBytes[1], &macBytes[2], &macBytes[3], &macBytes[4], &macBytes[5]);

    uint8_t packet[16];
    packet[0] = 0x42; // 'B'
    memcpy(&packet[1], macBytes, 6);
    
    int16_t t = (int16_t)(temp * 10);
    memcpy(&packet[7], &t, 2);
    
    uint16_t h = (uint16_t)(humid * 10);
    memcpy(&packet[9], &h, 2);
    
    packet[11] = batt;
    packet[12] = (uint8_t)rssi;
    packet[13] = 0; 
    packet[14] = 0;
    packet[15] = 0;

    if (_server && _server->server) {
        // Broadcast
        _channels->broadcast(_systemWs, _server->server, ChannelSubscriptions::BLE, packet, 16);
    }
}

} // namespace API
