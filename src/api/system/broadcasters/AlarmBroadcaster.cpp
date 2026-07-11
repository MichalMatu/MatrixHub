#include "AlarmBroadcaster.h"
#include "AlarmStatePacket.h"
#include "../../../alarms/AlarmService.h" // Keep for AlarmStateChange definition if needed, but instance() is removed
#include "../../../alarms/types/AlarmConstants.h"
#include "../../../system/logging/Logging.h"
#include <Arduino.h>

#undef LOG_TAG
#define LOG_TAG "AlarmBcast"

namespace API {

AlarmBroadcaster::AlarmBroadcaster() {}

void AlarmBroadcaster::begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, PsychicHttpServer* server, ALARMS::AlarmService* alarmService) {
    _systemWs = systemWs;
    _channels = channels;
    _server = server;

    if (alarmService) {
        // Register callback with AlarmService for state changes
        alarmService->setStateChangeCallback(
            [this](const ALARMS::AlarmStateChange& change) {
                this->onAlarmStateChange(change);
            }
        );
    }
    
    LOGI("AlarmBroadcaster initialized");
}

void AlarmBroadcaster::onAlarmStateChange(const ALARMS::AlarmStateChange& change) {
    // Only broadcast if someone subscribed to ALARMS channel
    if (!_channels || !_channels->hasSubscribers(ChannelSubscriptions::ALARMS)) {
        return;
    }

    uint8_t packet[kAlarmStatePacketSize] = {0};
    const size_t packetSize = encodeAlarmStatePacket(change, packet, sizeof(packet));
    if (packetSize == 0) {
        return;
    }

    if (_server && _server->server) {
        // Send to WebSocket channel
        _channels->broadcast(
            _systemWs,
            _server->server,
            ChannelSubscriptions::ALARMS,
            packet,
            packetSize);
        LOGD("Broadcast alarm state: id=%s trig=%d val=%.1f seq=%lu",
             change.id,
             change.triggered,
             change.currentValue,
             static_cast<unsigned long>(change.transitionSeq));
    }
}

} // namespace API
