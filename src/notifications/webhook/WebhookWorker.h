#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <atomic>
#include "../../config/System.h"
class NotificationSettingsService;
#include "../runtime/WebhookTransportService.h"

namespace NOTIFICATIONS {

struct WebhookJob {
    // Fixed-capacity queue item stored directly in PSRAM. This trades a small,
    // predictable queue footprint for zero per-message heap churn while alerts
    // sit in the webhook queue.
    char payload[CONFIG::NOTIFICATIONS::WEBHOOK::MAX_PAYLOAD] = {0};
    uint16_t payloadLen = 0;
    // Retry count travels with the payload so the same alert can survive short
    // transport glitches without adding another worker or durable queue.
    uint8_t attemptCount = 0;
};

class WebhookWorker {
public:
    WebhookWorker(NotificationSettingsService* settings, WebhookTransportService* transport);
    ~WebhookWorker();
    
    bool enqueue(const char* payload, size_t len);

    /// Set the cancellation flag checked by the HTTP client during network I/O.
    void setCancelFlag(std::atomic<bool>* flag);
    
    // Statistics for diagnostics
    uint32_t getSentCount() const;
    uint32_t getFailCount() const;

    // Returns true if work was done (message processed)
    bool processOne();

    // Unified per-cycle hook used by NotificationWorker
    bool processCycle();

private:
    // Return the full dispatch result so processOne() can distinguish between
    // retryable transport failures and permanent/configuration failures.
    WebhookDispatchResult sendRequest(const char* url, const char* payload);

    NotificationSettingsService* _settings = nullptr;
    WebhookTransportService* _transport = nullptr;
    QueueHandle_t _queue = nullptr;
    // Queue payload storage is now explicit and fixed-size:
    // - payload bytes live inline in PSRAM queue slots,
    // - FreeRTOS queue control stays in internal DRAM,
    // - retries reuse the same slot contents instead of allocating another
    //   PsramString on every requeue.
    uint8_t* _queueStorage = nullptr;
    StaticQueue_t* _queueBuffer = nullptr;
    uint8_t _failStreak = 0;
    uint32_t _nextAllowedSendMs = 0;
    // static TaskHandle_t _taskHandle; // Removed
};

} // namespace NOTIFICATIONS
