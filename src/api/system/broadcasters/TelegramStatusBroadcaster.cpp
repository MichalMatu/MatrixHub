/**
 * @file TelegramStatusBroadcaster.cpp
 * @brief Implementation of Telegram worker status WebSocket broadcaster
 *
 * Binary packet format (18 bytes):
 * [0]    Magic 0x47 ('G' for teleGram)
 * [1]    Flags: bit0=enabled, bit1=running
 * [2-3]  last_poll_age_sec (uint16 LE)
 * [4-7]  messages_processed (uint32 LE)
 * [8-11] messages_sent (uint32 LE)
 * [12-15] commands_executed (uint32 LE)
 * [16-17] last_http_code (int16 LE)
 */

#include "TelegramStatusBroadcaster.h"

#include "../../../notifications/telegram/runtime/TelegramWorker.h"
#include "../../common/WebSocketBroadcaster.h"
#include "../../../system/logging/Logging.h"

#include <Arduino.h>

#undef LOG_TAG
#define LOG_TAG "TgBcast"

namespace API {

void TelegramStatusBroadcaster::begin(WebSocketBroadcaster* systemWs,
                                      ChannelSubscriptions* channels,
                                      PsychicHttpServer* server,
                                      TELEGRAM::TelegramWorker* worker) {
    _systemWs = systemWs;
    _channels = channels;
    _server = server;
    _worker = worker;

    if (worker) {
        worker->setStatusChangeCallback(
            [this](const TELEGRAM::WorkerStatus& status) {
                this->onStatusChange(status);
            }
        );
    }

    LOGI("TelegramStatusBroadcaster initialized");
}

size_t TelegramStatusBroadcaster::buildPacket(const TELEGRAM::WorkerStatus& status,
                                              uint8_t* packet,
                                              size_t capacity) const {
    if (!packet || capacity < 18) {
        return 0;
    }

    size_t offset = 0;

    packet[offset++] = 0x47;

    uint8_t flags = 0;
    if (status.enabled) flags |= 0x01;
    if (status.running) flags |= 0x02;
    packet[offset++] = flags;

    uint16_t pollAge = 0;
    if (status.lastPollMs > 0) {
        pollAge = static_cast<uint16_t>((millis() - status.lastPollMs) / 1000);
    }
    memcpy(&packet[offset], &pollAge, sizeof(uint16_t));
    offset += 2;

    memcpy(&packet[offset], &status.messagesProcessed, sizeof(uint32_t));
    offset += 4;

    memcpy(&packet[offset], &status.messagesSent, sizeof(uint32_t));
    offset += 4;

    memcpy(&packet[offset], &status.commandsExecuted, sizeof(uint32_t));
    offset += 4;

    int16_t httpCode = static_cast<int16_t>(status.lastHttpCode);
    memcpy(&packet[offset], &httpCode, sizeof(int16_t));
    offset += 2;

    return offset;
}

void TelegramStatusBroadcaster::sendSnapshot(int fd) {
    if (!_systemWs || !_worker || fd < 0) {
        return;
    }

    const TELEGRAM::WorkerStatus status = _worker->getStatus();
    uint8_t packet[18];
    const size_t len = buildPacket(status, packet, sizeof(packet));
    if (len == 0) {
        return;
    }

    int target = fd;
    _systemWs->broadcast(&target, 1, packet, len, HTTPD_WS_TYPE_BINARY);
}

void TelegramStatusBroadcaster::onStatusChange(const TELEGRAM::WorkerStatus& status) {
    if (!_channels || !_channels->hasSubscribers(ChannelSubscriptions::TELEGRAM)) {
        return;
    }

    uint8_t packet[18];
    const size_t len = buildPacket(status, packet, sizeof(packet));
    if (len == 0) {
        return;
    }

    if (_server && _server->server) {
        _channels->broadcast(_systemWs, _server->server, ChannelSubscriptions::TELEGRAM, packet, len);
    }
}

}  // namespace API
