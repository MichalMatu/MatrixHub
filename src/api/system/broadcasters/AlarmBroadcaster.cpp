#include "AlarmBroadcaster.h"
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

    // Binary packet format:
    // [0]   Magic byte 0x41 ('A' for Alarm)
    // [1..32] Rule id (null-terminated char[kMaxIdLen], padded with zeros)
    // [33]    Triggered (0/1)
    // [34]    Severity (uint8)
    // [35..38] Current value (float, little-endian)
    constexpr size_t kPacketSize = 1 + ALARMS::kMaxIdLen + 1 + 1 + sizeof(float);
    uint8_t packet[kPacketSize] = {0};
    size_t offset = 0;
    
    packet[offset++] = 0x41;  // 'A'
    memcpy(&packet[offset], change.id, ALARMS::kMaxIdLen);
    offset += ALARMS::kMaxIdLen;
    packet[offset++] = change.triggered ? 1 : 0;
    packet[offset++] = static_cast<uint8_t>(change.severity);
    
    // Pack float as little-endian bytes
    memcpy(&packet[offset], &change.currentValue, sizeof(float));
    offset += sizeof(float);

    if (_server && _server->server) {
        // Send to WebSocket channel
        _channels->broadcast(_systemWs, _server->server, ChannelSubscriptions::ALARMS, packet, offset);
        LOGD("Broadcast alarm state: id=%s trig=%d val=%.1f",
             change.id, change.triggered, change.currentValue);
    }
}

} // namespace API
