/**
 * @file TelegramClient.h
 * @brief Single shared Telegram API client
 * 
 * Thread-safe HTTP client for all Telegram operations:
 * - sendMessage() for outbound messages
 * - forEachUpdateLite() for polling commands (streaming, zero-heap)
 * 
 * Uses ONE persistent TLS connection (~46KB) shared across all operations.
 */

#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <atomic>
#include "../polling/TelegramUpdatesLite.h"
#include "../runtime/TelegramRuntimeConfigView.h"
#include "../../../system/memory/SystemAllocator.h"
#include "TelegramClientBuffers.h"
#include "TlsManager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace TELEGRAM {

struct SendResult {
    bool success = false;
    int httpCode = 0;
    const char* error = nullptr;
};

struct FetchResult {
    bool success = false;
    int httpCode = 0;
    const char* error = nullptr;
};

class TelegramClient {
public:
    TelegramClient();
    ~TelegramClient();
    
    TelegramClient(const TelegramClient&) = delete;
    TelegramClient& operator=(const TelegramClient&) = delete;

    /** Warmup TLS connection (call after WiFi connect) */
    bool warmup(const TelegramRuntimeConfigView& config);

    void setCancelFlag(std::atomic<bool>* flag);
    
    /**
     * Send message to Telegram.
     *
     * Refactor note:
     * The call still uses one shared synchronous transport path. What changed is
     * only ownership of the helpers behind it: URL/payload scratch and TLS
     * state are now explicit TelegramClient members instead of hidden statics.
     */
    SendResult sendMessage(const TelegramRuntimeConfigView& config,
                           const char* chatId,
                           const char* text,
                           size_t textLen);

    // Manual/API tests use the same serialized Telegram transport, but they do
    // not inherit the runtime worker cancel token. This mirrors the
    // Webhook/Pushover test behavior: diagnostics should not fail solely
    // because NotificationWorker is being reconfigured in the background.
    SendResult sendMessageWithoutCancel(const TelegramRuntimeConfigView& config,
                                        const char* chatId,
                                        const char* text,
                                        size_t textLen);

    /** Reset the active HTTP/TLS session so the next operation reconnects cleanly. */
    void resetSession();
    void cancelAndReset();

    /**
     * Fetch updates (streaming parser, no heap).
     * Calls cb for each update found in result[].
     */
    bool forEachUpdateLite(int64_t lastUpdateId,
                           FetchResult& result,
                           int timeoutSec,
                           const TelegramRuntimeConfigView& config,
                           UpdateLiteCallback cb,
                           void* user);
    
    /**
     * Generic HTTPS POST for webhooks (shares TLS client)
     * @param url Full URL (http:// or https://)
     * @param payload JSON payload
     * @param payloadLen Payload length
     * @param verifyTls true=use Root CA, false=skip verification
     * @return SendResult with success/httpCode
     */
    SendResult postRequest(const char* url, const char* payload, 
                           size_t payloadLen, bool verifyTls = false);

    /** Get RTC memory usage for static buffers (0; buffers are in PSRAM) */
    static size_t getRtcMemoryUsage();
    /** Get PSRAM usage for client-owned transport scratch buffers. */
    static size_t getPsramMemoryUsage();

private:
    SendResult sendMessageInternal(const TelegramRuntimeConfigView& config,
                                   const char* chatId,
                                   const char* text,
                                   size_t textLen,
                                   std::atomic<bool>* cancelOverride);

    // Final Telegram cleanup:
    // - removed file-static EXT_RAM_BSS_ATTR transport buffers
    // - removed function-local static TlsManager
    // - preserved the original "one shared serialized Telegram transport" model
    //
    // After the refactor ServiceRegistry still owns one TelegramClient, but the
    // client now owns all mutable transport state directly via RAII.
    TlsManager _tlsMgr;
    SYSTEM::MEMORY::PsramUniquePtr<TelegramClientBuffers> _buffers;
    HTTPClient _http;
    SemaphoreHandle_t _mutex = nullptr;
    std::atomic<bool>* _cancelFlag = nullptr;
};

}  // namespace TELEGRAM
