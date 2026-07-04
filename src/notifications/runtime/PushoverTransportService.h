#pragma once

#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../pushover/PushoverNotifier.h"
#include "../../system/network/UnifiedHttpClient.h"

namespace NOTIFICATIONS {

/**
 * Registry-owned Pushover transport shared by the runtime worker and API test sender.
 * The expected behavior is identical on both paths: one long-lived HTTP client,
 * one mutex domain, and one explicit reset point during shutdown.
 *
 * March 2026 note:
 * request cancellation is intentionally no longer configured by mutating the
 * shared client ahead of time; each send selects its own cancel token.
 */
class PushoverTransportService {
public:
    explicit PushoverTransportService(SemaphoreHandle_t concurrencyGuard);
    ~PushoverTransportService();

    void setCancelFlag(std::atomic<bool>* flag);
    void cancelAndReset();

    PushoverResult dispatch(const char* message, const char* userKey, const char* apiToken);

    // Test sends opt out of the runtime cancel token so diagnostics can still
    // run while the background notification runtime is being stopped.
    PushoverResult dispatchWithoutCancel(const char* message,
                                         const char* userKey,
                                         const char* apiToken);

private:
    PushoverResult dispatchInternal(const char* message,
                                    const char* userKey,
                                    const char* apiToken,
                                    std::atomic<bool>* cancelOverride);

    SemaphoreHandle_t _concurrencyGuard = nullptr;
    // Default runtime token published by NotificationWorker. Diagnostics may
    // override it per request to keep manual feedback deterministic.
    std::atomic<std::atomic<bool>*> _cancelFlag{nullptr};
    NETWORK::UnifiedHttpClient _client;
    // Shared PSRAM scratch reused by every serialized request. This keeps the
    // transport in the same memory model as Telegram: one long-lived buffer
    // instead of one malloc/free churn per outbound message.
    char* _payloadScratch = nullptr;
};

}  // namespace NOTIFICATIONS
