#include "PushoverWorker.h"
#include "PushoverSendValidator.h"
#include "../settings/NotificationChannelStateView.h"
#include <system/logging/Logging.h>
#include "../../system/rtc/RtcConfig.h"

#include <esp_heap_caps.h>

#undef LOG_TAG
#define LOG_TAG "Pushover"

namespace NOTIFICATIONS {

namespace {
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

bool isRetryableFailure(const PushoverResult& result) {
    if (result.success) {
        return false;
    }

    // Retry only transient delivery failures. Validation/configuration problems
    // are intentionally not retried because they will not heal by waiting.
    return strcmp(result.errorReason ? result.errorReason : "", "wifi_not_ready") == 0 ||
           strcmp(result.errorReason ? result.errorReason : "", "mutex_timeout") == 0 ||
           strcmp(result.errorReason ? result.errorReason : "", "cancelled") == 0 ||
           strcmp(result.errorReason ? result.errorReason : "", "send_failed") == 0;
}
} // namespace

PushoverWorker::PushoverWorker(NotificationSettingsService* settings, PushoverTransportService* transport)
    : _settings(settings), _transport(transport) {
    
    if (!_queue) {
        _queueStorage = (uint8_t*)heap_caps_malloc(CONFIG::NOTIFICATIONS::PUSHOVER::QUEUE_SIZE * sizeof(PushoverJob), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        _queueBuffer = (StaticQueue_t*)heap_caps_malloc(sizeof(StaticQueue_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        
        if (_queueStorage && _queueBuffer) {
            _queue = xQueueCreateStatic(CONFIG::NOTIFICATIONS::PUSHOVER::QUEUE_SIZE, sizeof(PushoverJob), _queueStorage, _queueBuffer);
            LOGI("Pushover queue created (size=%u, payload=%u bytes in PSRAM, control=%u bytes in DRAM)",
                 CONFIG::NOTIFICATIONS::PUSHOVER::QUEUE_SIZE,
                 CONFIG::NOTIFICATIONS::PUSHOVER::QUEUE_SIZE * sizeof(PushoverJob),
                 (unsigned)sizeof(StaticQueue_t));
        } else {
            LOGE("Failed to allocate Pushover queue buffers!");
            if (_queueStorage) heap_caps_free(_queueStorage);
            if (_queueBuffer) heap_caps_free(_queueBuffer);
            _queueStorage = nullptr;
            _queueBuffer = nullptr;
        }
    }
}

void PushoverWorker::setCancelFlag(std::atomic<bool>* flag) {
    if (_transport) {
        _transport->setCancelFlag(flag);
    }
}

PushoverWorker::~PushoverWorker() {
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

bool PushoverWorker::enqueue(const char* message) {
    if (!_queue) return false;
    
    size_t len = strlen(message);
    if (len >= CONFIG::NOTIFICATIONS::PUSHOVER::MAX_MESSAGE_LEN) {
        LOGE("Message too long: %u > %u", len, CONFIG::NOTIFICATIONS::PUSHOVER::MAX_MESSAGE_LEN);
        return false;
    }

    PushoverJob job;
    strlcpy(job.message, message, CONFIG::NOTIFICATIONS::PUSHOVER::MAX_MESSAGE_LEN);
    job.attemptCount = 0;

    if (xQueueSend(_queue, &job, TIMEOUT::QUEUE_NONBLOCK_TICKS) == pdTRUE) {
        LOGD("Enqueued to Pushover (len=%u, pending=%u)", (unsigned)len, (unsigned)uxQueueMessagesWaiting(_queue));
        return true;
    }
    
    LOGW("Queue full, dropping message (pending=%u)", (unsigned)uxQueueMessagesWaiting(_queue));
    return false;
}

bool PushoverWorker::processOne() {
    if (!_queue) {
        return false;
    }

    const uint32_t nowMs = millis();
    if (isDelayActive(nowMs, _nextAllowedSendMs)) {
        return false;
    }

    PushoverJob job;
    if (xQueueReceive(_queue, &job, TIMEOUT::QUEUE_NONBLOCK_TICKS) == pdTRUE) {
        LOGD("Processing Pushover message (pending=%u)", (unsigned)uxQueueMessagesWaiting(_queue));
        const auto channel = readPushoverChannelState(_settings);
        PUSHOVER::PushoverSendValidationInput input;
        input.settingsAvailable = channel.settingsAvailable;
        input.enabled = channel.enabled;
        input.configured = channel.configured;
        input.userKey = channel.userKey;
        input.apiToken = channel.apiToken;

        const auto validation = PUSHOVER::PushoverSendValidator::validate(input);
        if (!validation.ok) {
            _failStreak = 0;
            _nextAllowedSendMs = 0;
            return true;
        }

        const auto result = sendRequest(job.message, validation.userKey, validation.apiToken);
        if (result.success) {
            _failStreak = 0;
            const uint32_t completedAtMs = millis();
            if (uxQueueMessagesWaiting(_queue) > 0) {
                _nextAllowedSendMs = completedAtMs + CONFIG::NOTIFICATIONS::PUSHOVER::QUEUE_BREATHER_MS;
            } else {
                _nextAllowedSendMs = 0;
            }
        } else if (isRetryableFailure(result)) {
            if (_failStreak < 255) {
                _failStreak++;
            }

            // Requeue the same alert with a bounded retry budget. This keeps the
            // patch small while preventing one transient failure from silently
            // discarding the notification forever.
            const bool canRetry =
                job.attemptCount < CONFIG::NOTIFICATIONS::PUSHOVER::MAX_RETRY_ATTEMPTS;
            if (canRetry) {
                job.attemptCount++;
                if (xQueueSend(_queue, &job, TIMEOUT::QUEUE_NONBLOCK_TICKS) == pdTRUE) {
                    LOGW("Transient Pushover failure (%s), re-queued attempt %u/%u",
                         result.errorReason ? result.errorReason : "send_failed",
                         static_cast<unsigned>(job.attemptCount),
                         static_cast<unsigned>(CONFIG::NOTIFICATIONS::PUSHOVER::MAX_RETRY_ATTEMPTS));
                } else {
                    LOGE("Pushover retry requeue failed, dropping message");
                }
            } else {
                    LOGE("Pushover retry limit reached (%u), dropping message",
                         static_cast<unsigned>(job.attemptCount));
            }

            // Measure backoff from the end of the failed attempt so a long HTTP
            // timeout still produces a meaningful pause before the retry.
            const uint32_t completedAtMs = millis();
            const uint32_t delayMs = computeBackoffMs(
                _failStreak,
                CONFIG::NOTIFICATIONS::PUSHOVER::BACKOFF_BASE_MS,
                CONFIG::NOTIFICATIONS::PUSHOVER::BACKOFF_MAX_MS,
                CONFIG::NOTIFICATIONS::PUSHOVER::BACKOFF_MAX_EXPONENT);
            _nextAllowedSendMs = completedAtMs + delayMs;
        } else {
            _failStreak = 0;
            _nextAllowedSendMs = 0;
        }
        
        return true;
    }
    return false;
}

bool PushoverWorker::processCycle() {
    return processOne();
}

PushoverResult PushoverWorker::sendRequest(const char* message, const char* userKey, const char* apiToken) {
    LOGI("Sending message (%u chars)", strlen(message));
    
    LOG_PROFILE_START(startUs);
    if (!_transport) {
        PushoverResult unavailable;
        unavailable.success = false;
        unavailable.httpCode = -1;
        unavailable.errorReason = "transport_unavailable";
        return unavailable;
    }

    // Runtime worker and API test sender intentionally converge on one transport
    // path now. Mismatched behavior usually means the issue is in caller state or
    // validation, not in the HTTP/TLS implementation itself.
    auto res = _transport->dispatch(message, userKey, apiToken);
    LOG_PROFILE_END_SMART(startUs, "Pushover POST", TASK_MONITOR::INTERVAL_PUSHOVER_MS, TASK_MONITOR::THRESHOLD_NOTIF_POST_US);
    
    if (res.success) {
        RTC::runtimeStats.pushoverSent++;
        RTC::runtimeStats.pushoverLastSendMs = millis();
        RTC::runtimeStats.pushoverLastHttpCode = static_cast<int16_t>(res.httpCode);
        LOGD_THROTTLED(TASK_MONITOR::INTERVAL_PUSHOVER_MS,
                       "Success (total: %u)",
                       RTC::runtimeStats.pushoverSent);
        return res;
    } else {
        RTC::runtimeStats.pushoverFailed++;
        RTC::runtimeStats.pushoverLastSendMs = millis();
        RTC::runtimeStats.pushoverLastHttpCode = static_cast<int16_t>(res.httpCode);
        LOGE("Failed (fails: %u)", RTC::runtimeStats.pushoverFailed);
    }
    return res;
}

} // namespace NOTIFICATIONS
