/**
 * @file ApiExecutor.h
 * @brief Telegram API execution layer
 * 
 * Extracted from TelegramClient.cpp (331 LOC → modular architecture)
 * Handles actual HTTP POST/GET operations for Telegram API.
 */

#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include "TlsManager.h"
#include "../runtime/TelegramRuntimeConfigView.h"
#include "../../../config/App.h"
#include <NetworkClient.h>
#include <atomic>

namespace TELEGRAM {

struct TelegramClientBuffers;

// Forward declarations (defined in TelegramClient.h)
struct SendResult;
struct FetchResult;

/**
 * @brief Executes Telegram API operations
 * 
 * Responsibilities:
 * - Send messages via sendMessage
 * - Stream updates via beginGetUpdatesStream (zero-heap)
 * - Generic POST requests (webhooks)
 * - Response parsing and error handling
 */
class ApiExecutor {
public:
    /**
     * @brief Send message to Telegram
     * @param chatId Target chat ID
     * @param text Message text
     * @param textLen Text length
     * @param config Runtime Telegram configuration snapshot
     * @param buffers Client-owned PSRAM scratch borrowed under TelegramClient mutex
     * @param tlsMgr TLS manager
     * @param http HTTP client
     * @return SendResult with success/error
     */
    static SendResult sendMessage(const char* chatId, 
                                  const char* text, 
                                  size_t textLen,
                                  const TelegramRuntimeConfigView& config,
                                  TelegramClientBuffers& buffers,
                                  std::atomic<bool>* cancelFlag,
                                  TlsManager& tlsMgr,
                                  HTTPClient& http);



    
    /**
     * @brief Generic HTTPS POST for webhooks
     * @param url Full URL (http:// or https://)
     * @param payload JSON payload
     * @param payloadLen Payload length
     * @param verifyTls true=use Root CA, false=skip verification
     * @param tlsMgr TLS manager
     * @param http HTTP client
     * @return SendResult with success/httpCode
     */
    static SendResult postRequest(const char* url, 
                                 const char* payload,
                                 size_t payloadLen,
                                 bool verifyTls,
                                 std::atomic<bool>* cancelFlag,
                                 TlsManager& tlsMgr,
                                 HTTPClient& http);

private:
    /**
     * @brief Internal locked POST helper
     * @note Caller MUST hold lock!
     */
    static SendResult performPostLocked(const char* url, 
                                       const char* payload,
                                       size_t payloadLen,
                                       TlsMode tlsMode,
                                       bool reuseConnection,
                                       std::atomic<bool>* cancelFlag,
                                       TlsManager& tlsMgr,
                                       HTTPClient& http);
    
    static constexpr const char* kHost = APP::NOTIFICATIONS::TELEGRAM_API_HOST;
};

}  // namespace TELEGRAM
