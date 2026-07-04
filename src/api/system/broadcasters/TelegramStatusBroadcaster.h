/**
 * @file TelegramStatusBroadcaster.h
 * @brief Broadcasts Telegram worker status changes via WebSocket channel
 */

#pragma once

#include "../../common/ChannelSubscriptions.h"
#include <PsychicHttpServer.h>

namespace TELEGRAM { class TelegramWorker; struct WorkerStatus; }

namespace API {

class TelegramStatusBroadcaster {
public:
    TelegramStatusBroadcaster() = default;

    void begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, PsychicHttpServer* server,
               TELEGRAM::TelegramWorker* worker);
    void sendSnapshot(int fd);

private:
    WebSocketBroadcaster* _systemWs = nullptr;
    ChannelSubscriptions* _channels = nullptr;
    PsychicHttpServer* _server = nullptr;
    TELEGRAM::TelegramWorker* _worker = nullptr;

    size_t buildPacket(const TELEGRAM::WorkerStatus& status, uint8_t* packet, size_t capacity) const;
    void onStatusChange(const TELEGRAM::WorkerStatus& status);
};

}  // namespace API
