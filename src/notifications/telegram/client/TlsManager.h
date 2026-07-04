/**
 * @file TlsManager.h
 * @brief Telegram TLS connection lifecycle manager
 * 
 * Extracted from TelegramClient.cpp (331 LOC → modular architecture)
 * Handles TLS mode switching, warmup, and cleanup.
 */

#pragma once

#include <atomic>
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include "../../../config/App.h"
#include "../runtime/TelegramRuntimeConfigView.h"

namespace TELEGRAM {

struct TelegramClientBuffers;

/**
 * TLS configuration mode for NetworkClientSecure
 */
enum class TlsMode {
    NotConfigured,  // Initial state / after reset
    RootCA,         // Telegram API (Root CA verification)
    Insecure        // Webhooks (skip certificate verification)
};

/**
 * @brief Manages TLS connection lifecycle
 * 
 * Responsibilities:
 * - Switch between TLS modes (RootCA / Insecure)
 * - Reset TLS connection
 * - Warmup TLS for optimal performance
 * - Clean up HTTP/TLS resources
 */
class TlsManager {
public:
    TlsManager();
    
    /**
     * @brief Reset TLS connection
     */
    void resetTls();
    
    /**
     * @brief Ensure specific TLS mode is active
     * @param required Required TLS mode
     */
    void ensureTlsMode(TlsMode required);
    
    /**
     * @brief Get current TLS mode
     */
    TlsMode getCurrentMode() const { return _currentTlsMode; }

    // The cancel flag belongs to the runtime owner (NotificationWorker). TlsManager
    // only reads it to short-circuit connect/warmup and to support best-effort
    // shutdown of in-flight network work.
    void setCancelFlag(std::atomic<bool>* flag);

    // Force-close the active socket from the task that owns the TelegramClient
    // mutex. Do not call this out-of-band while another task may be inside
    // NetworkClientSecure/mbedTLS; that stack is not safe to tear down from a
    // concurrent shutdown task during handshake.
    void cancelActiveIo();
    
    /**
     * @brief Warmup TLS connection
     * @param config Runtime Telegram configuration snapshot
     * @param buffers Client-owned PSRAM scratch buffers
     *
     * Refactor note:
     * Warmup no longer relies on any hidden URL scratch at file scope. The
     * owning TelegramClient passes request scratch explicitly so this path keeps
     * the same zero-heap behavior without global mutable transport data.
     * @param http HTTP client reference
     * @return true if warmup successful
     */
    bool warmup(const TelegramRuntimeConfigView& config,
                TelegramClientBuffers& buffers,
                HTTPClient& http);
    
    /**
     * @brief Full cleanup - HTTP end + TLS reset
     * Use for: errors, connection failures, webhook (different hosts)
     * @param http HTTP client reference
     */
    void cleanup(HTTPClient& http);
    
    /**
     * @brief End HTTP request but keep TLS connection alive
     * Use for: Telegram API (same host, polling every 10s)
     * Reduces fragmentation by avoiding repeated TLS alloc/free
     * @param http HTTP client reference
     */
    void endRequest(HTTPClient& http);
    
    /**
     * @brief Drain HTTP response body
     * @param http HTTP client reference
     */
    void drainResponse(HTTPClient& http);
    
    /**
     * @brief Get TLS client reference
     */
    NetworkClientSecure& getClient() { return _client; }

    /**
     * @brief Log TLS connection event with telemetry
     * @param op Operation name/tag
     * @param wasConnected Whether client was connected before operation
     */
    void logConnEvent(const char* op, bool wasConnected);
    
    bool connectIfNeeded(TlsMode requiredMode);

private:
    bool isCancelled() const;

    NetworkClientSecure _client;
    TlsMode _currentTlsMode = TlsMode::NotConfigured;
    std::atomic<bool>* _cancelFlag = nullptr;

    
    static constexpr const char* kHost = APP::NOTIFICATIONS::TELEGRAM_API_HOST;
    // Trigger cleanup if largest free block drops below this (indicates fragmentation)
    static constexpr size_t kMinSafeBlockSize = 32768; // 32KB
};

}  // namespace TELEGRAM
