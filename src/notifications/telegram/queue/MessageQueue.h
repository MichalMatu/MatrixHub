/**
 * @file MessageQueue.h
 * @brief Thread-safe outbound message queue for Telegram
 *
 * Registry-owned queue used by:
 * - TelegramNotifier for alarm/user notifications
 * - TelegramWorker for direct enqueue helpers
 * - TelegramPoller for command replies
 *
 * The queue keeps message storage in PSRAM, but the queue object itself now has
 * an explicit owner and lifecycle instead of relying on hidden process-wide
 * statics.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace TELEGRAM {

/** Maximum text length per message (Telegram limit is 4096) */
constexpr size_t kMaxMessageLen = 1024; // Increased for PSRAM (was 768)
constexpr size_t kMaxChatIdLen = 24;    
constexpr size_t kQueueCapacity = 20;   // Increased capacity in PSRAM (was 10)

/**
 * Fixed-size message structure (no dynamic allocation)
 */
struct OutboundMessage {
    char chatId[kMaxChatIdLen];
    char text[kMaxMessageLen];
};

/**
 * Thread-safe message queue with explicit lifecycle.
 */
class MessageQueue {
public:
    MessageQueue();
    ~MessageQueue();

    MessageQueue(const MessageQueue&) = delete;
    MessageQueue& operator=(const MessageQueue&) = delete;

    bool isReady() const;

    /**
     * Add message to queue
     * @param chatId Target chat ID
     * @param text Message text (will be truncated if too long)
     * @return true if queued, false if queue full
     */
    bool enqueue(const char* chatId, const char* text);
    
    /**
     * Add message with explicit length
     */
    bool enqueue(const char* chatId, const char* text, size_t textLen);
    
    /**
     * Remove message from front of queue
     * @param msg Output: the dequeued message
     * @return true if message retrieved, false if queue empty
     */
    bool dequeue(OutboundMessage& msg);
    
    /**
     * Check if queue is empty
     */
    bool isEmpty();
    
    /**
     * Get number of pending messages
     */
    size_t count();
    
    /**
     * Clear all pending messages
     */
    void clear();

private:
    SemaphoreHandle_t _mutex = nullptr;
    OutboundMessage* _buffer = nullptr;
    uint8_t _head = 0;
    uint8_t _tail = 0;
    uint8_t _count = 0;
};

}  // namespace TELEGRAM
