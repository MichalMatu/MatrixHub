/**
 * @file NotificationBroadcaster.h
 * @brief Broadcasts notification worker statistics via WebSocket channel
 * 
 * Periodically reads RTC runtime stats and sends a compact binary packet
 * to all WebSocket subscribers on the NOTIF_STATS channel.
 */

#pragma once

#include "../../common/ChannelSubscriptions.h"
#include <PsychicHttpServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

namespace TELEGRAM { class TelegramWorker; }

namespace API {

class NotificationBroadcaster {
public:
    NotificationBroadcaster() = default;

    void begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, PsychicHttpServer* server, TELEGRAM::TelegramWorker* telegramWorker);
    void syncSubscriptionState();
    void sendSnapshot(int fd);

private:
    WebSocketBroadcaster* _systemWs = nullptr;
    ChannelSubscriptions* _channels = nullptr;
    PsychicHttpServer* _server = nullptr;
    TELEGRAM::TelegramWorker* _telegramWorker = nullptr;
    TimerHandle_t _broadcastTimer = nullptr;

    static void timerCallback(TimerHandle_t xTimer);
    size_t buildPacket(uint8_t* packet, size_t capacity) const;
    void broadcastStats();
};

} // namespace API
