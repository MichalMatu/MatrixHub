/**
 * @file TelegramClient.cpp
 * @brief Single shared Telegram API client implementation
 */

#include "TelegramClient.h"
#include "ApiExecutor.h"
#include "UpdatesFetcher.h"
#include "../polling/TelegramUpdatesLiteParser.h"
#include "../../../system/logging/Logging.h"
#include "../../../system/utils/ScopeLock.h"

#include <WiFi.h>

#undef LOG_TAG
#define LOG_TAG "TgClient"

namespace TELEGRAM {

TelegramClient::TelegramClient() {
    _mutex = xSemaphoreCreateBinary();
    if (_mutex) {
        xSemaphoreGive(_mutex);
    } else {
        LOGE("Failed to create Telegram client mutex");
    }

    // Final Telegram cleanup: URL/payload scratch lives in PSRAM, but ownership
    // now follows the TelegramClient instance instead of a translation-unit
    // global. Helper modules borrow this bundle explicitly under the client
    // mutex, preserving the same zero-heap request path without hidden statics.
    _buffers = SYSTEM::MEMORY::makeUniqueInPsram<TelegramClientBuffers>();
    if (!_buffers) {
        LOGE("Failed to allocate Telegram client buffers in PSRAM");
    }
}

TelegramClient::~TelegramClient() {
    // ServiceRegistry stops the notification runtime before owned services are
    // destroyed, so cleanup here can be simple RAII instead of a full cancel
    // choreography. After the refactor there is no hidden file-static Telegram
    // transport state left that would outlive this object.
    _tlsMgr.cleanup(_http);
    if (_mutex) {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }
}

size_t TelegramClient::getRtcMemoryUsage() {
    // Buffers live in PSRAM; RTC usage is 0.
    return 0; 
}

size_t TelegramClient::getPsramMemoryUsage() {
    return sizeof(TelegramClientBuffers);
}

void TelegramClient::setCancelFlag(std::atomic<bool>* flag) {
    // TelegramClient stays as the transport facade, but ownership of "should I
    // still run?" now lives outside in NotificationWorker/ServiceRegistry.
    _cancelFlag = flag;
    _tlsMgr.setCancelFlag(flag);
}

bool TelegramClient::warmup(const TelegramRuntimeConfigView& config) {
    if (!_buffers) {
        return false;
    }

    if (_cancelFlag && !_cancelFlag->load(std::memory_order_acquire)) {
        return false;
    }

    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(APP::NOTIFICATIONS::TELEGRAM_MUTEX_TIMEOUT_MS));
    if (!lock.isLocked()) {
        return false;
    }
    
    // Warmup keeps the original behavior of preparing a reusable Telegram TLS
    // connection up front, but the URL scratch now comes from client-owned
    // PSRAM passed down explicitly to TlsManager.
    return _tlsMgr.warmup(config, *_buffers, _http);
}

SendResult TelegramClient::sendMessage(const TelegramRuntimeConfigView& config,
                                       const char* chatId,
                                       const char* text,
                                       size_t textLen) {
    return sendMessageInternal(config, chatId, text, textLen, _cancelFlag);
}

SendResult TelegramClient::sendMessageWithoutCancel(const TelegramRuntimeConfigView& config,
                                                    const char* chatId,
                                                    const char* text,
                                                    size_t textLen) {
    return sendMessageInternal(config, chatId, text, textLen, nullptr);
}

SendResult TelegramClient::sendMessageInternal(const TelegramRuntimeConfigView& config,
                                               const char* chatId,
                                               const char* text,
                                               size_t textLen,
                                               std::atomic<bool>* cancelOverride) {
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(APP::NOTIFICATIONS::TELEGRAM_MUTEX_TIMEOUT_MS));
    if (!lock.isLocked()) {
        SendResult r;
        r.error = "mutex_timeout";
        return r;
    }

    if (cancelOverride && !cancelOverride->load(std::memory_order_acquire)) {
        SendResult r;
        r.error = "cancelled";
        return r;
    }

    if (!_buffers) {
        SendResult r;
        r.error = "buffers_unavailable";
        return r;
    }
    
    // ApiExecutor now receives the scratch bundle explicitly, which makes it
    // clear in code review where request-building memory comes from.
    return ApiExecutor::sendMessage(
        chatId, text, textLen, config, *_buffers, cancelOverride, _tlsMgr, _http);
}

