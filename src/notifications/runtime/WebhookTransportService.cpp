#include "WebhookTransportService.h"

#include "../../config/System.h"
#include "../../system/logging/Logging.h"
#include "../../system/network/NetworkReadiness.h"
#include "../../system/utils/ScopeLock.h"
#include "../../system/watchdog/TaskWatchdog.h"

#include <cstring>

#undef LOG_TAG
#define LOG_TAG "WebhookTx"

namespace NOTIFICATIONS {

WebhookTransportService::WebhookTransportService(SemaphoreHandle_t concurrencyGuard)
    : _concurrencyGuard(concurrencyGuard)
    , _client(nullptr, true) {}

void WebhookTransportService::setCancelFlag(std::atomic<bool>* flag) {
    // March 2026 refactor:
    // publish only the default runtime token here. The in-flight request chooses
    // its token later via postWithContext(), so test traffic cannot accidentally
    // rewrite cancellation behavior for an already-running runtime request.
    _cancelFlag.store(flag, std::memory_order_release);
}

void WebhookTransportService::cancelAndReset() {
    // stop() may call this while another task still owns the transport mutex.
    // Use the non-blocking socket abort path here; the next serialized request
    // will finish the deferred HTTPClient cleanup inside UnifiedHttpClient.
    _client.cancelActiveIo();
}

WebhookDispatchResult WebhookTransportService::dispatch(const char* url, const char* payload) {
    return dispatchInternal(url, payload, _cancelFlag.load(std::memory_order_acquire));
}

WebhookDispatchResult WebhookTransportService::dispatchWithoutCancel(const char* url,
                                                                    const char* payload) {
    return dispatchInternal(url, payload, nullptr);
}

WebhookDispatchResult WebhookTransportService::dispatchInternal(const char* url,
                                                               const char* payload,
                                                               std::atomic<bool>* cancelOverride) {
    WebhookDispatchResult result;

    if (!url || strlen(url) == 0) {
        result.error = "empty_url";
        return result;
    }

    if (!payload) {
        result.error = "empty_payload";
        return result;
    }

    if (!NETWORK::isWifiReadyForHttp()) {
        result.error = "wifi_not_ready";
        return result;
    }

    constexpr TickType_t kMutexTimeout =
        pdMS_TO_TICKS(CONFIG::NOTIFICATIONS::WEBHOOK::MUTEX_TIMEOUT_MS);

    // Target behavior after the refactor:
    // one shared client is still serialized through one mutex domain, but the
    // cancel-token decision is now request-local before the POST starts.
    // The lock still serializes webhook/test traffic through one client on
    // purpose. If something "hangs", check whether the symptom is actually a
    // mutex timeout caused by another in-flight notification rather than the HTTP
    // stack itself being wedged.
    SYSTEM::ScopeLock lock(_concurrencyGuard, kMutexTimeout);
    if (_concurrencyGuard && !lock.isLocked()) {
        result.error = "notification_mutex_timeout";
        return result;
    }

    const bool wdtEnabled = SYSTEM::TaskWatchdog::instance().isInitialized();
    if (wdtEnabled) {
        SYSTEM::TaskWatchdog::instance().reset();
    }

    const size_t payloadLen = strlen(payload);
    int httpCode = 0;
    // Important part of the fix: pass the cancel flag as request context instead
    // of mutating shared client state before taking the lock.
    if (_client.postWithContext(url,
                                payload,
                                payloadLen,
                                nullptr,
                                0,
                                CONFIG::NOTIFICATIONS::WEBHOOK::HTTP_TIMEOUT_MS,
                                &httpCode,
                                cancelOverride)) {
        result.success = true;
        result.httpCode = httpCode;
    } else {
        result.httpCode = httpCode;
        // Preserve more specific failure semantics:
        // - cancelled: runtime stop/reconfigure interrupted the request,
        // - http_error: the peer answered with a real non-2xx status,
        // - send_failed: lower-level transport or handshake failure.
        result.error = (cancelOverride && !cancelOverride->load(std::memory_order_acquire))
                           ? "cancelled"
                           : (httpCode > 0 ? "http_error" : "send_failed");
    }

    if (wdtEnabled) {
        SYSTEM::TaskWatchdog::instance().reset();
    }
    return result;
}

}  // namespace NOTIFICATIONS
