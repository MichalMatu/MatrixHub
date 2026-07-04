#pragma once

#include "../../common/ChannelSubscriptions.h"
#include <PsychicHttpServer.h>
#include <cstdint>

namespace AIRMOUSE {
class AirMouseService;
}

namespace API {

class AirMouseBroadcaster {
public:
    AirMouseBroadcaster();
    ~AirMouseBroadcaster();

    void begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, PsychicHttpServer* server,
               AIRMOUSE::AirMouseService* airMouseService);
    void syncSubscriptionState();

private:
    WebSocketBroadcaster* _systemWs = nullptr;
    ChannelSubscriptions* _channels = nullptr;
    PsychicHttpServer* _server = nullptr;
    AIRMOUSE::AirMouseService* _airMouseService = nullptr;

    bool _streamingEnabled = false;
    uint32_t _imuLastSendTime = 0;
    float _imuLastGx = 0.0f;
    float _imuLastGy = 0.0f;
    float _imuLastGz = 0.0f;

    void setStreamingEnabled(bool enabled);
};

} // namespace API
