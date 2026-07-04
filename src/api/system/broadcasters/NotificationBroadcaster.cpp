/**
 * @file NotificationBroadcaster.cpp
 * @brief Broadcasts notification worker statistics via WebSocket binary packet
 * 
 * Packet format (112 bytes total):
 * [0]       Magic 0x4E ('N')
 * [1-14]    Webhook:   sent(u32) failed(u32) lastMs(u32) httpCode(i16)
 * [15-28]   Pushover:  sent(u32) failed(u32) lastMs(u32) httpCode(i16)
 * [29-40]   UDP:       sent(u32) failed(u32) lastMs(u32)
 * [41-88]   Heartbeat: 4 slots × { lastPingMs(u32) successCount(u32) failCount(u32) }
 * [89-107]  Telegram:  flags(u8) lastActivityMs(u32) messagesProcessed(u32)
 *                      messagesSent(u32) commandsExecuted(u32) lastHttpCode(i16)
 * [108-111] uptimeMs(u32) — device millis() for age calculations
 * 
 * Note: Test sends (from /api/notifications/{type}/test) are counted in the
 * main sent/failed counters, matching the Telegram unified style.
 */

#include "NotificationBroadcaster.h"
#include "../../../system/rtc/RtcConfig.h"
#include "../../../system/logging/Logging.h"
#include "../../../notifications/telegram/runtime/TelegramWorker.h"
#include "../../common/WebSocketBroadcaster.h"
#include <Arduino.h>

#undef LOG_TAG
#define LOG_TAG "NotifBcast"

namespace {

// --- Packet layout constants ---
constexpr size_t kMagicSize     = 1;
constexpr size_t kWebhookSize   = 14;   // u32 sent + u32 failed + u32 lastMs + i16 httpCode
constexpr size_t kPushoverSize  = 14;
constexpr size_t kUdpSize       = 12;   // u32 sent + u32 failed + u32 lastMs
constexpr size_t kHeartbeatSize = 48;   // 4 slots × (u32 lastPingMs + u32 success + u32 fail)
constexpr size_t kTelegramSize  = 19;   // flags + lastActivityMs + u32 processed + u32 sent + u32 commands + i16 httpCode
constexpr size_t kUptimeSize    = 4;    // u32 millis()

constexpr size_t kPacketSize = kMagicSize + kWebhookSize + kPushoverSize
                             + kUdpSize + kHeartbeatSize + kTelegramSize
                             + kUptimeSize;

// Compile-time guard: if packet layout changes, update frontend parser (parsers.ts)!
static_assert(kPacketSize == 112, "Packet size mismatch!");

constexpr uint8_t kMagic = 0x4E; // 'N'

// --- Serialization helpers ---
inline void writeU32(uint8_t* buf, size_t& off, uint32_t val) {
    memcpy(&buf[off], &val, 4); off += 4;
}
inline void writeI16(uint8_t* buf, size_t& off, int16_t val) {
    memcpy(&buf[off], &val, 2); off += 2;
}

} // anonymous namespace

namespace API {

void NotificationBroadcaster::begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, PsychicHttpServer* server, TELEGRAM::TelegramWorker* telegramWorker) {
    _systemWs = systemWs;
    _channels = channels;
    _server = server;
    _telegramWorker = telegramWorker;

    _broadcastTimer = xTimerCreate(
        "NotifBcastTmr",
        pdMS_TO_TICKS(2000),
        pdTRUE,
        this,
        timerCallback
    );

    syncSubscriptionState();
    LOGI("Initialized (packet=%u bytes, interval=2s)", kPacketSize);
}

void NotificationBroadcaster::syncSubscriptionState() {
    if (!_broadcastTimer) {
        return;
    }

    const bool shouldRun = _channels && _channels->hasSubscribers(ChannelSubscriptions::NOTIF_STATS);
    const bool isRunning = xTimerIsTimerActive(_broadcastTimer) != pdFALSE;

    if (shouldRun == isRunning) {
        return;
    }

    if (shouldRun) {
        xTimerStart(_broadcastTimer, 0);
        broadcastStats();
        return;
    }

    xTimerStop(_broadcastTimer, 0);
}

