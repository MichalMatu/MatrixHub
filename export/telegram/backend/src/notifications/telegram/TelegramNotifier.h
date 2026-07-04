/**
 * @file TelegramNotifier.h
 * @brief High-level interface for sending Telegram notifications
 * 
 * Queues messages for delivery via shared TelegramWorker.
 * Non-blocking - returns immediately after queuing.
 */

#pragma once

#include <Arduino.h>

class NotificationSettingsService;
namespace TELEGRAM {
class MessageQueue;
}

namespace NOTIFICATIONS {

struct TelegramSendResult {
    bool queued = false;      // Message was added to queue
    const char* error = nullptr;
};

class TelegramNotifier {
public:
    TelegramNotifier(NotificationSettingsService* settings, ::TELEGRAM::MessageQueue* queue);
    ~TelegramNotifier() = default;
    
    /**
     * Queue message for sending (non-blocking)
     * @return result indicating if message was queued
     */
    TelegramSendResult sendMessage(const char* text, size_t textLen);
    
    /** Check if Telegram is ready for outbound notifications */
    bool isConfigured() const;
    
    /** Check if notifications are enabled */
    bool isEnabled() const;

private:
    NotificationSettingsService* _settings = nullptr;
    ::TELEGRAM::MessageQueue* _queue = nullptr;
};

}  // namespace NOTIFICATIONS
