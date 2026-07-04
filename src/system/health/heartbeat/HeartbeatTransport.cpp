#include "HeartbeatTransport.h"

#include "HeartbeatConfigSanitizer.h"
#include "../../logging/Logging.h"
#include "../../network/UnifiedHttpClient.h"

#include <esp_heap_caps.h>
#include <new>

#undef LOG_TAG
#define LOG_TAG "HeartTx"

namespace SYSTEM {
namespace HEARTBEAT_DETAIL {

HeartbeatTransport::HeartbeatTransport(std::atomic<bool>* runningFlag)
    : _runningFlag(runningFlag) {
}

bool HeartbeatTransport::ensureClient() {
    if (_client) {
        return true;
    }

    _clientStorage = heap_caps_malloc(sizeof(NETWORK::UnifiedHttpClient), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!_clientStorage) {
        LOGE("Failed to allocate Heartbeat HTTP client");
        return false;
    }

    _client = new (_clientStorage) NETWORK::UnifiedHttpClient(_runningFlag, false);
    _client->setReuse(false);
    _lastUseMs = millis();
    return true;
}

void HeartbeatTransport::configureClientForSlot(NETWORK::UnifiedHttpClient& client, const RTC::HeartbeatSlot& slot) {
    client.setRootCA(nullptr);
    client.setUseRootCaBundle(false);
    client.setInsecure(false);

    if (startsWithHttps(slot.url)) {
        if (slot.allowInsecure) {
            client.setInsecure(true);
        } else {
            client.setUseRootCaBundle(true);
        }
    }
}

bool HeartbeatTransport::ping(const RTC::HeartbeatSlot& slot, uint32_t timeoutMs, uint8_t retries) {
    if (!ensureClient()) {
        return false;
    }

    configureClientForSlot(*_client, slot);
    const bool ok = _client->getWithRetry(slot.url, nullptr, 0, timeoutMs, retries);
    _lastUseMs = millis();
    return ok;
}

void HeartbeatTransport::cancelActiveIo() {
    if (!_client) {
        return;
    }

    // Match the shutdown strategy already used by notification transports:
    // break blocking socket/TLS I/O immediately, but defer the full HTTPClient
    // cleanup to the next serialized path or the final release() call.
    _client->cancelActiveIo();
}

void HeartbeatTransport::release() {
    if (!_client) {
        return;
    }

    _client->disconnect();
    _client->~UnifiedHttpClient();
    _client = nullptr;

    if (_clientStorage) {
        heap_caps_free(_clientStorage);
        _clientStorage = nullptr;
    }

    _lastUseMs = 0;
}

void HeartbeatTransport::releaseIfIdle(uint32_t nowMs, uint32_t idleMs) {
    if (!_client || _lastUseMs == 0 || (nowMs - _lastUseMs) < idleMs) {
        return;
    }

    LOGD("Releasing idle Heartbeat HTTP client");
    release();
}

} // namespace HEARTBEAT_DETAIL
} // namespace SYSTEM
