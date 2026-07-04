#pragma once

/**
 * @file UnifiedHttpClient.h
 * @brief Generic HTTP/HTTPS client with connection reuse and retry logic
 * 
 * Features:
 * - HTTP and HTTPS support (WiFiClient / WiFiClientSecure)
 * - Keep-Alive connection reuse
 * - Retry with exponential backoff
 * - Configurable timeouts
 * - Cancellation support via atomic flag
 * - NO String allocations (caller-provided buffers)
 * - Uses vTaskDelay() for RTOS compatibility
 */

#include "INetworkClient.h"
#include <Arduino.h>
#include <atomic>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

namespace NETWORK {

// Default configuration
static constexpr uint16_t kDefaultTimeoutMs = 10000;
static constexpr uint16_t kDefaultRetryDelayMs = 200;
static constexpr uint8_t kDefaultMaxRetries = 3;
static constexpr uint16_t kDefaultPollTimeoutMs = 2000;

/**
 * @brief Generic HTTP/HTTPS client implementing INetworkClient
 * 
 * Based on ShellyHttpClient implementation with:
 * - Connection reuse for same host and protocol
 * - Automatic HTTP/HTTPS detection
 * - Retry logic with backoff
 * - Proper RTOS delays
 */
class UnifiedHttpClient : public INetworkClient {
public:
    /**
     * @brief Construct HTTP client
     * 
     * @param runningFlag Optional atomic flag for cancellation support
     * @param allowInsecure If true, skip SSL certificate verification (default: true)
     */
    explicit UnifiedHttpClient(std::atomic<bool>* runningFlag = nullptr, bool allowInsecure = true);
    
    ~UnifiedHttpClient() override = default;

    /**
     * @brief Set Root CA certificate for SSL verification
     * 
     * @param rootCA PEM-encoded root certificate
     */
    void setRootCA(const char* rootCA);

    /**
     * @brief Enable/disable insecure mode (skip certificate verification)
     */
    void setInsecure(bool insecure);

    /**
     * @brief Use the built-in ESP-IDF root CA bundle for HTTPS validation.
     */
    void setUseRootCaBundle(bool useRootCaBundle);

    // INetworkClient interface
    bool get(const char* url, 
             char* responseBuffer = nullptr, 
             size_t bufferSize = 0,
             uint32_t timeoutMs = kDefaultTimeoutMs, 
             size_t* bytesRead = nullptr) override;

    bool post(const char* url,
              const char* payload,
              size_t payloadLen,
              char* responseBuffer = nullptr,
              size_t bufferSize = 0,
              uint32_t timeoutMs = kDefaultTimeoutMs) override;

    /**
     * @brief Perform POST with per-request cancellation context and HTTP code capture.
     *
     * March 2026 note:
     * This overload was added after Webhook/Pushover started sharing one long-lived
     * client instance between the runtime worker and the diagnostics/test API.
     * The target behavior is:
     * - runtime requests observe the shared cancel token published by NotificationWorker,
     * - manual test requests may opt out of that token,
     * - callers no longer race by mutating one shared client-wide cancel pointer.
     *
     * The extra httpCodeOut output also preserves the real HTTP status so upper
     * layers can distinguish transport failures from a valid HTTP rejection.
     */
    bool postWithContext(const char* url,
                         const char* payload,
                         size_t payloadLen,
                         char* responseBuffer = nullptr,
                         size_t bufferSize = 0,
                         uint32_t timeoutMs = kDefaultTimeoutMs,
                         int* httpCodeOut = nullptr,
                         std::atomic<bool>* cancelFlagOverride = nullptr);

    /**
     * @brief Set or replace the cancellation flag checked during network I/O.
     * 
     * When the pointed-to atomic becomes false, in-flight requests abort early.
     * Passing nullptr disables cancellation checks.
     */
    void setCancelFlag(std::atomic<bool>* flag);

    /**
     * @brief Enable/disable connection reuse (Keep-Alive)
     */
    void setReuse(bool reuse);

    /**
     * @brief GET with retry support
     */
    bool getWithRetry(const char* url, 
                      char* responseBuffer = nullptr, 
                      size_t bufferSize = 0,
                      uint32_t timeoutMs = kDefaultTimeoutMs,
                      uint8_t maxRetries = kDefaultMaxRetries,
                      size_t* bytesRead = nullptr);

    /**
     * @brief Quick poll without retry (for status checks)
     *
     * Intended for background/device-health polling paths where the caller
     * already has its own retry/backoff policy and we want to avoid flooding
     * the console with low-level socket logs for every expected miss.
     */
    bool poll(const char* url, char* responseBuffer, size_t bufferSize, uint32_t timeoutMs = kDefaultPollTimeoutMs, size_t* bytesRead = nullptr);

    /**
     * @brief Force close connection
     */
    void disconnect();

    /**
     * @brief Best-effort abort of in-flight socket/TLS I/O.
     *
     * Unlike disconnect(), this is safe to call from shutdown paths that do
     * not hold the higher-level transport mutex. It only tears down the raw
     * sockets and marks the HTTPClient wrapper for cleanup on the next locked
     * request path.
     */
    void cancelActiveIo();

private:
    // Default cancellation token for callers that do not provide a per-request
    // override. Stored atomically because the registry can publish/clear the
    // runtime token while other tasks are preparing new requests.
    std::atomic<std::atomic<bool>*> _runningFlag{nullptr};
    
    // Persistent connection members
    WiFiClient _wifiClient;
    WiFiClientSecure _secureClient;
    HTTPClient _httpClient;
    
    char _lastHost[64] = {0};
    uint16_t _lastPort = 0;
    bool _lastWasSecure = false;
    
    // SSL configuration
    const char* _rootCA = nullptr;
    bool _allowInsecure = true;
    bool _useRootCaBundle = false;
    bool _reuseEnabled = true;
    std::atomic<bool> _deferredHttpReset{false};

    // Keep-alive metadata is tracked separately from the underlying client
    // objects. We only persist it after begin() succeeds, otherwise later
    // requests can misdiagnose a plain connect/TLS failure as "stale reused
    // socket" and perform one unnecessary extra retry.
    void clearConnectionState();
    void rememberConnectionState(const char* host, uint16_t port, bool isSecure);
    std::atomic<bool>* resolveCancelFlag(std::atomic<bool>* cancelFlagOverride) const;
    bool isCancelled(std::atomic<bool>* cancelFlagOverride) const;
    void extractHostPort(const char* url, char* hostBuffer, size_t hostBufferSize, uint16_t& port);
    bool performRequest(const char* url, const char* method, const char* payload, size_t payloadLen,
                        char* buffer, size_t bufferSize, uint32_t timeout, size_t* bytesRead,
                        int* httpCodeOut, std::atomic<bool>* cancelFlagOverride);
    // suppressTransportErrorLogs is intentionally private/internal: it lets
    // background pollers compress noisy socket-layer failures into one higher-
    // level caller log without muting errors for interactive/API traffic.
    //
    // reusedConnectionOut is a debug/decision signal for the caller: it tells
    // performRequest() whether the failed attempt really used an existing
    // keep-alive socket, which is the only case where an automatic stale-
    // connection retry is justified.
    bool performRequestOnce(const char* url, const char* method, const char* payload, size_t payloadLen,
                            char* buffer, size_t bufferSize, uint32_t timeout, size_t* bytesRead,
                            bool* reusedConnectionOut, int* httpCodeOut, std::atomic<bool>* cancelFlagOverride,
                            bool suppressTransportErrorLogs = false);
};

} // namespace NETWORK
