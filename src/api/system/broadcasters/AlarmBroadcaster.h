#pragma once

#include "../../common/ChannelSubscriptions.h"
#include <PsychicHttpServer.h>

// Forward declaration
namespace ALARMS {
class AlarmService;
struct AlarmStateChange;
}

namespace API {

class AlarmBroadcaster {
public:
    AlarmBroadcaster();
    ~AlarmBroadcaster() = default;

    void begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, PsychicHttpServer* server, ALARMS::AlarmService* alarmService);

private:
    WebSocketBroadcaster* _systemWs = nullptr;
    ChannelSubscriptions* _channels = nullptr;
    PsychicHttpServer* _server = nullptr;

    void onAlarmStateChange(const ALARMS::AlarmStateChange& change);
};

} // namespace API
