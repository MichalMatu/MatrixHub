#include "ChannelSubscriptions.h"
#include "WebSocketBroadcaster.h"
#include "../../config/System.h"
#include "../../system/utils/ScopeLock.h"
#include "../../system/logging/Logging.h"
#include "../../system/memory/PsramAllocator.h"
#include <cstring>
#include <ArduinoJson.h>

#undef LOG_TAG
#define LOG_TAG "ChannelSub"

namespace API {

ChannelSubscriptions::ChannelSubscriptions() {
    _lock = xSemaphoreCreateMutex();
    for (auto& count : _subscriberCounts) {
        count.store(0, std::memory_order_relaxed);
    }
}

ChannelSubscriptions::~ChannelSubscriptions() {
    if (_lock) {
        vSemaphoreDelete(_lock);
    }
}

size_t ChannelSubscriptions::getCachedSubscriberCount(Channel ch) const {
    if (!isSingleChannel(ch)) {
        return 0;
    }

    const size_t index = channelIndex(ch);
    if (index >= CHANNEL_COUNT) {
        return 0;
    }

    return _subscriberCounts[index].load(std::memory_order_relaxed);
}

void ChannelSubscriptions::incrementSubscriberCountLocked(Channel ch) {
    if (!isSingleChannel(ch)) {
        return;
    }

    const size_t index = channelIndex(ch);
    if (index < CHANNEL_COUNT) {
        _subscriberCounts[index].fetch_add(1, std::memory_order_relaxed);
    }
}

void ChannelSubscriptions::decrementSubscriberCountLocked(Channel ch) {
    if (!isSingleChannel(ch)) {
        return;
    }

    const size_t index = channelIndex(ch);
    if (index < CHANNEL_COUNT) {
        const size_t current = _subscriberCounts[index].load(std::memory_order_relaxed);
        if (current > 0) {
            _subscriberCounts[index].fetch_sub(1, std::memory_order_relaxed);
        }
    }
}

void ChannelSubscriptions::decrementMaskCountsLocked(uint16_t mask) {
    while (mask != 0) {
        const uint16_t bit = static_cast<uint16_t>(mask & (~mask + 1));
        decrementSubscriberCountLocked(static_cast<Channel>(bit));
        mask &= static_cast<uint16_t>(mask - 1);
    }
}

bool ChannelSubscriptions::subscribe(int fd, Channel ch) {
    bool changed = false;
    SYSTEM::ScopeLock lock(_lock);
    if (lock.isLocked()) {
        uint16_t& mask = _subscriptions[fd];
        if ((mask & static_cast<uint16_t>(ch)) == 0) {
            mask |= static_cast<uint16_t>(ch);
            incrementSubscriberCountLocked(ch);
            changed = true;
            LOGD("Client %d subscribed to channel 0x%04X", fd, ch);
        }
    }
    return changed;
}

bool ChannelSubscriptions::unsubscribe(int fd, Channel ch) {
    bool changed = false;
    SYSTEM::ScopeLock lock(_lock);
    if (lock.isLocked()) {
        auto it = _subscriptions.find(fd);
        if (it != _subscriptions.end()) {
            const uint16_t channelMask = static_cast<uint16_t>(ch);
            if ((it->second & channelMask) != 0) {
                it->second &= ~channelMask;
                decrementSubscriberCountLocked(ch);
                changed = true;
                LOGD("Client %d unsubscribed from channel 0x%04X", fd, ch);
                // Remove if no subscriptions left
                if (it->second == 0) {
                    _subscriptions.erase(it);
                }
            }
        }
    }
    return changed;
}

void ChannelSubscriptions::unsubscribeAll(int fd) {
    SYSTEM::ScopeLock lock(_lock);
    if (lock.isLocked()) {
        auto it = _subscriptions.find(fd);
        if (it != _subscriptions.end()) {
            LOGD("Client %d removed from all channels", fd);
            decrementMaskCountsLocked(it->second);
            _subscriptions.erase(it);
        }
    }
}

bool ChannelSubscriptions::hasSubscribers(Channel ch) const {
    return getCachedSubscriberCount(ch) > 0;
}

size_t ChannelSubscriptions::getSubscriberCount(Channel ch) const {
    return getCachedSubscriberCount(ch);
}

void ChannelSubscriptions::broadcast(WebSocketBroadcaster* broadcaster, httpd_handle_t serverHandle, Channel ch,
                                      const uint8_t* data, size_t len) {
    if (!serverHandle || !data || len == 0 || getCachedSubscriberCount(ch) == 0) return;

    int broadcastTargets[MAX_BROADCAST_TARGETS];
    size_t targetCount = 0;

    {
        SYSTEM::ScopeLock lock(_lock);
        if (lock.isLocked()) {
            for (const auto& [fd, mask] : _subscriptions) {
                if ((mask & (uint16_t)ch)) {
                    if (targetCount < MAX_BROADCAST_TARGETS) {
                        broadcastTargets[targetCount++] = fd;
                    } else {
                        LOGW("Broadcast target limit (%zu) exceeded! Dropping client %d on channel 0x%04X", MAX_BROADCAST_TARGETS, fd, ch);
                    }
                }
            }
        }
    }

    if (targetCount == 0) return;

    if (broadcaster) {
        // Use ASYNC multi-cast if broadcaster is provided
        broadcaster->broadcast(broadcastTargets, targetCount, (uint8_t*)data, len, HTTPD_WS_TYPE_BINARY);
    } else {
        // Fallback to SYNC send (blocking) if no broadcaster
        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
        ws_pkt.payload = (uint8_t*)data;
        ws_pkt.len = len;
        ws_pkt.type = HTTPD_WS_TYPE_BINARY;

        for (size_t i = 0; i < targetCount; i++) {
            int fd = broadcastTargets[i];
            httpd_ws_send_data(serverHandle, fd, &ws_pkt);
        }
    }
}

ChannelSubscriptions::Channel ChannelSubscriptions::channelFromName(const char* name) {
    if (!name) return NONE;
    
    if (strcmp(name, "shelly") == 0) return SHELLY;
    if (strcmp(name, "sensing") == 0) return SENSING;
    if (strcmp(name, "alarms") == 0) return ALARMS;
    if (strcmp(name, "power") == 0) return POWER;
    if (strcmp(name, "ble") == 0) return BLE;
    if (strcmp(name, "telegram") == 0) return TELEGRAM;
    if (strcmp(name, "notif_stats") == 0) return NOTIF_STATS;
    if (strcmp(name, "telemetry") == 0) return TELEMETRY;
    if (strcmp(name, "sensors") == 0) return TELEMETRY;
    if (strcmp(name, "macros") == 0) return MACROS;
    if (strcmp(name, "macro") == 0) return MACROS;
    if (strcmp(name, "system_status") == 0) return SYSTEM_STATUS;
    if (strcmp(name, "system") == 0) return SYSTEM_STATUS;
    if (strcmp(name, "airmouse") == 0) return AIRMOUSE;
    if (strcmp(name, "air_mouse") == 0) return AIRMOUSE;
    
    return NONE;
}

bool ChannelSubscriptions::handleMessage(int fd, const char* msg, size_t len, Channel* outChannel, bool* outIsSubscribe, bool* outChanged) {
    if (!msg || len == 0) return false;
    
    SYSTEM::SpiRamJsonDocument doc(LIMITS::API::JSON_DOC::CHANNEL_SUBSCRIPTIONS);
    DeserializationError error = deserializeJson(doc, msg, len);
    
    if (error || doc.overflowed()) {
        LOGW("WebSocket JSON parse failed: %s", doc.overflowed() ? "overflow" : error.c_str());
        return false;
    }
    
    bool isSubscribe = doc["subscribe"].is<const char*>();
    bool isUnsubscribe = doc["unsubscribe"].is<const char*>();
    
    if (!isSubscribe && !isUnsubscribe) {
        return false;
    }
    
    const char* channelName = isSubscribe ? doc["subscribe"].as<const char*>() : doc["unsubscribe"].as<const char*>();
    if (!channelName) return false;
    
    Channel ch = channelFromName(channelName);
    if (ch == NONE) {
        LOGW("Unknown channel: %s", channelName);
        return false;
    }
    
    const bool changed = isSubscribe ? subscribe(fd, ch) : unsubscribe(fd, ch);

    if (outChannel) *outChannel = ch;
    if (outIsSubscribe) *outIsSubscribe = isSubscribe;
    if (outChanged) *outChanged = changed;
    
    return true;
}

} // namespace API
