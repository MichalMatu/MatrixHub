#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <atomic>
#include "../../config/System.h"
#include "../runtime/PushoverTransportService.h"

class NotificationSettingsService;

namespace NOTIFICATIONS {

struct PushoverJob {
    char message[CONFIG::NOTIFICATIONS::PUSHOVER::MAX_MESSAGE_LEN];
    // Retry count stays inside the queued job so we can requeue the same alert
    // after a transient failure without changing the broader worker design.
    uint8_t attemptCount = 0;
};

class PushoverWorker {
public:
    PushoverWorker(NotificationSettingsService* settings, PushoverTransportService* transport);
    ~PushoverWorker();
    
    bool enqueue(const char* message);

    /// Set the cancellation flag checked by the HTTP client during network I/O.
    void setCancelFlag(std::atomic<bool>* flag);
    
    // Returns true if work was done (message processed)
    bool processOne();

    // Unified per-cycle hook used by NotificationWorker
    bool processCycle();

private:
    // Return structured status so the caller can keep retry logic local to the
    // worker instead of treating every failure as an immediate permanent drop.
    PushoverResult sendRequest(const char* message, const char* userKey, const char* apiToken);

    NotificationSettingsService* _settings = nullptr;
    PushoverTransportService* _transport = nullptr;
    QueueHandle_t _queue = nullptr;
    uint8_t* _queueStorage = nullptr;
    StaticQueue_t* _queueBuffer = nullptr;
    uint8_t _failStreak = 0;
    uint32_t _nextAllowedSendMs = 0;
};

} // namespace NOTIFICATIONS
