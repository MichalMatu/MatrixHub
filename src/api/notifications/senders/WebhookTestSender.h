/**
 * @file WebhookTestSender.h
 * @brief Webhook test sender with synchronous delivery
 *
 * Sends test webhook messages directly via HTTPClient or TelegramClient (for HTTPS)
 * for immediate feedback.
 */

#pragma once

#include <Arduino.h>

class NotificationSettingsService;
namespace NOTIFICATIONS {
class WebhookTransportService;
}

namespace API {

struct WebhookSendTestResult {
    bool success = false;
    int httpCode = 0;
    const char* error = nullptr;
    char response[256] = {0};  // Response is usually ignored for webhooks but good to have
};

class WebhookTestSender {
public:
    WebhookTestSender(NotificationSettingsService* settingsService,
                      NOTIFICATIONS::WebhookTransportService* transport);

    bool isConfigured() const;

    /**
     * Send test webhook synchronous (blocks until done)
     * Returns real HTTP result
     */
    WebhookSendTestResult sendTest(const char* payload, size_t payloadLen);

private:
    NotificationSettingsService* _settingsService;
    NOTIFICATIONS::WebhookTransportService* _transport = nullptr;
};

}  // namespace API
