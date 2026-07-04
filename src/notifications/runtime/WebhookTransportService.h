#pragma once

#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../webhook/WebhookNotifier.h"
#include "../../system/network/UnifiedHttpClient.h"

namespace NOTIFICATIONS {

/**
 * Registry-owned webhook transport shared by the runtime worker and API test sender.
 * This removes the old hidden singleton client from notifier code and makes
 * ownership, cancellation, and reset behavior explicit.
 *
 * March 2026 note:
 * the transport still keeps one shared HTTP client for DRAM reasons, but request
 * cancellation is now selected per call instead of by mutating shared client state.
 */
class WebhookTransportService {
public:
    explicit WebhookTransportService(SemaphoreHandle_t concurrencyGuard);

    void setCancelFlag(std::atomic<bool>* flag);
    void cancelAndReset();

    WebhookDispatchResult dispatch(const char* url, const char* payload);

    // Test sends intentionally bypass the runtime cancel token so a manual
    // diagnostics request is not aborted just because the background worker is
    // currently stopping or being reconfigured.
    WebhookDispatchResult dispatchWithoutCancel(const char* url, const char* payload);

private:
    WebhookDispatchResult dispatchInternal(const char* url,
                                          const char* payload,
                                          std::atomic<bool>* cancelOverride);

    SemaphoreHandle_t _concurrencyGuard = nullptr;
    // Default runtime token published by NotificationWorker. Manual tests pass
    // their own request-local override (or nullptr) when they want different behavior.
    std::atomic<std::atomic<bool>*> _cancelFlag{nullptr};
    NETWORK::UnifiedHttpClient _client;
};

}  // namespace NOTIFICATIONS
