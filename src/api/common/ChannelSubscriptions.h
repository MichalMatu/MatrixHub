#pragma once

#include "../../system/memory/PsramAllocator.h"
#include <array>
#include <atomic>
#include <map>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_http_server.h>

namespace API { // [Restored]
class WebSocketBroadcaster;

/**
 * @brief Manages WebSocket channel subscriptions with bitmask-based tracking.
 * 
 * Zero-alloc in hot-path: uses uint8_t bitmask per client instead of set/vector.
 * Thread-safe via FreeRTOS mutex.
 */
class ChannelSubscriptions {
public:
    // Channel identifiers (bitmask)
    enum Channel : uint16_t {
        NONE    = 0x0000,
        SHELLY  = 0x0001,
        SENSING = 0x0002,
        ALARMS  = 0x0004,
        POWER   = 0x0008,
        BLE     = 0x0010,
        TELEGRAM = 0x0020,
        NOTIF_STATS = 0x0040,
        TELEMETRY = 0x0080,
        MACROS = 0x0100,
        SYSTEM_STATUS = 0x0200,
        AIRMOUSE = 0x0400,
        // Add future channels here...
    };

    ChannelSubscriptions();
    ~ChannelSubscriptions();

    /**
     * @brief Subscribe a client to a channel.
     * @param fd Client socket file descriptor
     * @param ch Channel to subscribe to
     * @return true if the subscription state changed
     */
    bool subscribe(int fd, Channel ch);

    /**
     * @brief Unsubscribe a client from a channel.
     * @param fd Client socket file descriptor
     * @param ch Channel to unsubscribe from
     * @return true if the subscription state changed
     */
    bool unsubscribe(int fd, Channel ch);

    /**
     * @brief Remove client from all channels (call on disconnect).
     * @param fd Client socket file descriptor
     */
    void unsubscribeAll(int fd);

    /**
     * @brief Check if any client is subscribed to a channel.
     * O(1) via cached per-channel subscriber counts.
     * @param ch Channel to check
     * @return true if at least one subscriber exists
     */
    bool hasSubscribers(Channel ch) const;

    /**
     * @brief Get count of subscribers for a channel.
     * @param ch Channel to count
     * @return Number of subscribed clients
     */
    size_t getSubscriberCount(Channel ch) const;

    /**
     * @brief Broadcast binary data to all subscribers of a channel.
     * @param broadcaster AsyncTask-enabled broadcaster
     * @param serverHandle HTTP server handle for sending
     * @param ch Target channel
     * @param data Binary payload
     * @param len Payload length
     */
    void broadcast(WebSocketBroadcaster* broadcaster, httpd_handle_t serverHandle, Channel ch, 
                   const uint8_t* data, size_t len);

    /**
     * @brief Parse subscribe/unsubscribe message from client.
     * Expected format: {"subscribe":"shelly"} or {"unsubscribe":"shelly"}
     * @param fd Client socket
     * @param msg JSON message string
     * @param len Message length
     * @param outChanged Set to true only when the subscription mask changed
     * @return true if message was handled (subscribe/unsubscribe)
     */
    bool handleMessage(int fd, const char* msg, size_t len, Channel* outChannel = nullptr, bool* outIsSubscribe = nullptr, bool* outChanged = nullptr);

    /**
     * @brief Convert channel name string to enum.
     * @param name Channel name (e.g., "shelly", "sensing")
     * @return Channel enum value, or NONE if unknown
     */
    static Channel channelFromName(const char* name);

private:
    static constexpr size_t CHANNEL_COUNT = 11;

    // fd -> subscribed channels bitmask (in PSRAM)
    using PsramSubscriptionMap = std::map<int, uint16_t, std::less<int>, SYSTEM::PsramAllocator<std::pair<const int, uint16_t>>>;
    PsramSubscriptionMap _subscriptions;
    std::array<std::atomic<size_t>, CHANNEL_COUNT> _subscriberCounts{};
    mutable SemaphoreHandle_t _lock;
    
    static constexpr uint8_t MAX_SEND_FAILURES = 10;
    
    static constexpr size_t MAX_BROADCAST_TARGETS = 32;  // Match WebSocketBroadcaster

    static constexpr bool isSingleChannel(Channel ch) {
        const uint16_t raw = static_cast<uint16_t>(ch);
        return raw != 0 && (raw & (raw - 1)) == 0;
    }

    static constexpr size_t channelIndex(Channel ch) {
        uint16_t raw = static_cast<uint16_t>(ch);
        size_t index = 0;
        while (raw > 1) {
            raw >>= 1;
            ++index;
        }
        return index;
    }

    static constexpr size_t invalidChannelIndex() {
        return CHANNEL_COUNT;
    }

    size_t getCachedSubscriberCount(Channel ch) const;
    void incrementSubscriberCountLocked(Channel ch);
    void decrementSubscriberCountLocked(Channel ch);
    void decrementMaskCountsLocked(uint16_t mask);
};

} // namespace API
