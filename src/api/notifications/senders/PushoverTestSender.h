/**
 * @file PushoverTestSender.h
 * @brief Pushover test sender with synchronous delivery
 *
 * Sends test Pushover messages directly for immediate feedback.
 */

#pragma once

#include <Arduino.h>

class NotificationSettingsService;
namespace NOTIFICATIONS {
class PushoverTransportService;
}

namespace API {

struct PushoverSendTestResult {
    bool success = false;
    int httpCode = 0;
    const char* error = nullptr;
};

class PushoverTestSender {
public:
    PushoverTestSender(NotificationSettingsService* settingsService,
                       NOTIFICATIONS::PushoverTransportService* transport);

    bool isConfigured() const;

    /**
     * Send test Pushover message synchronously (blocks until done).
     */
    PushoverSendTestResult sendTest(const char* message, size_t messageLen);

private:
    NotificationSettingsService* _settingsService = nullptr;
    NOTIFICATIONS::PushoverTransportService* _transport = nullptr;
};

}  // namespace API
