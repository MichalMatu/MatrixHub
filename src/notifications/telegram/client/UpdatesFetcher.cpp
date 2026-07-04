/**
 * @file UpdatesFetcher.cpp
 * @brief Telegram updates fetcher implementation
 */

#include "UpdatesFetcher.h"
#include "TelegramClientBuffers.h"
#include "../../../system/logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "TgFetch"

namespace TELEGRAM {

constexpr const char* kHost = "api.telegram.org";

namespace {

bool isCancelled(std::atomic<bool>* cancelFlag) {
    return cancelFlag && !cancelFlag->load(std::memory_order_acquire);
}

}  // namespace

NetworkClient* UpdatesFetcher::beginStream(int64_t lastUpdateId,
                                          FetchResult& result,
                                          int timeoutSec,
                                          const TelegramRuntimeConfigView& config,
                                          TelegramClientBuffers& buffers,
                                          std::atomic<bool>* cancelFlag,
                                          TlsManager& tlsMgr,
                                          HTTPClient& http) {
    result = FetchResult{};

    if (!config.settingsAvailable || !config.configured || config.botToken[0] == '\0') {
        result.error = "not_configured";
        return nullptr;
    }

    if (isCancelled(cancelFlag)) {
        result.error = "cancelled";
        return nullptr;
    }

    if (!tlsMgr.connectIfNeeded(TlsMode::RootCA)) {
        result.error = isCancelled(cancelFlag) ? "cancelled" : "tls_handshake_failed";
        return nullptr;
    }

    auto& client = tlsMgr.getClient();
    const bool wasConnected = client.connected();

    // Build URL with parameters into the client-owned scratch buffer.
    // Before the cleanup this path reached into a hidden file-static URL
    // buffer; now the dependency is explicit and still serialized by
    // TelegramClient::_mutex.
    snprintf(buffers.url, sizeof(buffers.url),
             "https://%s/bot%s/getUpdates?offset=%lld&timeout=%d&limit=5",
             kHost, config.botToken,
             lastUpdateId > 0 ? lastUpdateId + 1 : 0,
             timeoutSec);

    if (!http.begin(tlsMgr.getClient(), buffers.url)) {
        result.error = "http_begin_failed";
        tlsMgr.cleanup(http);
        tlsMgr.logConnEvent("GET(begin_fail_cleanup)", wasConnected);
        return nullptr;
    }

    if (isCancelled(cancelFlag)) {
        result.error = "cancelled";
        tlsMgr.cleanup(http);
        tlsMgr.logConnEvent("GET(cancel_cleanup)", wasConnected);
        return nullptr;
    }

    http.setReuse(true);
    // Keep this close to the long-poll timeout to avoid holding the network mutex too long.
    // (timeoutSec is already capped in TelegramPoller)
    http.setConnectTimeout(3000);
    http.setTimeout((timeoutSec * 1000) + 1500);

    result.httpCode = http.GET();

    if (isCancelled(cancelFlag)) {
        result.error = "cancelled";
        tlsMgr.cleanup(http);
        tlsMgr.logConnEvent("GET(cancel_after_call)", wasConnected);
        return nullptr;
    }

    // For long polling, a read timeout is expected and simply means "no updates".
    // endRequest() will check if connection is still alive:
    // - If alive: keep for reuse
    // - If closed by server: call http.end() to reset HTTPClient (but keep TLS mode)
    if (result.httpCode == HTTPC_ERROR_READ_TIMEOUT) {
        result.success = true;
        result.error = nullptr;
        tlsMgr.endRequest(http);  // Handles both alive and closed connections
        tlsMgr.logConnEvent("GET(timeout)", wasConnected);
        return nullptr;
    }

    if (result.httpCode < 200 || result.httpCode >= 300) {
        result.error = (result.httpCode > 0) ? "http_error" : "connection_error";
        tlsMgr.cleanup(http);
        tlsMgr.logConnEvent("GET(http_fail_cleanup)", wasConnected);
        return nullptr;
    }

    // Successful request; connection may be newly opened here.
    tlsMgr.logConnEvent("GET(ok_stream)", wasConnected);

    auto* stream = http.getStreamPtr();
    if (!stream) {
        result.error = "no_stream";
        tlsMgr.cleanup(http);
        tlsMgr.logConnEvent("GET(no_stream_cleanup)", wasConnected);
        return nullptr;
    }

    if (isCancelled(cancelFlag)) {
        result.error = "cancelled";
        tlsMgr.cleanup(http);
        tlsMgr.logConnEvent("GET(cancel_before_stream_cleanup)", wasConnected);
        return nullptr;
    }

    // NOTE: do not set result.success here; caller will set based on parsed telegram ok.
    return stream;
}

} // namespace TELEGRAM
