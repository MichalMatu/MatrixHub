/**
 * @file TelegramTestSender.h
 * @brief Telegram test sender with synchronous delivery
 *
 * Sends test messages directly via TelegramClient for immediate feedback.
 */

#pragma once

#include <Arduino.h>

class NotificationSettingsService;
namespace TELEGRAM {
class TelegramClient;
class TelegramWorker;
}

namespace API {

struct SendTestResult {
    bool success = false;
    int httpCode = 0;
    const char* error = nullptr;
    char response[256] = {0};  // Telegram API response
};

class TelegramTestSender {
public:
    TelegramTestSender(NotificationSettingsService* settingsService,
                       TELEGRAM::TelegramClient* client,
                       TELEGRAM::TelegramWorker* worker);

    bool isConfigured() const;

    /**
     * Send test message synchronously (blocks until done)
     * Returns real HTTP result from Telegram API
     */
    SendTestResult sendTest(const char* text, size_t textLen);

private:
    NotificationSettingsService* _settingsService;
    TELEGRAM::TelegramClient* _client = nullptr;
    TELEGRAM::TelegramWorker* _worker = nullptr;
};

}  // namespace API
