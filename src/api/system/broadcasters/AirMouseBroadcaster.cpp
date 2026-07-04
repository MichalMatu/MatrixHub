#include "AirMouseBroadcaster.h"
#include "../../../airmouse/AirMouseService.h"
#include "../../../system/logging/Logging.h"
#include <Arduino.h>
#include <cmath>

#undef LOG_TAG
#define LOG_TAG "AirMouseBcast"

namespace API {

namespace {
constexpr uint8_t kAirMouseMagic = 0x49; // 'I' for IMU

struct __attribute__((packed)) AirMousePacket {
    uint8_t magic;
    uint32_t ts;
    float gx;
    float gy;
    float gz;
    float ax;
    float ay;
    float az;
    float deltaG;
};
} // namespace

AirMouseBroadcaster::AirMouseBroadcaster() {}

AirMouseBroadcaster::~AirMouseBroadcaster() {
    setStreamingEnabled(false);
}

void AirMouseBroadcaster::begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, PsychicHttpServer* server,
                                AIRMOUSE::AirMouseService* airMouseService) {
    _systemWs = systemWs;
    _channels = channels;
    _server = server;
    _airMouseService = airMouseService;
    syncSubscriptionState();
}

void AirMouseBroadcaster::syncSubscriptionState() {
    const bool shouldStream = _channels && _channels->hasSubscribers(ChannelSubscriptions::AIRMOUSE);
    if (shouldStream == _streamingEnabled) {
        return;
    }
    setStreamingEnabled(shouldStream);
}

void AirMouseBroadcaster::setStreamingEnabled(bool enabled) {
    if (!_airMouseService) {
        return;
    }

    if (!enabled) {
        _airMouseService->setImuCallback(nullptr);
        _streamingEnabled = false;
        return;
    }

    _imuLastSendTime = 0;
    _imuLastGx = 0.0f;
    _imuLastGy = 0.0f;
    _imuLastGz = 0.0f;

    _airMouseService->setImuCallback([this](float gx, float gy, float gz, float ax, float ay, float az, float deltaG) {
        if (!_channels || !_systemWs || !_server || !_server->server) {
            return;
        }
        if (!_channels->hasSubscribers(ChannelSubscriptions::AIRMOUSE)) {
            return;
        }

        // Preserve the existing AirMouse websocket throttle semantics when
        // moving the stream onto /ws/system so the UI cadence does not change.
        const uint32_t now = millis();
        const uint32_t elapsed = now - _imuLastSendTime;
        const bool isMoving = (deltaG > 0.2f) ||
                              (fabsf(gx - _imuLastGx) > 5.0f) ||
                              (fabsf(gy - _imuLastGy) > 5.0f) ||
                              (fabsf(gz - _imuLastGz) > 5.0f);
        const uint32_t interval = isMoving ? 50 : 1000;
        if (elapsed < interval) {
            return;
        }

        _imuLastSendTime = now;
        _imuLastGx = gx;
        _imuLastGy = gy;
        _imuLastGz = gz;

        AirMousePacket packet = {
            .magic = kAirMouseMagic,
            .ts = now,
            .gx = gx,
            .gy = gy,
            .gz = gz,
            .ax = ax,
            .ay = ay,
            .az = az,
            .deltaG = deltaG
        };

        _channels->broadcast(_systemWs, _server->server, ChannelSubscriptions::AIRMOUSE,
                             reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
    });

    _streamingEnabled = true;
    LOGI("AirMouse system-channel streaming: ACTIVE");
}

} // namespace API
