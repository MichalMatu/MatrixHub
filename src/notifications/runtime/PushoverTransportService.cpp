#include "PushoverTransportService.h"

#include "../pushover/PushoverTlsConfig.h"
#include "../webhook/JsonEscaper.h"
#include "../../config/System.h"
#include "../../system/logging/Logging.h"
#include "../../system/network/NetworkReadiness.h"
#include "../../system/utils/ScopeLock.h"

#include <esp_heap_caps.h>

#undef LOG_TAG
#define LOG_TAG "PushoverTx"

namespace NOTIFICATIONS {

namespace {

constexpr const char* kPushoverApiUrl = "https://api.pushover.net/1/messages.json";
constexpr size_t kTokenPrefixLen = sizeof("{\"token\":\"") - 1;
constexpr size_t kUserFragmentLen = sizeof("\",\"user\":\"") - 1;
constexpr size_t kMessageFragmentLen = sizeof("\",\"message\":\"") - 1;
constexpr size_t kJsonSuffixLen = sizeof("\"}") - 1;
constexpr size_t kMaxPushoverMessageLen =
    CONFIG::NOTIFICATIONS::PUSHOVER::MAX_MESSAGE_LEN - 1;
constexpr size_t kMaxEscapedMessageLen = kMaxPushoverMessageLen * 2;
// Capacity planning for the reusable PSRAM scratch buffer:
// - user key / API token are fixed-size secrets from RTC config (31 chars max),
// - message may grow during JSON escaping,
// - the transport is fully serialized by one mutex, so one buffer per
//   transport instance is enough for both runtime sends and API test sends.
constexpr size_t kPushoverPayloadScratchLen =
    kTokenPrefixLen + 31 +
    kUserFragmentLen + 31 +
    kMessageFragmentLen + kMaxEscapedMessageLen +
    kJsonSuffixLen + 1;

}  // namespace

PushoverTransportService::PushoverTransportService(SemaphoreHandle_t concurrencyGuard)
    : _concurrencyGuard(concurrencyGuard)
    , _client(nullptr, false) {
    _client.setRootCA(PushoverTlsConfig::getRootCaPem());
    // March 2026 memory cleanup:
    // Pushover used to allocate/free one PSRAM payload buffer per request.
    // Keep one long-lived scratch buffer instead so repeated notifications do
    // not churn PSRAM and fragment the heap under bursty alert traffic.
    _payloadScratch = static_cast<char*>(
        heap_caps_malloc(kPushoverPayloadScratchLen, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!_payloadScratch) {
        LOGE("OOM (PSRAM): failed to allocate %u-byte Pushover scratch buffer",
             static_cast<unsigned>(kPushoverPayloadScratchLen));
    }
}

PushoverTransportService::~PushoverTransportService() {
    if (_payloadScratch) {
        heap_caps_free(_payloadScratch);
        _payloadScratch = nullptr;
    }
}

void PushoverTransportService::setCancelFlag(std::atomic<bool>* flag) {
    // Same intent as WebhookTransportService: store the default runtime token
    // only. The actual request-local behavior is selected inside dispatchInternal().
    _cancelFlag.store(flag, std::memory_order_release);
}

void PushoverTransportService::cancelAndReset() {
    // Same shutdown rule as WebhookTransportService: abort raw I/O immediately
    // without touching HTTPClient state outside the transport mutex.
    _client.cancelActiveIo();
}

PushoverResult PushoverTransportService::dispatch(const char* message,
                                                 const char* userKey,
                                                 const char* apiToken) {
    return dispatchInternal(
        message, userKey, apiToken, _cancelFlag.load(std::memory_order_acquire));
}

PushoverResult PushoverTransportService::dispatchWithoutCancel(const char* message,
                                                              const char* userKey,
                                                              const char* apiToken) {
    return dispatchInternal(message, userKey, apiToken, nullptr);
}

PushoverResult PushoverTransportService::dispatchInternal(const char* message,
                                                         const char* userKey,
                                                         const char* apiToken,
                                                         std::atomic<bool>* cancelOverride) {
    PushoverResult result;

    if (!message || !userKey || !apiToken) {
        result.errorReason = "miss arg";
        return result;
    }

    if (!NETWORK::isWifiReadyForHttp()) {
        result.errorReason = "wifi_not_ready";
        return result;
    }

    const size_t messageRawLen = strlen(message);
    const size_t escapedMessageLen = JsonEscaper::getEscapedLength(message);
    const size_t totalLen =
        kTokenPrefixLen + strlen(apiToken) +
        kUserFragmentLen + strlen(userKey) +
        kMessageFragmentLen + escapedMessageLen +
        kJsonSuffixLen;

    if (!_payloadScratch) {
        result.errorReason = "oom";
        return result;
    }

    // Guard the static scratch size explicitly. If this ever trips in the
    // field, inspect config lengths / escaping first rather than assuming the
    // HTTP layer is corrupting memory.
    if (totalLen + 1 > kPushoverPayloadScratchLen) {
        LOGE("Pushover payload too large for scratch buffer: %u > %u",
             static_cast<unsigned>(totalLen + 1),
             static_cast<unsigned>(kPushoverPayloadScratchLen));
        result.errorReason = "oom";
        return result;
    }

    size_t pos = 0;
    pos += snprintf(_payloadScratch + pos,
                    totalLen + 1 - pos,
                    "{\"token\":\"%s\",\"user\":\"%s\",\"message\":\"",
                    apiToken,
                    userKey);
    pos += JsonEscaper::escape(message, messageRawLen, _payloadScratch + pos, totalLen + 1 - pos);
    pos += snprintf(_payloadScratch + pos, totalLen + 1 - pos, "\"}");

    const TickType_t mutexTimeout =
        pdMS_TO_TICKS(CONFIG::NOTIFICATIONS::PUSHOVER::MUTEX_TIMEOUT_MS);

    // Target behavior after the refactor:
    // Pushover keeps one shared client + one serialization domain, but manual
    // tests no longer interfere with cancellation semantics of runtime traffic.
    // Pushover now shares the same serialization model as Webhook: one explicit
    // transport object and one lock domain. If delivery looks flaky, inspect
    // whether requests are timing out on this mutex before blaming TLS.
    SYSTEM::ScopeLock lock(_concurrencyGuard, mutexTimeout);
    if (_concurrencyGuard && !lock.isLocked()) {
        result.errorReason = "mutex_timeout";
        return result;
    }

    int httpCode = 0;
    // Request-scoped cancel selection fixes the old race where a test request
    // could temporarily replace the client-wide cancel flag for another caller.
    // The mutex above serializes every caller through one transport instance,
    // so one shared scratch buffer is enough and avoids hot-path PSRAM churn.
    if (_client.postWithContext(kPushoverApiUrl,
                                _payloadScratch,
                                pos,
                                nullptr,
                                0,
                                CONFIG::NOTIFICATIONS::PUSHOVER::HTTP_TIMEOUT_MS,
                                &httpCode,
                                cancelOverride)) {
        result.success = true;
        result.httpCode = httpCode;
    } else {
        result.success = false;
        result.httpCode = httpCode;
        // Preserve the distinction between a clean HTTP rejection and a lower
        // level transport failure so retries/diagnostics can evolve separately.
        result.errorReason = (cancelOverride && !cancelOverride->load(std::memory_order_acquire))
                                 ? "cancelled"
                                 : (httpCode > 0 ? "http_error" : "send_failed");
    }
    return result;
}

}  // namespace NOTIFICATIONS