void NotificationBroadcaster::timerCallback(TimerHandle_t xTimer) {
    auto* self = static_cast<NotificationBroadcaster*>(pvTimerGetTimerID(xTimer));
    if (self) {
        self->broadcastStats();
    }
}

size_t NotificationBroadcaster::buildPacket(uint8_t* packet, size_t capacity) const {
    if (!packet || capacity < kPacketSize) {
        return 0;
    }

    memset(packet, 0, kPacketSize);
    size_t offset = 0;
    const auto& stats = RTC::runtimeStats;

    // [0] Magic
    packet[offset++] = kMagic;

    // Webhook (14 bytes)
    writeU32(packet, offset, stats.webhookSent);
    writeU32(packet, offset, stats.webhookFailed);
    writeU32(packet, offset, stats.webhookLastSendMs);
    writeI16(packet, offset, stats.webhookLastHttpCode);

    // Pushover (14 bytes)
    writeU32(packet, offset, stats.pushoverSent);
    writeU32(packet, offset, stats.pushoverFailed);
    writeU32(packet, offset, stats.pushoverLastSendMs);
    writeI16(packet, offset, stats.pushoverLastHttpCode);

    // UDP (12 bytes)
    writeU32(packet, offset, stats.udpSent);
    writeU32(packet, offset, stats.udpFailed);
    writeU32(packet, offset, stats.udpLastSendMs);

    // Heartbeat (48 bytes)
    for (int i = 0; i < 4; i++) {
        writeU32(packet, offset, stats.heartbeatSlots[i].lastPingMs);
        writeU32(packet, offset, stats.heartbeatSlots[i].successCount);
        writeU32(packet, offset, stats.heartbeatSlots[i].failCount);
    }

    // Telegram (19 bytes)
    TELEGRAM::WorkerStatus telegramStatus = {};
    if (_telegramWorker) {
        telegramStatus = _telegramWorker->getStatus();
    }

    uint8_t telegramFlags = 0;
    if (telegramStatus.enabled) telegramFlags |= 0x01;
    if (telegramStatus.running) telegramFlags |= 0x02;
    packet[offset++] = telegramFlags;

    const uint32_t lastActivityMs =
        (telegramStatus.lastSendMs > telegramStatus.lastPollMs)
            ? telegramStatus.lastSendMs
            : telegramStatus.lastPollMs;
    writeU32(packet, offset, lastActivityMs);
    writeU32(packet, offset, telegramStatus.messagesProcessed);
    writeU32(packet, offset, telegramStatus.messagesSent);
    writeU32(packet, offset, telegramStatus.commandsExecuted);
    writeI16(packet, offset, static_cast<int16_t>(telegramStatus.lastHttpCode));

    // Device uptime (4 bytes)
    writeU32(packet, offset, (uint32_t)millis());

    return offset;
}

void NotificationBroadcaster::sendSnapshot(int fd) {
    if (!_systemWs || fd < 0) {
        return;
    }

    uint8_t packet[kPacketSize];
    const size_t len = buildPacket(packet, sizeof(packet));
    if (len == 0) {
        return;
    }

    int target = fd;
    _systemWs->broadcast(&target, 1, packet, len, HTTPD_WS_TYPE_BINARY);
}

void NotificationBroadcaster::broadcastStats() {
    if (!_channels || !_channels->hasSubscribers(ChannelSubscriptions::NOTIF_STATS)) {
        return;
    }

    uint8_t packet[kPacketSize];
    const size_t len = buildPacket(packet, sizeof(packet));
    if (len == 0) {
        return;
    }

    if (_server && _server->server) {
        _channels->broadcast(_systemWs, _server->server, ChannelSubscriptions::NOTIF_STATS, packet, len);
    }
}

} // namespace API
