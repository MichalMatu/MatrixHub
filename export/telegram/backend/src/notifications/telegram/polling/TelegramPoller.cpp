/**
 * @file TelegramPoller.cpp
 * @brief Telegram command polling and dispatch handler implementation
 */

#include "TelegramPoller.h"
#include "../client/TelegramClient.h"
#include "../queue/MessageQueue.h"
#include "../runtime/TelegramWorker.h"
#include "../commands/TelegramCommandParser.h"
#include "../commands/TelegramCommandDispatcher.h"
#include "../../../system/logging/Logging.h"
#include "../../../system/health/heap/HeapMonitor.h"
#include "../../../config/App.h"
#include "../../../system/rtc/RtcConfig.h"

#include "TelegramUpdatesLite.h"
#include "../../../system/utils/ScopeLock.h"
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#undef LOG_TAG
#define LOG_TAG "TgPoller"

namespace TELEGRAM {

// Local aliases for readability
namespace {
    constexpr uint32_t NETWORK_SLOW_THRESHOLD_MS = APP::NOTIFICATIONS::TELEGRAM_NETWORK_SLOW_THRESHOLD_MS;
    constexpr int LONG_POLL_TIMEOUT_SEC = APP::NOTIFICATIONS::TELEGRAM_LONG_POLL_TIMEOUT_SEC;
    constexpr uint32_t FETCH_SLOW_THRESHOLD_US = NETWORK_SLOW_THRESHOLD_MS * 1000u;
}

bool TelegramPoller::isAuthorized(const char* chatId, const char* authorizedChatId) {
    return authorizedChatId && authorizedChatId[0] && strcmp(chatId, authorizedChatId) == 0;
}

void TelegramPoller::pollAndDispatch(int64_t& lastUpdateId, 
                                      WorkerStatus& status,
                                      const TelegramRuntimeConfigView& config,
                                      TelegramClient* client,
                                      SemaphoreHandle_t statusMutex,
                                      MessageQueue* outboundQueue,
                                      TelegramCommandRuntimeState& runtimeState,
                                      char* discoveredChatId,
                                      size_t discoveredChatIdCapacity) {
    // Long poll timeout is capped at 1s to prevent holding the Notification Mutex
    // for too long, ensuring fair access for WebhookWorker and other notification tasks.
    const int longPollTimeout = LONG_POLL_TIMEOUT_SEC;

    // Monitor heap before large operation
    auto heapBefore = SYSTEM::HeapMonitor::instance().getFreeHeap();
    auto largestBefore = SYSTEM::HeapMonitor::instance().getLargestFreeBlock();
    
    FetchResult fetchResult;
    char authorizedChatId[TelegramRuntimeConfigView::kChatIdCapacity] = {0};
    if (config.hasChatId) {
        strlcpy(authorizedChatId, config.chatId, sizeof(authorizedChatId));
    }
    if (discoveredChatId && discoveredChatIdCapacity > 0) {
        discoveredChatId[0] = '\0';
    }

    constexpr size_t kMaxUpdates = APP::NOTIFICATIONS::TELEGRAM_MAX_UPDATES_PER_POLL;
    UpdateLite liteUpdates[kMaxUpdates];
    size_t liteCount = 0;
    
    // =========================================================================
    // LOCKING: TelegramClient Mutex ensures thread-safety for internal buffers
    // =========================================================================
    
    constexpr TickType_t kMutexTimeout = TIMEOUT::MUTEX_NETWORK_TICKS;
    uint32_t lockStart = millis();

    struct Ctx {
        UpdateLite* arr;
        size_t max;
        size_t count;
        int64_t maxSeenUpdateId;
    } ctx { liteUpdates, kMaxUpdates, 0, lastUpdateId };

    auto cb = [](const UpdateLite& upd, void* user) -> bool {
        auto* ctx = static_cast<Ctx*>(user);
        if (!ctx) return false;
        if (upd.updateId > ctx->maxSeenUpdateId) {
            ctx->maxSeenUpdateId = upd.updateId;
        }
        if (ctx->count < ctx->max) {
            ctx->arr[ctx->count++] = upd;
        }
        return true;
    };

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

    LOG_PROFILE_START(tgFetch);
    if (!client->forEachUpdateLite(lastUpdateId, fetchResult, longPollTimeout, config, cb, &ctx)) {
        LOG_PROFILE_END_SMART(tgFetch, "Telegram fetch (error)", 0, FETCH_SLOW_THRESHOLD_US);
        const int httpCode = fetchResult.httpCode;
        const char* errMsg = fetchResult.error;
        withStatusLock([&]() {
            status.lastHttpCode = httpCode;
            if (errMsg) {
                strncpy(status.lastError, errMsg, sizeof(status.lastError) - 1);
                status.lastError[sizeof(status.lastError) - 1] = '\0';
            }
        });
        return;
    }
    liteCount = ctx.count;

    LOG_PROFILE_END_SMART(tgFetch, "Telegram fetch", TASK_MONITOR::INTERVAL_TELEGRAM_FETCH_MS, FETCH_SLOW_THRESHOLD_US);
    
    const int httpCode = fetchResult.httpCode;
    
    if (!fetchResult.success) {
        LOGW("Poll failed: http=%d err=%s", fetchResult.httpCode, fetchResult.error ? fetchResult.error : "none");
        const char* errMsg = fetchResult.error;
        withStatusLock([&]() {
            status.lastHttpCode = httpCode;
            if (errMsg) {
                strncpy(status.lastError, errMsg, sizeof(status.lastError) - 1);
                status.lastError[sizeof(status.lastError) - 1] = '\0';
            }
        });
        return;
    }
    
    // lastPollMs is now set by TelegramWorker BEFORE calling pollAndDispatch
    // to ensure the interval is always respected, even on failure.
    withStatusLock([&]() {
        status.lastHttpCode = httpCode;
        status.lastError[0] = '\0';
    });
    
    if (liteCount == 0) {
        LOGD("Poll OK, no new updates (lastId=%lld)", lastUpdateId);
        return;
    }

    LOGI("Received %u updates (lastId before=%lld)", (unsigned)liteCount, lastUpdateId);

    uint32_t processedDelta = 0;
    uint32_t commandsDelta = 0;
    auto& parsedMessage = runtimeState.parsedMessage;
    auto& commandContext = runtimeState.commandContext;
    LOG_PROFILE_START(tgDispatch);
    for (size_t i = 0; i < liteCount; i++) {
        const auto& upd = liteUpdates[i];
        if (!upd.hasMessage) continue;

        memset(&parsedMessage, 0, sizeof(parsedMessage));
        parsedMessage.updateId = upd.updateId;
        strncpy(parsedMessage.chatId, upd.chatId, sizeof(parsedMessage.chatId) - 1);
        strncpy(parsedMessage.text, upd.text, sizeof(parsedMessage.text) - 1);
        strncpy(parsedMessage.fromUsername, upd.fromUsername, sizeof(parsedMessage.fromUsername) - 1);
        parsedMessage.chatId[sizeof(parsedMessage.chatId) - 1] = '\0';
        parsedMessage.text[sizeof(parsedMessage.text) - 1] = '\0';
        parsedMessage.fromUsername[sizeof(parsedMessage.fromUsername) - 1] = '\0';
        parsedMessage.date = upd.date;

        Commands::TelegramCommandParser::parseCommand(parsedMessage);
        
        processedDelta++;
        
        // Accepted onboarding trade-off:
        // this project intentionally auto-binds the first chat when Telegram is
        // enabled but no chatId is configured yet. The owner uses Telegram as a
        // private single-user control channel and typically completes pairing
        // immediately after enabling the bot, so the hijack window is short and
        // considered low-risk for this product. Future audits should treat this
        // as a conscious onboarding compromise, not an accidental omission.
        if (!config.hasChatId && authorizedChatId[0] == '\0' && upd.chatId[0] != '\0') {
            strlcpy(authorizedChatId, upd.chatId, sizeof(authorizedChatId));
            if (discoveredChatId && discoveredChatIdCapacity > 0) {
                strlcpy(discoveredChatId, upd.chatId, discoveredChatIdCapacity);
            }
        }
        
        if (!isAuthorized(parsedMessage.chatId, authorizedChatId)) {
            LOGW("Unauthorized: %s", parsedMessage.chatId);
            continue;
        }
        
        // Use the explicit PSRAM-backed runtime state instead of hidden globals.
        if (Commands::TelegramCommandDispatcher::dispatch(parsedMessage, commandContext) &&
            commandContext.shouldReply && commandContext.responseLen > 0) {
            
            commandsDelta++;
            RTC::runtimeStats.telegramCmdsHandled++;
            
            // Queue response for sending
            if (outboundQueue) {
                outboundQueue->enqueue(
                    parsedMessage.chatId, commandContext.response, commandContext.responseLen);
            } else {
                LOGE("Telegram outbound queue unavailable, dropping command reply");
            }
        }
    }
    LOG_PROFILE_END_SMART(tgDispatch, "Telegram commands dispatch", TASK_MONITOR::INTERVAL_TELEGRAM_DISP_MS, TASK_MONITOR::THRESHOLD_TELEGRAM_US);

    if (processedDelta || commandsDelta) {
        withStatusLock([&]() {
            status.messagesProcessed += processedDelta;
            status.commandsExecuted += commandsDelta;
        });
    }

    // Commit the offset only after a full response has been parsed and walked.
    // If the stream breaks mid-parse, we keep the previous offset so Telegram
    // can redeliver those updates instead of silently dropping commands.
    if (ctx.maxSeenUpdateId > lastUpdateId) {
        lastUpdateId = ctx.maxSeenUpdateId;
    }
    
    // Log heap impact of this poll cycle
    auto heapAfter = SYSTEM::HeapMonitor::instance().getFreeHeap();
    auto largestAfter = SYSTEM::HeapMonitor::instance().getLargestFreeBlock();
    int32_t deltaHeap = (int32_t)heapAfter - (int32_t)heapBefore;
    
    // Log heap impact only for significant changes
    if (deltaHeap < -(int32_t)APP::NOTIFICATIONS::TELEGRAM_HEAP_DELTA_THRESHOLD || 
        deltaHeap > (int32_t)APP::NOTIFICATIONS::TELEGRAM_HEAP_DELTA_THRESHOLD) {
        LOGD("[Telegram] Heap: ΔFree=%+d (now: %u/%u)",
             deltaHeap, heapAfter, largestAfter);
    }
}

}  // namespace TELEGRAM
