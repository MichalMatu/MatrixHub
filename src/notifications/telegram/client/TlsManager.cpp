/**
 * @file TlsManager.cpp
 * @brief Telegram TLS connection lifecycle manager implementation
 */

#include "TlsManager.h"
#include "TelegramClientBuffers.h"
#include "TelegramConnectionValidator.h"
#include "TelegramTlsConfig.h"
#include "../../../config/App.h"
#include "../../../system/logging/Logging.h"

#include <WiFi.h>
#include <Esp.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#undef LOG_TAG
#define LOG_TAG "TgTls"

namespace TELEGRAM {

namespace {

void formatIpAddress(const IPAddress& address, char* out, size_t outSize) {
    if (!out || outSize == 0) {
        return;
    }

    snprintf(out,
             outSize,
             "%u.%u.%u.%u",
             static_cast<unsigned>(address[0]),
             static_cast<unsigned>(address[1]),
             static_cast<unsigned>(address[2]),
             static_cast<unsigned>(address[3]));
}

}  // namespace

TlsManager::TlsManager() {
    // Client initialized with defaults
}

void TlsManager::setCancelFlag(std::atomic<bool>* flag) {
    _cancelFlag = flag;
}

bool TlsManager::isCancelled() const {
    return _cancelFlag && !_cancelFlag->load(std::memory_order_acquire);
}

void TlsManager::cancelActiveIo() {
    // Caller must hold TelegramClient's mutex. NetworkClientSecure/mbedTLS can
    // crash if another task force-closes the socket while a TLS handshake is
    // reading certificate state.
    _client.stop();
    _currentTlsMode = TlsMode::NotConfigured;
}

void TlsManager::resetTls() {
    cancelActiveIo();
}

void TlsManager::ensureTlsMode(TlsMode required) {
    // Already in correct mode?
    if (_currentTlsMode == required) {
        return;
    }
    
    // Switching modes? Reset first
    if (_currentTlsMode != TlsMode::NotConfigured) {
        resetTls();
    }
    
    // Configure new mode
    switch (required) {
        case TlsMode::RootCA:
            TelegramTlsConfig::configure(_client);
            _client.setHandshakeTimeout(APP::NOTIFICATIONS::TELEGRAM_TLS_HANDSHAKE_TIMEOUT_SEC);
            _client.setTimeout(APP::NOTIFICATIONS::TELEGRAM_HTTP_IO_TIMEOUT_MS);
            break;
        case TlsMode::Insecure:
            _client.setInsecure();
            _client.setHandshakeTimeout(APP::NOTIFICATIONS::TELEGRAM_TLS_HANDSHAKE_TIMEOUT_SEC);
            _client.setTimeout(APP::NOTIFICATIONS::TELEGRAM_HTTP_IO_TIMEOUT_MS);
            break;
        case TlsMode::NotConfigured:
            return;
    }
    
    _currentTlsMode = required;
}

bool TlsManager::connectIfNeeded(TlsMode requiredMode) {
    if (isCancelled()) {
        return false;
    }

    if (_client.connected() && _currentTlsMode == requiredMode) return true;
    ensureTlsMode(requiredMode);

    IPAddress resolved;
    if (!NOTIFICATIONS::TELEGRAM::TelegramConnectionValidator::resolveApiHost(resolved)) {
        LOGW("TLS connect failed: dns resolve for %s failed", kHost);
        return false;
    }

    if (isCancelled()) {
        cancelActiveIo();
        return false;
    }

    _client.setConnectionTimeout(APP::NOTIFICATIONS::TELEGRAM_TLS_CONNECT_TIMEOUT_MS);

    // No network mutex needed — serialized by TelegramClient::_mutex.
    // Keep TLS failure windows short so CORE_PRO tasks like ImuSampler are not
    // starved for multiple seconds during a broken Telegram reconnect.
    //
    // Accepted trade-off:
    // Telegram TLS connect remains a blocking call so the project can keep one lightweight shared
    // client with socket reuse instead of spawning more tasks or maintaining parallel TLS state.
    // The project owner is aware that reconnect/handshake latency can stall the notification
    // pipeline temporarily and explicitly accepts that trade-off under the current DRAM budget.
    LOG_PROFILE_START(tlsStart);
    bool ok = false;
    if (requiredMode == TlsMode::RootCA) {
        ok = TelegramTlsConfig::connectResolved(_client, resolved, 443, kHost);
    } else {
        ok = _client.connect(resolved, 443, kHost, nullptr, nullptr, nullptr);
    }
    LOG_PROFILE_END_SMART(tlsStart, "TLS Handshake", TASK_MONITOR::INTERVAL_TLS_HANDSHAKE_MS, TASK_MONITOR::THRESHOLD_TLS_HANDSHAKE_US);
    if (isCancelled()) {
        cancelActiveIo();
        return false;
    }
    if (!ok) {
        NOTIFICATIONS::TELEGRAM::TelegramConnectionValidator::invalidateReachabilityCache(true);
        // Keep retry/failure logs allocation-free; this path can be noisy on
        // bad WiFi and should not add extra String churn on top of TLS work.
        char ipBuf[16];
        formatIpAddress(resolved, ipBuf, sizeof(ipBuf));
        LOGW("TLS connect failed: host=%s ip=%s", kHost, ipBuf);
    } else if (requiredMode == TlsMode::RootCA) {
        char ipBuf[16];
        formatIpAddress(resolved, ipBuf, sizeof(ipBuf));
        LOGD("TLS connect OK: host=%s ip=%s", kHost, ipBuf);
    }
    return ok;
}

void TlsManager::cleanup(HTTPClient& http) {
    // Full cleanup - use for errors and webhooks (different hosts)
    http.end();
    resetTls();

}

void TlsManager::endRequest(HTTPClient& http) {
    // [FIX] If server closed the connection (e.g., after timeout), we MUST call
    // http.end() to reset HTTPClient internal state. Otherwise, next http.begin()
    // won't establish a new connection (was=0 now=0 bug).
    // 
    // If still connected: keep connection alive for reuse (no http.end())
    // If disconnected: call http.end() to reset state, but keep TLS mode
    
    if (!_client.connected()) {
        // Connection was closed by server - reset HTTPClient state
        http.end();
        return;
    }
    
    // Connection still alive - reset HTTPClient state but keep socket (reuse)
    http.end();
    
    // Check heap health - if largest block is too small, recycle to defrag
    size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    if (largestBlock < kMinSafeBlockSize) {
        LOGW("TLS: Low memory (largest=%u), recycling connection to defrag", largestBlock);
        resetTls();  // Full socket close
    }
}

void TlsManager::drainResponse(HTTPClient& http) {
    WiFiClient* s = http.getStreamPtr();
    if (!s) return;
    
    uint8_t buf[128];
    int remaining = http.getSize();
    if (remaining < 0) remaining = 2048;

    // Telegram responses are small, but the connection may still deliver them
    // in short bursts. Avoid Stream::readBytes() here because it waits for the
    // full requested length and can emit a misleading timeout warning even when
    // the API call itself already succeeded and we are only draining leftovers
    // before reusing the keep-alive connection.
    constexpr uint32_t kDrainBudgetMs = 250;
    constexpr uint32_t kDrainPollDelayMs = 5;
    const uint32_t startMs = millis();

    while (remaining > 0 && (millis() - startMs) < kDrainBudgetMs) {
        int available = s->available();
        if (available <= 0) {
            delay(kDrainPollDelayMs);
            continue;
        }

        size_t toRead = static_cast<size_t>(available);
        const size_t maxChunk = min(static_cast<size_t>(remaining), sizeof(buf));
        if (toRead > maxChunk) {
            toRead = maxChunk;
        }

        const int n = s->read(buf, toRead);
        if (n <= 0) {
            break;
        }

        remaining -= n;
    }
}

bool TlsManager::warmup(const TelegramRuntimeConfigView& config,
                        TelegramClientBuffers& buffers,
                        HTTPClient& http) {
    if (isCancelled()) {
        return false;
    }

    if (!config.settingsAvailable || !config.configured || config.botToken[0] == '\0') {
        LOGW("Warmup skipped: not configured");
        return false;
    }
    
    // Check if we have enough contiguous heap for TLS buffers (~16-20KB needed)
    size_t maxBlock = ESP.getMaxAllocHeap();
    if (maxBlock < APP::NOTIFICATIONS::TELEGRAM_TLS_MIN_HEAP) {
        LOGW("Warmup skipped: insufficient heap (%u bytes contiguous, need %u)", 
             maxBlock, APP::NOTIFICATIONS::TELEGRAM_TLS_MIN_HEAP);
        return false;
    }
    
    // Retry logic - TLS warmup can fail due to heap fragmentation on first boot
    
    for (int attempt = 1; attempt <= APP::NOTIFICATIONS::TELEGRAM_TLS_WARMUP_ATTEMPTS; attempt++) {
        if (isCancelled()) {
            cleanup(http);
            return false;
        }

        if (!connectIfNeeded(TlsMode::RootCA)) {
            cleanup(http);
            if (attempt < APP::NOTIFICATIONS::TELEGRAM_TLS_WARMUP_ATTEMPTS) {
                LOGW("Warmup TLS connect attempt %d/%d failed, retrying after %ums...",
                     attempt, APP::NOTIFICATIONS::TELEGRAM_TLS_WARMUP_ATTEMPTS,
                     APP::NOTIFICATIONS::TELEGRAM_TLS_WARMUP_RETRY_DELAY_MS);
                vTaskDelay(pdMS_TO_TICKS(APP::NOTIFICATIONS::TELEGRAM_TLS_WARMUP_RETRY_DELAY_MS));
                continue;
            }

            LOGW("TLS warmup connect failed after %d attempts", APP::NOTIFICATIONS::TELEGRAM_TLS_WARMUP_ATTEMPTS);
            break;
        }
        
        // Simple GET to /getMe to establish TLS connection. The temporary URL
        // now lives in the TelegramClient-owned scratch bundle passed in by the
        // caller instead of a hidden static buffer.
        snprintf(buffers.url, sizeof(buffers.url), "https://%s/bot%s/getMe", kHost, config.botToken);
        
        bool success = false;
        if (http.begin(_client, buffers.url)) {
            http.setReuse(true);
            http.setTimeout(APP::NOTIFICATIONS::TELEGRAM_HTTP_IO_TIMEOUT_MS);

            if (isCancelled()) {
                cleanup(http);
                return false;
            }

            int httpCode = http.GET();

            if (isCancelled()) {
                cleanup(http);
                return false;
            }

            if (httpCode > 0) {
                drainResponse(http);
                success = (httpCode >= 200 && httpCode < 300);
                if (!success) {
                    LOGW("Warmup HTTP error: %d", httpCode);
                }
            } else {
                LOGE("Warmup connection failed: %s (alloc specific?)", http.errorToString(httpCode).c_str());
            }
        } else {
            LOGE("Warmup http.begin() failed - allocation error?");
        }

        if (success) {
            LOGI("TLS warmup OK - connection kept alive for reuse");
            endRequest(http);  // Keep TLS alive
            return true;
        }

        // Cleanup before retry or exit
        cleanup(http);

        if (attempt < APP::NOTIFICATIONS::TELEGRAM_TLS_WARMUP_ATTEMPTS) {
            LOGW("Warmup attempt %d/%d failed, retrying after %ums...", 
                 attempt, APP::NOTIFICATIONS::TELEGRAM_TLS_WARMUP_ATTEMPTS, 
                 APP::NOTIFICATIONS::TELEGRAM_TLS_WARMUP_RETRY_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(APP::NOTIFICATIONS::TELEGRAM_TLS_WARMUP_RETRY_DELAY_MS));  // Allow heap defragmentation
        } else {
            LOGW("TLS warmup failed after %d attempts - full cleanup", APP::NOTIFICATIONS::TELEGRAM_TLS_WARMUP_ATTEMPTS);
        }
    }
    
    LOGE("TLS warmup CRITICAL FAILURE after %d attempts - disabling Telegram", APP::NOTIFICATIONS::TELEGRAM_TLS_WARMUP_ATTEMPTS);
    // Optional: Disable telegram in runtime settings to prevent further loops?
    return false;
}

void TlsManager::logConnEvent(const char* op, bool wasConnected) {
    bool isConnected = _client.connected();
    const uint32_t freeHeap = ESP.getFreeHeap();
    const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

    LOGV("TLS conn: %s was=%d now=%d free=%u largest=%u",
         op,
         wasConnected ? 1 : 0,
         isConnected ? 1 : 0,
         (unsigned)freeHeap,
         (unsigned)largest);

    if (!wasConnected && isConnected) {
        LOGD("TLS conn: NEW (%s)", op);
    } else if (wasConnected && !isConnected) {
        LOGW("TLS conn: CLOSED (%s)", op);
    }
}

}  // namespace TELEGRAM
