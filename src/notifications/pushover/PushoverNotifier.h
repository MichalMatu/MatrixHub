#pragma once

#include <Arduino.h>

class NotificationSettingsService;

namespace NOTIFICATIONS {

struct PushoverSendResult {
    bool queued = false;
    const char* error = nullptr;
};

struct PushoverResult {
    bool success = false;
    int httpCode = 0;
    const char* errorReason = nullptr;
};

class PushoverWorker;

class PushoverNotifier {
public:
    PushoverNotifier(NotificationSettingsService* settings, PushoverWorker* worker);
    ~PushoverNotifier() = default;

    PushoverSendResult sendMessage(const char* message, size_t len);
    bool isConfigured() const;
    bool isEnabled() const;

private:
    NotificationSettingsService* _settings = nullptr;
    PushoverWorker* _worker = nullptr;
};

} // namespace NOTIFICATIONS
