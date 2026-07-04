#pragma once

#include "../../common/WebSocketBroadcaster.h"
#include "../../common/ChannelSubscriptions.h"
#include <PsychicHttpServer.h>

namespace WIFISENSING {
    class WifiSensingService;
}

namespace API {

class SensorBroadcaster {
public:
    SensorBroadcaster();
    ~SensorBroadcaster();

    void begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, PsychicHttpServer* server, WIFISENSING::WifiSensingService* wifiSensing);
    
    // For Shelly integration
    void sendShellyEvent(const void* devicePtr);

private:
    WebSocketBroadcaster* _systemWs = nullptr;
    ChannelSubscriptions* _channels = nullptr;
    PsychicHttpServer* _server = nullptr;
    
    uint32_t _lastSensingBroadcastMs = 0;
    uint32_t _lastSensorBroadcastMs = 0;

    void onSensorUpdate(const void* snapPtr); // Using void* to avoid circular dep, cast inside
    void onSensingUpdate(const void* samplePtr, const void* statsPtr, bool isMotion);
};

} // namespace API
