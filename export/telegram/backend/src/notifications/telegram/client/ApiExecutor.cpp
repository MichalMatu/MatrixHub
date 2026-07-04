/**
 * @file ApiExecutor.cpp
 * @brief Telegram API execution layer implementation
 */

#include "ApiExecutor.h"
#include "TelegramClient.h"  // For SendResult/FetchResult definitions
#include "TelegramClientBuffers.h"
#include "../../webhook/JsonEscaper.h"  // For JSON string escaping
#include "../../../system/logging/Logging.h"

#include <WiFi.h>
#include <esp_heap_caps.h>

#undef LOG_TAG
#define LOG_TAG "TgApi"

namespace TELEGRAM {

namespace {

bool isCancelled(std::atomic<bool>* cancelFlag) {
    return cancelFlag && !cancelFlag->load(std::memory_order_acquire);
}

bool containsTelegramOkTrue(const char* buf, size_t len) {
    return buf && len > 0 && strstr(buf, "\"ok\":true") != nullptr;
}

bool containsTelegramOkFalse(const char* buf, size_t len) {
    return buf && len > 0 && strstr(buf, "\"ok\":false") != nullptr;
}

size_t readTelegramResponsePrefix(WiFiClient& stream, char* buf, size_t bufSize) {
    if (!buf || bufSize == 0) {
        return 0;
    }

    // Telegram replies are tiny and start with {"ok":...}. We only need a
    // small prefix to distinguish success from API-level failure, so prefer a
    // short polling loop over Stream::readBytes(). That avoids the noisy lower
    // level "Timeout waiting for data" warning that can appear even when the
    // HTTP request itself succeeded and the response body trickles in.
    constexpr uint32_t kPrefixReadBudgetMs = 250;
    constexpr uint32_t kPollDelayMs = 10;

    size_t len = 0;
    const uint32_t startMs = millis();

    while (len < (bufSize - 1) && (millis() - startMs) < kPrefixReadBudgetMs) {
        int available = stream.available();
        if (available <= 0) {
            if (len > 0) {
                break;
            }
            delay(kPollDelayMs);
            continue;
        }

        size_t chunk = static_cast<size_t>(available);
        const size_t remaining = (bufSize - 1) - len;
        if (chunk > remaining) {
            chunk = remaining;
        }

        const int readNow = stream.read(reinterpret_cast<uint8_t*>(buf + len), chunk);
        if (readNow <= 0) {
            break;
        }

        len += static_cast<size_t>(readNow);
        buf[len] = '\0';

        if (containsTelegramOkTrue(buf, len) || containsTelegramOkFalse(buf, len)) {
            break;
        }
    }

    return len;
}

}  // namespace

/**
 * Build sendMessage JSON payload without ArduinoJson (no heap allocation).
 * Format: {"chat_id":"<id>","text":"<escaped_text>"}
 * 
 * @return payload length, or 0 on error
 */
static size_t buildSendMessagePayload(const char* chatId, const char* text, 
                                       size_t textLen, char* buf, size_t bufSize) {
    // Header: {"chat_id":"
    // Middle: <chatId>","text":"
    // Escaped text
    // Footer: "}
    
    auto isSafeChatId = [](const char* id) -> bool {
        if (!id || id[0] == '\0') return false;
        size_t i = 0;
        if (id[0] == '-') i = 1;
        if (id[i] == '\0') return false;
        for (; id[i] != '\0'; i++) {
            if (id[i] < '0' || id[i] > '9') return false;
        }
        return true;
    };

    constexpr size_t kMinOverhead = 32;  // JSON structure overhead (without escaped expansion)
    if (!isSafeChatId(chatId) || !text || bufSize < kMinOverhead) {
        return 0;
    }
    
    size_t pos = 0;
    
    // Write header
    int written = snprintf(buf + pos, bufSize - pos, "{\"chat_id\":\"%s\",\"text\":\"", chatId);
    if (written < 0 || (size_t)written >= bufSize - pos) {
        return 0;
    }
    pos += written;
    
    // Escape and write text
    size_t escapedLen = NOTIFICATIONS::JsonEscaper::escape(text, textLen, buf + pos, bufSize - pos - 3);  // -3 for "}
    if (escapedLen == 0 && textLen > 0) {
        LOGE("Payload truncation or escape error: textLen=%u, bufferSpace=%u", (unsigned)textLen, (unsigned)(bufSize - pos));
        return 0;  // Escape failed
    }
    pos += escapedLen;
    
    // Write footer
    if (pos + 2 >= bufSize) {
        return 0;
    }
    buf[pos++] = '"';
    buf[pos++] = '}';
    buf[pos] = '\0';
    
    return pos;
}

SendResult ApiExecutor::sendMessage(const char* chatId, 
                                    const char* text, 
                                    size_t textLen,
                                    const TelegramRuntimeConfigView& config,
                                    TelegramClientBuffers& buffers,
                                    std::atomic<bool>* cancelFlag,
                                    TlsManager& tlsMgr,
                                    HTTPClient& http) {
    SendResult r;
    
    if (!config.settingsAvailable || !config.configured || config.botToken[0] == '\0') {
        r.error = "not_configured";
        LOGW("Send failed: not configured");
        return r;
    }

    if (isCancelled(cancelFlag)) {
        r.error = "cancelled";
        return r;
    }
    
    // Refactor note:
    // URL/payload assembly used to depend on file-static buffers in
    // TelegramClient.cpp. The behavior is unchanged, but the storage now comes
    // in explicitly from the owning TelegramClient.
    snprintf(buffers.url, sizeof(buffers.url), "https://%s/bot%s/sendMessage", kHost, config.botToken);
    
    // Build payload without ArduinoJson (avoids heap allocation)
    // Format: {"chat_id":"<id>","text":"<escaped_text>"}
    size_t payloadLen = buildSendMessagePayload(
        chatId, text, textLen, buffers.payload, sizeof(buffers.payload));
    
    // Validate payload size
    if (payloadLen == 0) {
        r.error = "payload_build_failed";
        LOGW("Send failed: payload build error");
        return r;
    }
    
    // Delegate after the request has been assembled in the borrowed scratch
    // bundle. This preserves the original zero-heap send path.
    r = performPostLocked(buffers.url,
                          buffers.payload,
                          payloadLen,
                          TlsMode::RootCA,
                          true,
                          cancelFlag,
                          tlsMgr,
                          http);
    
    return r;
}



SendResult ApiExecutor::postRequest(const char* url, 
                                    const char* payload,
                                    size_t payloadLen,
                                    bool verifyTls,
                                    std::atomic<bool>* cancelFlag,
                                    TlsManager& tlsMgr,
                                    HTTPClient& http) {
    SendResult result;
    result.success = false;
    
    if (!url || strlen(url) == 0) {
        result.error = "Empty URL";
        return result;
    }
    
    if (!payload || payloadLen == 0) {
        result.error = "Empty payload";
        return result;
    }

    if (isCancelled(cancelFlag)) {
        result.error = "cancelled";
        return result;
    }
    
    // Delegate to locked helper
    result = performPostLocked(url, payload, payloadLen, 
                              verifyTls ? TlsMode::RootCA : TlsMode::Insecure, 
                              false, // Webhooks typically don't reuse connection
                              cancelFlag,
                              tlsMgr, http);
    
    return result;
}

SendResult ApiExecutor::performPostLocked(const char* url, 
                                         const char* payload, 
                                         size_t payloadLen, 
                                         TlsMode tlsMode, 
                                         bool reuseConnection,
                                         std::atomic<bool>* cancelFlag,
                                         TlsManager& tlsMgr,
                                         HTTPClient& http) {
    SendResult r;
    
    // NOTE: Caller MUST hold lock!
    
    if (isCancelled(cancelFlag)) {
        r.error = "cancelled";
        return r;
    }

    if (!tlsMgr.connectIfNeeded(tlsMode)) {
        r.error = isCancelled(cancelFlag) ? "cancelled" : "tls_handshake_failed";
        return r;
    }

    auto& client = tlsMgr.getClient();
    const bool wasConnected = client.connected();
    
    if (!http.begin(tlsMgr.getClient(), url)) {
        r.error = "http_begin_failed";
        tlsMgr.cleanup(http);
        return r;
    }

    if (isCancelled(cancelFlag)) {
        r.error = "cancelled";
        tlsMgr.cleanup(http);
        return r;
    }
    
    http.setReuse(reuseConnection);
    http.setConnectTimeout(TIMEOUT::HTTP_CONNECT_MS);
    http.setTimeout(TIMEOUT::HTTP_READWRITE_MS);
    http.addHeader("Content-Type", "application/json");
    
    if (isCancelled(cancelFlag)) {
        r.error = "cancelled";
        tlsMgr.cleanup(http);
        return r;
    }

    r.httpCode = http.POST((uint8_t*)payload, payloadLen);

    if (isCancelled(cancelFlag)) {
        r.error = "cancelled";
        tlsMgr.cleanup(http);
        return r;
    }
    
    if (r.httpCode >= 200 && r.httpCode < 300) {
        // [FIX] Verify Telegram API level success ("ok": true)
        // Some errors can return 200 OK with an error body (e.g. rate limiting or invalid state)
        WiFiClient* stream = http.getStreamPtr();
        char peekBuf[128];
        size_t peekLen = 0;
        if (stream) {
            peekLen = readTelegramResponsePrefix(*stream, peekBuf, sizeof(peekBuf));
            if (peekLen > 0) {
                if (containsTelegramOkTrue(peekBuf, peekLen)) {
                    r.success = true;
                } else {
                    r.success = false;
                    r.error = "tg_api_error";
                    LOGW("Telegram API reports failure: %s", peekBuf);
                }
            } else {
                r.success = false;
                r.error = "empty_body";
            }
        } else {
            r.error = "stream_missing";
        }

        tlsMgr.drainResponse(http);
        // Keep TLS alive for Telegram (same host), full cleanup for webhooks
        if (reuseConnection) {
            tlsMgr.endRequest(http);
            tlsMgr.logConnEvent("POST(reuse)", wasConnected);
        } else {
            tlsMgr.cleanup(http);
            tlsMgr.logConnEvent("POST(cleanup)", wasConnected);
        }
    } else {
        r.error = isCancelled(cancelFlag)
                      ? "cancelled"
                      : ((r.httpCode > 0) ? "http_error" : "connection_error");
        LOGW("Request failed: %d", r.httpCode);
        tlsMgr.cleanup(http);  // Full reset on error
        tlsMgr.logConnEvent("POST(error_cleanup)", wasConnected);
    }
    
    return r;
}

}  // namespace TELEGRAM
