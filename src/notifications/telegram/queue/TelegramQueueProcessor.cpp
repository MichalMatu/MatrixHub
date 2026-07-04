/**
 * @file TelegramQueueProcessor.cpp
 * @brief Telegram outbound message queue processor implementation
 */

#include "TelegramQueueProcessor.h"
#include "../client/TelegramClient.h"
#include "../runtime/TelegramRuntimeConfigView.h"
#include "MessageQueue.h"
#include "../runtime/TelegramWorker.h"
#include "../../../system/logging/Logging.h"
#include "../../../system/watchdog/TaskWatchdog.h"
#include "../../../system/utils/ScopeLock.h"
#include "../../../system/rtc/RtcConfig.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#undef LOG_TAG
#define LOG_TAG "TgQueue"

namespace TELEGRAM {

namespace {

bool isRetryableSendError(const char* error) {
    if (!error || error[0] == '\0') {
        return false;
    }

    return strcmp(error, "mutex_timeout") == 0 ||
           strcmp(error, "cancelled") == 0 ||
           strcmp(error, "tls_handshake_failed") == 0 ||
           strcmp(error, "http_begin_failed") == 0 ||
           strcmp(error, "http_error") == 0 ||
           strcmp(error, "connection_error") == 0 ||
           strcmp(error, "empty_body") == 0 ||
           strcmp(error, "stream_missing") == 0;
}

bool requeueMessageOrCountFailure(MessageQueue* queue,
                                  const OutboundMessage& msg,
                                  uint32_t& failedDelta) {
    if (queue && queue->enqueue(msg.chatId, msg.text)) {
        return true;
    }

    LOGE("CRITICAL: Queue full, message dropped! (ChatId: %s)", msg.chatId);
    failedDelta++;
    return false;
}

}  // namespace

OutboundProcessResult TelegramQueueProcessor::processOneQueuedMessage(WorkerStatus& status,
                                                                      TelegramClient* client,
                                                                      const TelegramRuntimeConfigView& config,
                                                                      SemaphoreHandle_t statusMutex,
                                                                      MessageQueue* queue) {
    auto withStatusLock = [&](auto&& fn) {
        if (!statusMutex) {
            fn();
            return true;
        }
        SYSTEM::ScopeLock lock(statusMutex, pdMS_TO_TICKS(50));
        if (!lock.isLocked()) {
            return false;
        }
        fn();
        return true;
    };

    OutboundMessage msg;

    const size_t queueSize = queue ? queue->count() : 0;
    if (queueSize == 0) {
        return {};
    }

    LOGI("Processing queued Telegram message (pending=%u)", (unsigned)queueSize);

    uint32_t sentDelta = 0;
    uint32_t failedDelta = 0;
    uint32_t lastSendMs = 0;
    const char* lastError = nullptr;
    bool didWork = false;
    bool applyQueueDelay = false;

    if (queue && queue->dequeue(msg)) {
        didWork = true;
        LOGI("Sending queued message to %s (len=%u)", msg.chatId, strlen(msg.text));

        LOG_PROFILE_START(tgSend);
        auto result = client->sendMessage(
            config,
            msg.chatId, msg.text, strlen(msg.text));
        LOG_PROFILE_END_IF_SLOW(tgSend, "Telegram sendMessage", TASK_MONITOR::THRESHOLD_TELEGRAM_POST_US);

        if (result.success) {
            sentDelta++;
            RTC::runtimeStats.telegramMsgsSent++;
            lastSendMs = millis();
            applyQueueDelay = queue && queue->count() > 0;
            LOGD_THROTTLED(TASK_MONITOR::INTERVAL_TELEGRAM_DISP_MS, "Sent OK");
        } else {
            const char* error = result.error ? result.error : "send_failed";
            lastError = error;

            // Network and transport errors are usually transient on ESP32, so
            // keep the alert/command reply in the queue instead of dropping it
            // after a single TLS/HTTP hiccup.
            if (isRetryableSendError(error)) {
                LOGW("Transient send failure (%s), re-queuing message...", error);
                const bool requeued = requeueMessageOrCountFailure(queue, msg, failedDelta);
                applyQueueDelay = requeued || (queue && queue->count() > 0);
            } else {
                LOGW("Permanent send failure (%s), dropping message", error);
                failedDelta++;
                applyQueueDelay = queue && queue->count() > 0;
            }
        }

        // Feed Watchdog in case of a large queue burst.
        SYSTEM::TaskWatchdog::instance().reset();
    }

    if (sentDelta || failedDelta || lastError) {
        withStatusLock([&]() {
            if (sentDelta) {
                status.messagesSent += sentDelta;
                status.lastSendMs = lastSendMs;
            }
            if (failedDelta) {
                status.messagesFailed += failedDelta;
            }
            if (lastError) {
                strncpy(status.lastError, lastError, sizeof(status.lastError) - 1);
                status.lastError[sizeof(status.lastError) - 1] = '\0';
            }
        });
    }

    return {.didWork = didWork, .applyQueueDelay = applyQueueDelay};
}

}  // namespace TELEGRAM
