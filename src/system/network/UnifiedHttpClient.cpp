/**
 * @file UnifiedHttpClient.cpp
 * @brief Generic HTTP/HTTPS client implementation
 */

#include "UnifiedHttpClient.h"
#include "UnifiedHttpClientPolicy.h"
#include "../logging/Logging.h"
#include "../../config/Network.h"
#include "../watchdog/TaskWatchdog.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#undef LOG_TAG
#define LOG_TAG "HttpClient"

namespace NETWORK {

namespace {
extern const uint8_t _binary_x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t _binary_x509_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");

class ScopedTransportErrorLogMute {
public:
    explicit ScopedTransportErrorLogMute(bool enabled) : _enabled(enabled) {
        if (!_enabled) {
            return;
        }

        // Background LAN polling can intentionally hit stale/wrong peers for a
        // while (for example during device bring-up or when a Shelly IP changed).
        // In that mode we still want one higher-level caller log, but the raw
        // Arduino NetworkClient/HTTPClient error cascade is too noisy.
        _networkClientLevel = esp_log_level_get("NetworkClient.cpp");
        _networkClientCompatLevel = esp_log_level_get("NetworkClient");
        _httpClientLevel = esp_log_level_get("HTTPClient.cpp");
        _httpClientCompatLevel = esp_log_level_get("HTTPClient");

        esp_log_level_set("NetworkClient.cpp", ESP_LOG_NONE);
        esp_log_level_set("NetworkClient", ESP_LOG_NONE);
        esp_log_level_set("HTTPClient.cpp", ESP_LOG_NONE);
        esp_log_level_set("HTTPClient", ESP_LOG_NONE);
    }

