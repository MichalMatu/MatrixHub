#include "WebhookWorker.h"
#include "WebhookSendValidator.h"
#include "../settings/NotificationChannelStateView.h"
#include <WiFi.h>
#include <system/logging/Logging.h>
#include "../../system/rtc/RtcConfig.h"
#include "../../config/System.h"
#include <esp_heap_caps.h>

#undef LOG_TAG
#define LOG_TAG "WebHook"

namespace NOTIFICATIONS {

namespace {
constexpr size_t kHttpsPrefixLen = sizeof("https") - 1;

uint32_t computeBackoffMs(uint8_t failStreak,
                          uint32_t baseMs,
                          uint32_t maxMs,
                          uint8_t maxExponent) {
    if (failStreak == 0) {
        return 0;
    }
    uint8_t exp = static_cast<uint8_t>(failStreak - 1);
    if (exp > maxExponent) {
        exp = maxExponent;
    }
    uint32_t delay = baseMs << exp;
    if (delay < baseMs || delay > maxMs) {
        delay = maxMs;
    }
    return delay;
}

bool isDelayActive(uint32_t nowMs, uint32_t untilMs) {
    return untilMs != 0 && static_cast<int32_t>(nowMs - untilMs) < 0;
}

bool isRetryableFailure(const WebhookDispatchResult& result) {
    if (result.success) {
        return false;
    }

    // Retry only failures that can realistically clear on their own on the next
    // pass. Configuration errors and HTTP-level rejections are treated as final.
    return strcmp(result.error ? result.error : "", "wifi_not_ready") == 0 ||
           strcmp(result.error ? result.error : "", "notification_mutex_timeout") == 0 ||
           strcmp(result.error ? result.error : "", "cancelled") == 0 ||
           strcmp(result.error ? result.error : "", "send_failed") == 0;
}
} // namespace

WebhookWorker::WebhookWorker(NotificationSettingsService* settings, WebhookTransportService* transport)
    : _settings(settings), _transport(transport) {
    
    if (!_queue) {
        // March 2026 memory cleanup:
        // Webhook used to queue heap-allocated PsramString payloads, which
        // meant every enqueue/retry produced another small PSRAM allocation.
        // Move that payload storage into one fixed-capacity queue arena so
        // queue pressure is predictable and fragmentation is lower.
        _queueStorage = static_cast<uint8_t*>(heap_caps_malloc(
            CONFIG::NOTIFICATIONS::WEBHOOK::QUEUE_SIZE * sizeof(WebhookJob),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        _queueBuffer = static_cast<StaticQueue_t*>(heap_caps_malloc(
            sizeof(StaticQueue_t),
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

        if (_queueStorage && _queueBuffer) {
            _queue = xQueueCreateStatic(
                CONFIG::NOTIFICATIONS::WEBHOOK::QUEUE_SIZE,
                sizeof(WebhookJob),
                _queueStorage,
                _queueBuffer);
            LOGI("Queue created (size=%u, payload=%u bytes in PSRAM, control=%u bytes in DRAM)",
                 CONFIG::NOTIFICATIONS::WEBHOOK::QUEUE_SIZE,
                 static_cast<unsigned>(CONFIG::NOTIFICATIONS::WEBHOOK::QUEUE_SIZE * sizeof(WebhookJob)),
                 static_cast<unsigned>(sizeof(StaticQueue_t)));
        } else {
            LOGE("Failed to allocate Webhook queue buffers!");
            if (_queueStorage) {
                heap_caps_free(_queueStorage);
                _queueStorage = nullptr;
            }
            if (_queueBuffer) {
                heap_caps_free(_queueBuffer);
                _queueBuffer = nullptr;
            }
        }
    }
    // No xTaskCreate here anymore
}

void WebhookWorker::setCancelFlag(std::atomic<bool>* flag) {
    if (_transport) {
        _transport->setCancelFlag(flag);
    }
}

WebhookWorker::~WebhookWorker() {
    if (_queue) {
        vQueueDelete(_queue);
        _queue = nullptr;
    }

    if (_queueStorage) {
        heap_caps_free(_queueStorage);
        _queueStorage = nullptr;
    }

    if (_queueBuffer) {
        heap_caps_free(_queueBuffer);
        _queueBuffer = nullptr;
    }
}

bool WebhookWorker::enqueue(const char* payload, size_t len) {
    if (!_queue) {
        LOGW("Queue not initialized");
        return false;
    }
    
    if (len >= CONFIG::NOTIFICATIONS::WEBHOOK::MAX_PAYLOAD) {
        LOGE("Payload too large: %u > %u", (unsigned)len, (unsigned)CONFIG::NOTIFICATIONS::WEBHOOK::MAX_PAYLOAD);
        return false;
    }

    // With fixed queue storage there is no per-message heap allocation left on
    // this path, so a full queue is just a backpressure signal instead of a
    // fragmenting allocation storm.
    if (uxQueueSpacesAvailable(_queue) == 0) {
        LOGW("Queue full, dropping message without allocation");
        return false;
    }

    WebhookJob job;
    // Payload bytes are copied into the fixed PSRAM slot immediately, so this
    // queue no longer depends on the caller keeping any temporary String/buffer
    // alive after enqueue() returns.
    memcpy(job.payload, payload, len);
    job.payload[len] = '\0';
    job.payloadLen = static_cast<uint16_t>(len);
    job.attemptCount = 0;

    if (xQueueSend(_queue, &job, TIMEOUT::QUEUE_NONBLOCK_TICKS) == pdTRUE) {
        LOGD("Enqueued %u bytes to fixed PSRAM queue, depth: %u",
             (unsigned)len, (unsigned)uxQueueMessagesWaiting(_queue));
        return true;
    }
    
    LOGW("Queue full (race condition), dropping message");
    return false;
}

bool WebhookWorker::processOne() {
    if (!_queue) {
        return false;
    }

    const uint32_t nowMs = millis();
    if (isDelayActive(nowMs, _nextAllowedSendMs)) {
        return false;
    }

    WebhookJob job;
    // Non-blocking check
    if (xQueueReceive(_queue, &job, TIMEOUT::QUEUE_NONBLOCK_TICKS) == pdTRUE) {
        const auto channel = readWebhookChannelState(_settings);
        WEBHOOK::WebhookSendValidationInput input;
        input.settingsAvailable = channel.settingsAvailable;
        input.enabled = channel.enabled;
        input.configured = channel.configured;
        input.url = channel.url;
        input.payloadLen = job.payloadLen;

        const auto validation = WEBHOOK::WebhookSendValidator::validate(input);
        if (!validation.ok) {
            LOGW("Not configured or wrong mode, dropping");
            _failStreak = 0;
            _nextAllowedSendMs = 0;
        } else {
            // Retries now requeue the same inline payload bytes. When debugging
            // memory growth, this path should no longer create additional PSRAM
            // objects; only queue depth and transport-level buffers should move.
            const auto result = sendRequest(validation.url, job.payload);
            if (result.success) {
                _failStreak = 0;
                const uint32_t completedAtMs = millis();
                if (uxQueueMessagesWaiting(_queue) > 0) {
                    _nextAllowedSendMs = completedAtMs + CONFIG::NOTIFICATIONS::WEBHOOK::QUEUE_BREATHER_MS;
                } else {
                    _nextAllowedSendMs = 0;
                }
            } else if (isRetryableFailure(result)) {
                if (_failStreak < 255) {
                    _failStreak++;
                }

                // The queue now stores the whole payload inline, so retries keep
                // using the same fixed-capacity PSRAM slot instead of allocating
                // another PsramString for each requeue.
                const bool canRetry =
                    job.attemptCount < CONFIG::NOTIFICATIONS::WEBHOOK::MAX_RETRY_ATTEMPTS;
                if (canRetry) {
                    job.attemptCount++;
                    if (xQueueSend(_queue, &job, TIMEOUT::QUEUE_NONBLOCK_TICKS) == pdTRUE) {
                        LOGW("Transient webhook failure (%s), re-queued attempt %u/%u",
                             result.error ? result.error : "send_failed",
                             static_cast<unsigned>(job.attemptCount),
                             static_cast<unsigned>(CONFIG::NOTIFICATIONS::WEBHOOK::MAX_RETRY_ATTEMPTS));
                    } else {
                        LOGE("Webhook retry requeue failed, dropping message");
                    }
                } else {
                        LOGE("Webhook retry limit reached (%u), dropping message",
                             static_cast<unsigned>(job.attemptCount));
                }

                // Base the next attempt on when the failed request finished,
                // not when it started, so a long timeout still yields a real
                // cooldown before the retry can run.
                const uint32_t completedAtMs = millis();
                const uint32_t delayMs = computeBackoffMs(
                    _failStreak,
                    CONFIG::NOTIFICATIONS::WEBHOOK::BACKOFF_BASE_MS,
                    CONFIG::NOTIFICATIONS::WEBHOOK::BACKOFF_MAX_MS,
                    CONFIG::NOTIFICATIONS::WEBHOOK::BACKOFF_MAX_EXPONENT);
                _nextAllowedSendMs = completedAtMs + delayMs;
            } else {
                _failStreak = 0;
                _nextAllowedSendMs = 0;
            }
        }
        return true; // Did work
    }
    return false; // Idle
}

bool WebhookWorker::processCycle() {
    return processOne();
}

WebhookDispatchResult WebhookWorker::sendRequest(const char* url, const char* payload) {
    size_t payloadLen = strlen(payload);
    bool isSecure = strncmp(url, "https", kHttpsPrefixLen) == 0;
    
    LOGI("POST %s (%u bytes)", isSecure ? "[HTTPS]" : "[HTTP]", payloadLen);

    LOG_PROFILE_START(startUs);
    if (!_transport) {
        WebhookDispatchResult unavailable;
        unavailable.error = "transport_unavailable";
        unavailable.httpCode = -1;
        return unavailable;
    }

    // Runtime worker and API test sender now share the exact same transport
    // implementation. If one path works and the other does not, debug above this
    // layer first (settings/queueing/caller state) before touching HTTP code.
    auto result = _transport->dispatch(url, payload);
    LOG_PROFILE_END_SMART(startUs, "Webhook POST", TASK_MONITOR::INTERVAL_WEBHOOK_MS, TASK_MONITOR::THRESHOLD_NOTIF_POST_US);

    if (result.success) {
        RTC::runtimeStats.webhookSent++;
        RTC::runtimeStats.webhookLastSendMs = millis();
        RTC::runtimeStats.webhookLastHttpCode = static_cast<int16_t>(result.httpCode);
        LOGD_THROTTLED(TASK_MONITOR::INTERVAL_WEBHOOK_MS,
                       "Success: %d (total: %u)",
                       result.httpCode,
                       RTC::runtimeStats.webhookSent);
        return result;
    } else {
        RTC::runtimeStats.webhookFailed++;
        RTC::runtimeStats.webhookLastSendMs = millis();
        RTC::runtimeStats.webhookLastHttpCode = static_cast<int16_t>(result.httpCode);
        if (result.error) {
             LOGE("Failed: %s (fails: %u)", result.error, RTC::runtimeStats.webhookFailed);
        } else {
             LOGE("Failed: HTTP %d (fails: %u)", result.httpCode, RTC::runtimeStats.webhookFailed);
        }
    }
    return result;
}

uint32_t WebhookWorker::getSentCount() const { return RTC::runtimeStats.webhookSent; }
uint32_t WebhookWorker::getFailCount() const { return RTC::runtimeStats.webhookFailed; }

} // namespace NOTIFICATIONS
