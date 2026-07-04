#pragma once

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace API {

class TelegramTestSender;
class WebhookTestSender;
class PushoverTestSender;

enum class NotificationTestScheduleStatus : uint8_t {
    Queued = 0,
    ServiceUnavailable,
    Busy,
    RateLimited,
    AllocFailed,
    TaskCreateFailed,
};

struct NotificationTestScheduleResult {
    NotificationTestScheduleStatus status = NotificationTestScheduleStatus::ServiceUnavailable;
    uint32_t retryAfterMs = 0;

    bool isQueued() const {
        return status == NotificationTestScheduleStatus::Queued;
    }
};

class NotificationTestJobScheduler {
public:
    // Test jobs run as detached background tasks. The scheduler now owns the
    // lifetime contract for their per-channel semaphores too, so teardown waits
    // for in-flight jobs before API-owned senders/transports can disappear.
    ~NotificationTestJobScheduler();

    bool begin();

    // Idempotent explicit shutdown hook for restart/deep-sleep paths.
    // Destruction still calls this as a final safety net, but production
    // shutdown should prefer invoking it before WiFi is torn down so detached
    // test tasks can finish while the network stack is still alive.
    void shutdown();

    NotificationTestScheduleResult scheduleTelegram(const char* text, TelegramTestSender* sender);
    NotificationTestScheduleResult scheduleWebhook(const char* payload, WebhookTestSender* sender);
    NotificationTestScheduleResult schedulePushover(const char* message, PushoverTestSender* sender);

private:
    SemaphoreHandle_t _telegramSemaphore = nullptr;
    SemaphoreHandle_t _webhookSemaphore = nullptr;
    SemaphoreHandle_t _pushoverSemaphore = nullptr;
};

}  // namespace API