void TelegramClient::resetSession() {
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(APP::NOTIFICATIONS::TELEGRAM_MUTEX_TIMEOUT_MS));
    if (!lock.isLocked()) {
        LOGW("resetSession: mutex timeout");
        return;
    }

    _tlsMgr.cleanup(_http);
}

void TelegramClient::cancelAndReset() {
    // Best-effort cancel for blocking TLS/HTTP calls:
    // stop the socket immediately, then try to grab the client mutex and clean
    // up HTTPClient state. If the mutex is busy, the active caller is expected
    // to observe the closed socket/cancel flag and finish its own cleanup path.
    _tlsMgr.cancelActiveIo();

    SYSTEM::ScopeLock lock(_mutex, 0);
    if (!lock.isLocked()) {
        // Worth checking first when shutdown still looks slow: if this log
        // appears repeatedly, a caller is holding the Telegram client mutex for
        // too long and cleanup is being deferred to that active path.
        LOGW("cancelAndReset: mutex busy, cleanup deferred to active caller");
        return;
    }

    _tlsMgr.cleanup(_http);
}


SendResult TelegramClient::postRequest(const char* url, const char* payload, 
                                        size_t payloadLen, bool verifyTls) {
    SendResult result;
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(APP::NOTIFICATIONS::TELEGRAM_MUTEX_LONG_TIMEOUT_MS));
    if (!lock.isLocked()) {
        result.error = "Mutex timeout";
        return result;
    }

    if (_cancelFlag && !_cancelFlag->load(std::memory_order_acquire)) {
        result.error = "cancelled";
        return result;
    }
    
    return ApiExecutor::postRequest(url, payload, payloadLen, verifyTls, _cancelFlag, _tlsMgr, _http);
}

bool TelegramClient::forEachUpdateLite(int64_t lastUpdateId,
                                       FetchResult& result,
                                       int timeoutSec,
                                       const TelegramRuntimeConfigView& config,
                                       UpdateLiteCallback cb,
                                       void* user) {
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(APP::NOTIFICATIONS::TELEGRAM_MUTEX_TIMEOUT_MS));
    if (!lock.isLocked()) {
        result.error = "mutex_timeout";
        return false;
    }

    if (_cancelFlag && !_cancelFlag->load(std::memory_order_acquire)) {
        result.error = "cancelled";
        return false;
    }

    if (!_buffers) {
        result.error = "buffers_unavailable";
        return false;
    }

    // Long-poll remains serialized under the client mutex. The expected behavior
    // after the refactor is that stop() closes the socket underneath this call,
    // so even a blocked poll should unwind within the configured timeout window.
    // The polling helper also borrows the same client-owned scratch bundle, so
    // this path no longer depends on a hidden translation-unit URL buffer.
    NetworkClient* stream = UpdatesFetcher::beginStream(
        lastUpdateId,
        result,
        timeoutSec,
        config,
        *_buffers,
        _cancelFlag,
        _tlsMgr,
        _http);
    // Note: result.httpCode and result.success are updated inside beginStream

    if (!stream) {
        // Special case: long-poll read timeout is expected and means "no updates".
        // ApiExecutor already ended the HTTP request and kept TLS alive.
        if (result.success && result.httpCode == HTTPC_ERROR_READ_TIMEOUT) {
            return true;
        }
        return false;
    }

    if (_cancelFlag && !_cancelFlag->load(std::memory_order_acquire)) {
        result.success = false;
        result.error = "cancelled";
        _tlsMgr.cleanup(_http);
        return false;
    }

    // Step 2: Parse stream under lock. 
    // Holding the lock for the duration of the long-poll is safer because the 
    // HTTPClient instance is shared and not thread-safe.
    bool telegramOk = false;
    const bool parseOk = TELEGRAM::TelegramUpdatesLiteParser::parse(*stream, cb, user, telegramOk);

    if (!parseOk) {
        result.success = false;
        result.error = "json_parse_error";
        _tlsMgr.cleanup(_http);
        return false;
    }

    result.success = telegramOk;
    if (!telegramOk) {
        result.error = "telegram_error";
        _tlsMgr.cleanup(_http);
        return true;
    }

    result.error = nullptr;
    _tlsMgr.endRequest(_http);
    
    return true;
}

}  // namespace TELEGRAM
