#pragma once

#include "../../common/ChannelSubscriptions.h"
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <esp_heap_caps.h>
#include <PsychicHttpServer.h>

namespace BLE { class BleService; }

namespace API {

class BleBroadcaster {
public:
    BleBroadcaster();
    ~BleBroadcaster();

    void setBleService(BLE::BleService* service) { _bleService = service; }
    void begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, PsychicHttpServer* server);
    void syncSubscriptionState();

private:
    WebSocketBroadcaster* _systemWs = nullptr;
    ChannelSubscriptions* _channels = nullptr;
    PsychicHttpServer* _server = nullptr;
    BLE::BleService* _bleService = nullptr;

    struct ThrottleEntry {
        char mac[18];
        uint32_t lastBroadcast;
    };
    
    ThrottleEntry* _throttleCache = nullptr;
    static const size_t kMaxThrottle = 20;
    bool _streamingEnabled = false;

    bool ensureThrottleCache();
    void setStreamingEnabled(bool enabled);
    void onBleDiscovery(const char* macStr, float temp, float humid, uint8_t batt, int8_t rssi);
};

} // namespace API
