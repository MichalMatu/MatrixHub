#pragma once

#include <Arduino.h>
#include "../../config/System.h"
#include "../../system/memory/SystemAllocator.h"

class NotificationSettingsService;

namespace NOTIFICATIONS {

struct WebhookSendResult {
    bool queued = false;      // Message was added to queue
    const char* error = nullptr;
};

struct WebhookDispatchResult {
    bool success = false;
    int httpCode = 0;
    const char* error = nullptr;
    char response[CONFIG::NOTIFICATIONS::WEBHOOK::RESPONSE_PREVIEW_LEN] = {0}; // Small buffer for response preview
};

class WebhookWorker;

class WebhookNotifier {
public:
    WebhookNotifier(NotificationSettingsService* settings, WebhookWorker* worker);
    ~WebhookNotifier() = default;
    
    /**
     * Queue message for sending (non-blocking)
     * @return result indicating if message was queued
     */
    WebhookSendResult sendMessage(const char* payload, size_t len);

    /**
     * Queue a plain-text message, auto-wrapped into a JSON payload.
     * Uses Discord-compatible key when a Discord webhook URL is detected.
     */
    WebhookSendResult sendTextMessage(const char* message, size_t len);

    /** Check if Webhook is configured */
    bool isConfigured() const;
    
    /** Check if notifications are enabled */
    bool isEnabled() const;

private:
    struct TextPayloadScratch {
        char json[CONFIG::NOTIFICATIONS::WEBHOOK::MAX_PAYLOAD] = {0};
    };

    NotificationSettingsService* _settings = nullptr;
    WebhookWorker* _worker = nullptr;
    // Alarm text -> webhook JSON wrapping is the only production caller today.
    // Keep one reusable PSRAM scratch buffer for that path so normal alarm
    // traffic avoids one malloc/free pair per notification. If allocation ever
    // fails at boot, sendTextMessage() falls back to the old one-shot buffer.
    SYSTEM::MEMORY::PsramUniquePtr<TextPayloadScratch> _textScratch;
};

}  // namespace NOTIFICATIONS