    ~ScopedTransportErrorLogMute() {
        if (!_enabled) {
            return;
        }

        esp_log_level_set("NetworkClient.cpp", _networkClientLevel);
        esp_log_level_set("NetworkClient", _networkClientCompatLevel);
        esp_log_level_set("HTTPClient.cpp", _httpClientLevel);
        esp_log_level_set("HTTPClient", _httpClientCompatLevel);
    }

private:
    bool _enabled = false;
    esp_log_level_t _networkClientLevel = ESP_LOG_INFO;
    esp_log_level_t _networkClientCompatLevel = ESP_LOG_INFO;
    esp_log_level_t _httpClientLevel = ESP_LOG_INFO;
    esp_log_level_t _httpClientCompatLevel = ESP_LOG_INFO;
};
} // namespace

UnifiedHttpClient::UnifiedHttpClient(std::atomic<bool>* runningFlag, bool allowInsecure)
    : _runningFlag(runningFlag),
      _allowInsecure(allowInsecure),
      _useRootCaBundle(false),
      _reuseEnabled(true) {
}

void UnifiedHttpClient::setRootCA(const char* rootCA) {
    _rootCA = rootCA;
}

void UnifiedHttpClient::setInsecure(bool insecure) {
    _allowInsecure = insecure;
}

void UnifiedHttpClient::setUseRootCaBundle(bool useRootCaBundle) {
    _useRootCaBundle = useRootCaBundle;
}

void UnifiedHttpClient::setReuse(bool reuse) {
    _reuseEnabled = reuse;
}

void UnifiedHttpClient::clearConnectionState() {
    _lastHost[0] = '\0';
    _lastPort = 0;
    _lastWasSecure = false;
}

void UnifiedHttpClient::rememberConnectionState(const char* host, uint16_t port, bool isSecure) {
    if (!host) {
        clearConnectionState();
        return;
    }

    strlcpy(_lastHost, host, sizeof(_lastHost));
    _lastPort = port;
    _lastWasSecure = isSecure;
}

void UnifiedHttpClient::setCancelFlag(std::atomic<bool>* flag) {
    // March 2026 refactor:
    // keep only the default token here. The actual request may still override it
    // via postWithContext(), which avoids cross-request races on shared clients.
    _runningFlag.store(flag, std::memory_order_release);
}

std::atomic<bool>* UnifiedHttpClient::resolveCancelFlag(std::atomic<bool>* cancelFlagOverride) const {
    if (cancelFlagOverride) {
        return cancelFlagOverride;
    }

    return _runningFlag.load(std::memory_order_acquire);
}

bool UnifiedHttpClient::isCancelled(std::atomic<bool>* cancelFlagOverride) const {
    std::atomic<bool>* flag = resolveCancelFlag(cancelFlagOverride);
    return flag && !flag->load(std::memory_order_acquire);
}

bool UnifiedHttpClient::get(const char* url, char* responseBuffer, size_t bufferSize,
                            uint32_t timeoutMs, size_t* bytesRead) {
    return performRequest(
        url, "GET", nullptr, 0, responseBuffer, bufferSize, timeoutMs, bytesRead, nullptr, nullptr);
}

bool UnifiedHttpClient::post(const char* url, const char* payload, size_t payloadLen,
                             char* responseBuffer, size_t bufferSize, uint32_t timeoutMs) {
    return performRequest(url,
                          "POST",
                          payload,
                          payloadLen,
                          responseBuffer,
                          bufferSize,
                          timeoutMs,
                          nullptr,
                          nullptr,
                          nullptr);
}

bool UnifiedHttpClient::postWithContext(const char* url,
                                        const char* payload,
                                        size_t payloadLen,
                                        char* responseBuffer,
                                        size_t bufferSize,
                                        uint32_t timeoutMs,
                                        int* httpCodeOut,
                                        std::atomic<bool>* cancelFlagOverride) {
    return performRequest(url,
                          "POST",
                          payload,
                          payloadLen,
                          responseBuffer,
                          bufferSize,
                          timeoutMs,
                          nullptr,
                          httpCodeOut,
                          cancelFlagOverride);
}

bool UnifiedHttpClient::getWithRetry(const char* url, char* responseBuffer, size_t bufferSize,
                                     uint32_t timeoutMs, uint8_t maxRetries, size_t* bytesRead) {
    for (uint8_t attempt = 0; attempt < maxRetries; attempt++) {
        if (performRequest(url,
                           "GET",
                           nullptr,
                           0,
                           responseBuffer,
                           bufferSize,
                           timeoutMs,
                           bytesRead,
                           nullptr,
                           nullptr)) {
            return true;
        }

        // Don't sleep on last attempt or if cancelled
        if (attempt < maxRetries - 1 && !isCancelled(nullptr)) {
            // Exponential backoff
            vTaskDelay(pdMS_TO_TICKS(kDefaultRetryDelayMs * (attempt + 1)));
        } else {
            // Important for bounded shutdown: once the caller cancelled (for
            // example Heartbeat stop) or we already consumed the final slot,
            // leave the loop immediately instead of accidentally issuing one
            // more full HTTP attempt on the next iteration.
            char safeUrl[96];
            LOG::Logging::redactUrlForLog(url, safeUrl, sizeof(safeUrl));
            LOGW("HTTP GET failed after %d attempts: %s", maxRetries, safeUrl);
            break;
        }
    }
    
    return false;
}

bool UnifiedHttpClient::poll(const char* url, char* responseBuffer, size_t bufferSize, uint32_t timeoutMs, size_t* bytesRead) {
    // For background polling, NEVER retry. Fail-fast to avoid starving the Watchdog.
    return performRequestOnce(
        url, "GET", nullptr, 0, responseBuffer, bufferSize, timeoutMs, bytesRead, nullptr, nullptr, nullptr, true);
}

void UnifiedHttpClient::disconnect() {
    _httpClient.setReuse(false);
    _httpClient.end();
    _wifiClient.stop();
    _secureClient.stop();
    clearConnectionState();
    _deferredHttpReset.store(false, std::memory_order_release);
}

void UnifiedHttpClient::cancelActiveIo() {
    // Shutdown/reconfigure paths may need to break a blocking POST/GET without
    // first taking the shared transport mutex. In that context it is safe to
    // tear down only the raw sockets here; the full HTTPClient::end() cleanup is
    // deferred until the next normal request path regains serialization.
    _wifiClient.stop();
    _secureClient.stop();
    clearConnectionState();
    _deferredHttpReset.store(true, std::memory_order_release);
}

void UnifiedHttpClient::extractHostPort(const char* url, char* hostBuffer, size_t hostBufferSize, uint16_t& port) {
    if (!url || !hostBuffer || hostBufferSize == 0) return;
    
    // Detect HTTPS for default port
    bool isHttps = strncmp(url, "https://", 8) == 0;
    port = isHttps ? 443 : API::DEFAULT_HTTP_PORT;
    hostBuffer[0] = '\0';
    
    const char* protocol = strstr(url, "://");
    const char* hostStart = protocol ? protocol + 3 : url;
    
    // Find end of host (start of path or end of string)
    const char* pathStart = strchr(hostStart, '/');
    size_t hostStrLen = pathStart ? (size_t)(pathStart - hostStart) : strlen(hostStart);
    
    if (hostStrLen >= hostBufferSize) {
        hostStrLen = hostBufferSize - 1;
    }
    
    // Check for port separator
    const char* colon = (const char*)memchr(hostStart, ':', hostStrLen);
    if (colon) {
        size_t actualHostLen = (size_t)(colon - hostStart);
        if (actualHostLen >= hostBufferSize) actualHostLen = hostBufferSize - 1;
        
        strncpy(hostBuffer, hostStart, actualHostLen);
        hostBuffer[actualHostLen] = '\0';
        
        port = (uint16_t)atoi(colon + 1);
    } else {
        strncpy(hostBuffer, hostStart, hostStrLen);
        hostBuffer[hostStrLen] = '\0';
    }
}

bool UnifiedHttpClient::performRequest(const char* url, const char* method, const char* payload, size_t payloadLen,
                                       char* buffer, size_t bufferSize, uint32_t timeout, size_t* bytesRead,
                                       int* httpCodeOut, std::atomic<bool>* cancelFlagOverride) {
    if (httpCodeOut) {
        // Report the actual library status to the caller so transport layers can
        // map "HTTP rejection" differently from "socket/TLS/timeout failure".
        *httpCodeOut = 0;
    }

    // Allow one automatic retry only for the specific keep-alive case we care
    // about: the previous socket was alive enough to be reused, but failed once
    // the request started. Fresh-connection failures should bubble up directly.
    for (int attempt = 0; attempt < 2; attempt++) {
        bool reusedConnection = false;
        bool result = performRequestOnce(url,
                                         method,
                                         payload,
                                         payloadLen,
                                         buffer,
                                         bufferSize,
                                         timeout,
                                         bytesRead,
                                         &reusedConnection,
                                         httpCodeOut,
                                         cancelFlagOverride,
                                         false);
        if (result) return true;
        
        // Retry only when the first failure came from a genuinely reused
        // socket. Fresh-connection failures should surface immediately so
        // shutdown budgets and caller retry math stay predictable.
        if (attempt == 0 && reusedConnection) {
            // First attempt failed — force fresh connection and retry
            LOGD("Request failed, forcing fresh connection for retry");
            disconnect();
            continue;
        }
        break;
    }
    return false;
}

bool UnifiedHttpClient::performRequestOnce(const char* url, const char* method, const char* payload, size_t payloadLen,
                                           char* buffer, size_t bufferSize, uint32_t timeout, size_t* bytesRead,
                                           bool* reusedConnectionOut, int* httpCodeOut,
                                           std::atomic<bool>* cancelFlagOverride,
                                           bool suppressTransportErrorLogs) {
    if (bytesRead) *bytesRead = 0;
    if (buffer && bufferSize > 0) buffer[0] = '\0';
    if (httpCodeOut) *httpCodeOut = 0;
    if (reusedConnectionOut) *reusedConnectionOut = false;

    if (isCancelled(cancelFlagOverride)) {
        return false;
    }

    if (_deferredHttpReset.exchange(false, std::memory_order_acq_rel)) {
        // Complete the deferred cleanup after an out-of-band cancelActiveIo().
        // This runs only on the normal serialized request path, so touching the
        // HTTPClient wrapper here is safe again.
        _httpClient.setReuse(false);
        _httpClient.end();
    }

    // Reset watchdog before starting network checks/connection
    SYSTEM::TaskWatchdog::instance().reset();

    if (WiFi.status() != WL_CONNECTED) {
        if (_lastHost[0] != '\0') {
            disconnect();
        }
        LOGD("STA offline, aborting outbound HTTP request");
        return false;
    }

    // Detect HTTPS
    bool isHttps = strncmp(url, "https://", 8) == 0;

    // Connection reuse logic
    char currentHost[64];
    uint16_t currentPort;
    extractHostPort(url, currentHost, sizeof(currentHost), currentPort);

    // If host changed or protocol changed, close previous connection
    bool hostChanged = _lastHost[0] != '\0' && 
                       (strncmp(currentHost, _lastHost, sizeof(_lastHost)) != 0 || currentPort != _lastPort);
    bool protocolChanged = _lastHost[0] != '\0' && _lastWasSecure != isHttps;
    
    if (hostChanged || protocolChanged) {
        if (hostChanged) {
            LOGD("Switching host: %s:%u -> %s:%u", _lastHost, _lastPort, currentHost, currentPort);
        }
        if (protocolChanged) {
            LOGD("Switching protocol: %s -> %s", _lastWasSecure ? "HTTPS" : "HTTP", isHttps ? "HTTPS" : "HTTP");
        }
        _httpClient.setReuse(false);
        _httpClient.end();
        if (_lastWasSecure) {
            _secureClient.stop();
        } else {
            _wifiClient.stop();
        }
        // Clear metadata before the new begin(). If begin() below fails we want
        // the client to surface a plain fresh-connect failure, not look like it
        // still owns a valid reusable socket for the old endpoint.
        clearConnectionState();
    }

    // Choose client based on protocol
    bool connected = false;
    bool reusedConnection = false;

    // UnifiedHttpClient will handle internal watchdog resets between chunks,
    // but we reset it here too for the blocking start.
    SYSTEM::TaskWatchdog::instance().reset();

    if (isHttps) {
        _secureClient.setCACertBundle(nullptr, 0);

        if (_rootCA) {
            _secureClient.setCACert(_rootCA);
        } else if (_useRootCaBundle) {
            _secureClient.setCACert(nullptr);
            _secureClient.setCACertBundle(
                _binary_x509_crt_bundle_start,
                static_cast<size_t>(_binary_x509_crt_bundle_end - _binary_x509_crt_bundle_start));
        } else if (_allowInsecure) {
            _secureClient.setInsecure();
        } else {
            char safeUrl[96];
            LOG::Logging::redactUrlForLog(url, safeUrl, sizeof(safeUrl));
            LOGW("HTTPS requested without a TLS verification mode: %s", safeUrl);
        }
        _secureClient.setTimeout(timeout); // WiFiClientSecure uses ms

        bool isReusing = !hostChanged && !protocolChanged && _secureClient.connected();
        reusedConnection = isReusing;
        if (isReusing) {
            LOGD("Reusing HTTPS connection to %s", currentHost);
        } else {
            LOGD("New HTTPS connection to %s", currentHost);
        }
        
        connected = _httpClient.begin(_secureClient, url);
    } else {
        bool isReusing = !hostChanged && !protocolChanged && _wifiClient.connected();
        if (isReusing) {
            LOGD("Reusing HTTP connection to %s", currentHost);
        } else {
            LOGD("New HTTP connection to %s", currentHost);
            _wifiClient.stop();
        }
        reusedConnection = isReusing;
        
        // WiFiClient::setTimeout takes milliseconds in ESP32 Arduino 2.x/3.x
        _wifiClient.setTimeout(timeout);
        connected = _httpClient.begin(_wifiClient, url);
    }

    if (reusedConnectionOut) {
        *reusedConnectionOut = reusedConnection;
    }
    
    if (connected) {
        // Keep-alive metadata should only describe a connection that actually
        // exists. This prevents unrelated begin()/TLS failures from poisoning
        // the next request into thinking a stale reusable socket exists.
        rememberConnectionState(currentHost, currentPort, isHttps);
        _httpClient.setConnectTimeout(timeout);
        _httpClient.setTimeout(timeout);
        
        // Enable/Disable Keep-Alive based on settings
        _httpClient.setReuse(_reuseEnabled);
        if (_reuseEnabled) {
            _httpClient.addHeader("Connection", "keep-alive");
        } else {
            _httpClient.addHeader("Connection", "close");
        }
        
        // Accepted trade-off:
        // The underlying HTTPClient / WiFiClientSecure calls here are intentionally synchronous.
        // Higher layers in this project prefer one small shared transport path with connection reuse
        // over an async redesign or extra worker stacks. The project owner is aware that a slow or
        // wedged peer can block the caller inside GET/POST until the library returns and accepts
        // that risk as part of the current DRAM-budgeted architecture.
        //
        // Reset Watchdog right before blocking GET/POST (can take seconds on TLS handshake)
        SYSTEM::TaskWatchdog::instance().reset();
        
        ScopedTransportErrorLogMute muteTransportErrors(suppressTransportErrorLogs);
        int code;
        if (strcmp(method, "POST") == 0 && payload && payloadLen > 0) {
            _httpClient.addHeader("Content-Type", "application/json");
            code = _httpClient.POST((uint8_t*)payload, payloadLen);
        } else {
            code = _httpClient.GET();
        }

        if (httpCodeOut) {
            // Keep the raw HTTPClient status available to the caller. Webhook and
            // Pushover transports use this to preserve diagnostics fidelity and to
            // avoid retrying every failure as if it were the same transport issue.
            *httpCodeOut = code;
        }
        
        // Feed watchdog immediately after blocking library call returns
        SYSTEM::TaskWatchdog::instance().reset();
        
        bool isSuccess = (code >= 200 && code < 300);
        
        if (isSuccess) {
            bool readSuccess = true;
            const bool callerConsumesBody = buffer && bufferSize > 0;
            if (callerConsumesBody) {
                WiFiClient* stream = _httpClient.getStreamPtr();
                if (stream) {
                    size_t totalRead = 0;
                    size_t maxRead = bufferSize - 1;
                    
                    uint32_t absoluteStart = millis(); // Absolute timeout for entire transfer
                    uint32_t readStart = millis();     // Idle timeout between chunks
                    
                    // Allow max 2x the base timeout for the entire transfer, 
                    // or a minimum of 5 seconds to ensure we don't cut off slow but valid large payloads.
                    uint32_t absoluteTimeout = (timeout * 2 > 5000) ? (timeout * 2) : 5000;
                    
                    while (_httpClient.connected() && (stream->available() || stream->connected()) && totalRead < maxRead) {
                        // The chosen cancel token is request-scoped. That is the
                        // important part of the March 2026 fix: another task may
                        // publish a different default token without changing the
                        // cancellation behavior of this in-flight request.
                        if (isCancelled(cancelFlagOverride)) {
                            readSuccess = false;
                            break;
                        }

                        if (millis() - readStart > timeout) {
                            LOGW("HTTP Idle Read Timeout");
                            readSuccess = false;
                            break;
                        }
                        
                        if (millis() - absoluteStart > absoluteTimeout) {
                            LOGW("HTTP Absolute Read Timeout (too slow)");
                            readSuccess = false;
                            break;
                        }
                        
                        if (stream->available()) {
                            // Reset watchdog for incoming data chunks
                            SYSTEM::TaskWatchdog::instance().reset();
                            
                            size_t available = stream->available();
                            size_t toRead = (available < maxRead - totalRead) ? available : (maxRead - totalRead);
                            
                            size_t read = stream->readBytes(buffer + totalRead, toRead);
                            if (read > 0) {
                                totalRead += read;
                                readStart = millis(); // Reset timeout on successful read
                            } else {
                                break; // Break if read fails to prevent infinite CPU spin
                            }
                        } else {
                            // Feed the watchdog during long HTTP stream reads
                            SYSTEM::TaskWatchdog::instance().reset();
                            vTaskDelay(pdMS_TO_TICKS(10)); // Yield while waiting for more data
                        }
                    }
                    
                    buffer[totalRead] = '\0';
                    if (bytesRead) *bytesRead = totalRead;
                    LOGD("HTTP Read: %u bytes", totalRead);
                } else {
                    readSuccess = false;
                }
            }
            
            // Keep-alive is only safe when this layer actually consumed the
            // response body. Some long-lived runtime callers intentionally pass
            // no output buffer because they only care about the HTTP status
            // code. Leaving such a 2xx response unread would keep stale bytes
            // on the socket and poison the next reused request, so we
            // intentionally sacrifice reuse for that narrower call pattern.
            //
            // Failed reads also leave the connection in unknown state.
            if (shouldCloseConnectionAfterSuccess(readSuccess,
                                                  _reuseEnabled,
                                                  callerConsumesBody)) {
                _httpClient.end();
                // Debug note: a partial/failed body read is treated as a dirty
                // socket. Clearing the reuse metadata here prevents the next
                // call from wasting time on a stale-connection retry.
                clearConnectionState();
            }
            return readSuccess;
        }
        
        // Handle errors
        if (code < 0) {
            // NOTE: Intentionally NOT using _httpClient.errorToString(code)
            // as it allocates a temporary Arduino String on heap,
            // causing memory fragmentation over long uptime.
            if (suppressTransportErrorLogs) {
                LOGD("HTTP error %d", code);
            } else {
                LOGW("HTTP error %d", code);
            }
        } else {
            if (suppressTransportErrorLogs) {
                LOGD("HTTP status %d", code);
            } else {
                LOGW("HTTP status %d", code);
            }
        }
        
        _httpClient.setReuse(false);
        _httpClient.end();
        // Any HTTP error/non-2xx response closes the current reuse chain. If a
        // future request should retry, it must first prove it has a live reused
        // socket again via _wifiClient/_secureClient.connected().
        clearConnectionState();
        
    } else {
        char safeUrl[96];
        LOG::Logging::redactUrlForLog(url, safeUrl, sizeof(safeUrl));
        if (suppressTransportErrorLogs) {
            LOGD("HTTP begin failed: %s", safeUrl);
        } else {
            LOGE("HTTP begin failed: %s", safeUrl);
        }
        // begin() never established a request, so do not retain host/protocol
        // metadata that could make the next failure look like stale keep-alive.
        clearConnectionState();
    }
    
    return false;
}

} // namespace NETWORK
