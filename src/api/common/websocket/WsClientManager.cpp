#include "WsClientManager.h"
#include "../IWebSocketAuthenticator.h"
#include "../../../system/logging/Logging.h"
#include "../../../system/health/network/HttpServerHealthTracker.h"
#include "../../../system/utils/ScopeLock.h"
#include "../../../config/Network.h"
#include <lwip/sockets.h>
#include <cstring>
#include <esp_heap_caps.h>

#undef LOG_TAG
#define LOG_TAG "WsClientMgr"

namespace API {
namespace WEBSOCKET {

namespace {
constexpr TickType_t kClientMapLockTimeout = pdMS_TO_TICKS(100);
}

WsClientManager::WsClientManager(const char* logTag, 
                                 IWebSocketAuthenticator* authenticator, 
                                 StateChangeCallback onStateChange, 
                                 uint32_t sendTimeoutMs)
    : _logTag(logTag), _authenticator(authenticator), _onStateChange(onStateChange), _sendTimeoutMs(sendTimeoutMs) {
    _lock = xSemaphoreCreateMutex();
}

WsClientManager::~WsClientManager() {
    if (_lock) {
        vSemaphoreDelete(_lock);
        _lock = nullptr;
    }
}

bool WsClientManager::isReadyForBroadcast(const ClientState& client, uint32_t now) {
    return static_cast<int32_t>(now - client.readyAtMs) >= 0;
}

void WsClientManager::drainPendingDisconnect(bool isBroadcastTaskContext) {
    if (!_onStateChange || isBroadcastTaskContext) return;

    if (_disconnectCallbackPending.exchange(false, std::memory_order_acq_rel)) {
        _onStateChange(false);
    }
}

void WsClientManager::setDisconnectPending(bool pending) {
    _disconnectCallbackPending.store(pending, std::memory_order_release);
}

esp_err_t WsClientManager::handleHandshake(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        if (_authenticator && !_authenticator->authenticate(req)) {
            LOGW("[%s] WS Auth Failed - closing connection", _logTag);
            int fd = httpd_req_to_sockfd(req);
            httpd_sess_trigger_close(req->handle, fd);
            return ESP_FAIL;
        }

        int fd = httpd_req_to_sockfd(req);
        _serverHandle = req->handle;
        bool shouldNotifyConnected = false;
        bool clientRegistered = false;
        bool shouldRecordWsOpen = false;
        
        {
            SYSTEM::ScopeLock lock(_lock);
            if (lock.isLocked()) {
                const bool wasEmpty = _clients.empty();
                const bool wasKnown = _clients.find(fd) != _clients.end();
                _clients[fd] = ClientState{
                    0,
                    millis() + API::WS_CLIENT_HANDSHAKE_GRACE_MS
                };
                _clientCount.store(_clients.size(), std::memory_order_release);
                clientRegistered = true;
                shouldRecordWsOpen = !wasKnown;
                LOGD("[%s] Client Connected: %d (Total: %zu)", _logTag, fd, _clients.size());
                if (wasEmpty) {
                    _disconnectCallbackPending.store(false, std::memory_order_release);
                    shouldNotifyConnected = (_onStateChange != nullptr);
                }
            }
        }

        if (clientRegistered) {
            struct timeval tv;
            tv.tv_sec = _sendTimeoutMs / 1000;
            tv.tv_usec = (_sendTimeoutMs % 1000) * 1000;
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            // The framework already counted this socket in its onOpen hook.
            // A successful WS handshake reuses that same TCP connection, so
            // counting here would inflate active/peak client diagnostics.
            if (shouldRecordWsOpen) {
                SYSTEM::HEALTH::HttpServerHealthTracker::recordWsOpen();
            }
        }
        if (shouldNotifyConnected) {
            _onStateChange(true);
        }
        return ESP_OK;
    }
    
    // Proactive disconnect via WS text frame
    uint8_t dummy_buf[128];
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = dummy_buf;
    httpd_ws_recv_frame(req, &ws_pkt, sizeof(dummy_buf));

    int fd = httpd_req_to_sockfd(req);
    LOGD("[%s] Proactive STOP signal received from FD %d", _logTag, fd);
    // When handling web socket frame directly from httpd, it's not the broadcast task context
    removeClient(fd, false, false);
    
    return ESP_OK;
}

void WsClientManager::removeClient(int fd, bool triggerClose, bool isBroadcastTaskContext) {
    bool shouldTriggerClose = false;
    bool shouldNotifyDisconnected = false;
    bool shouldDrainPendingDisconnect = false;
    bool shouldRecordWsClose = false;

    SYSTEM::ScopeLock lock(_lock, pdMS_TO_TICKS(100));
    if (lock.isLocked()) {
        auto it = _clients.find(fd);
        if (it != _clients.end()) {
            _clients.erase(it);
            _clientCount.store(_clients.size(), std::memory_order_release);
            shouldRecordWsClose = true;
            LOGD("[%s] Client Disconnected: %d (Remaining: %zu)", _logTag, fd, _clients.size());

            if (_clients.empty() && _onStateChange) {
                if (isBroadcastTaskContext) {
                    _disconnectCallbackPending.store(true, std::memory_order_release);
                } else {
                    _disconnectCallbackPending.store(false, std::memory_order_release);
                    shouldNotifyDisconnected = true;
                }
            }

            if (triggerClose && _serverHandle) {
                shouldTriggerClose = true;
                SYSTEM::HEALTH::HttpServerHealthTracker::recordWsForcedRemoval(fd);
            }
        } else if (_clients.empty() && _onStateChange) {
            shouldDrainPendingDisconnect = true;
        }
    }

    if (shouldTriggerClose && _serverHandle) {
        LOGW("[%s] Forcing session close for fd %d", _logTag, fd);
        httpd_sess_trigger_close(_serverHandle, fd);
    }
    if (shouldRecordWsClose) {
        SYSTEM::HEALTH::HttpServerHealthTracker::recordWsClose();
    }
    if (shouldNotifyDisconnected) {
        _onStateChange(false);
        return;
    }
    if (shouldDrainPendingDisconnect) {
        drainPendingDisconnect(isBroadcastTaskContext);
    }
}

void WsClientManager::performBroadcast(uint8_t* data, size_t len, httpd_ws_type_t type, int* targets, size_t targetCount, bool isBroadcastTaskContext) {
    if (!_serverHandle || _clientCount.load(std::memory_order_acquire) == 0) return;
    
    int broadcastTargets[MAX_BROADCAST_TARGETS];
    int removeTargets[MAX_BROADCAST_TARGETS];
    size_t actualTargetCount = 0;
    {
        SYSTEM::ScopeLock lock(_lock, kClientMapLockTimeout);
        if (!lock.isLocked()) return;
        if (_clients.empty()) return;

        if (targets && targetCount > 0) {
            const uint32_t now = millis();
            for (size_t i = 0; i < targetCount; i++) {
                int fd = targets[i];
                auto it = _clients.find(fd);
                if (it != _clients.end() && isReadyForBroadcast(it->second, now)) {
                    if (actualTargetCount < MAX_BROADCAST_TARGETS) {
                        broadcastTargets[actualTargetCount++] = fd;
                    }
                }
            }
        } else {
            const uint32_t now = millis();
            for (const auto& kv : _clients) {
                if (!isReadyForBroadcast(kv.second, now)) {
                    continue;
                }
                if (actualTargetCount < MAX_BROADCAST_TARGETS) {
                    broadcastTargets[actualTargetCount++] = kv.first;
                } else {
                    LOGW("[%s] Broadcast limit exceeded! Dropping client %d", _logTag, kv.first);
                }
            }
        }
    }

    if (actualTargetCount == 0) return;

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = data;
    ws_pkt.len = len;
    ws_pkt.type = type;

    size_t removeCount = 0;

    for (size_t i = 0; i < actualTargetCount; i++) {
        int fd = broadcastTargets[i];
        esp_err_t ret = httpd_ws_send_data(_serverHandle, fd, &ws_pkt);
        if (ret != ESP_OK) {
            SYSTEM::ScopeLock lock(_lock, pdMS_TO_TICKS(10));
            if (lock.isLocked()) {
                auto it = _clients.find(fd);
                if (it != _clients.end()) {
                    it->second.failures++;
                    
                    bool isSoftError = (ret == ESP_ERR_TIMEOUT || ret == ESP_ERR_NO_MEM || ret == ESP_FAIL);
                    uint8_t threshold = isSoftError ? MAX_SEND_FAILURES : 1; 
                    
                    if (it->second.failures >= threshold) {
                        LOGW("[%s] Client %d: %d failures (err: %d), removing",
                             _logTag, fd, it->second.failures, ret);
                        if (removeCount < MAX_BROADCAST_TARGETS) {
                            removeTargets[removeCount++] = fd;
                        }
                    }
                }
            }
        } else {
            SYSTEM::ScopeLock lock(_lock, pdMS_TO_TICKS(10));
            if (lock.isLocked()) {
                auto it = _clients.find(fd);
                if (it != _clients.end()) it->second.failures = 0;
            }
            httpd_sess_update_lru_counter(_serverHandle, fd);
        }
        
        if (i % 4 == 0) vTaskDelay(0);
    }

    for (size_t i = 0; i < removeCount; i++) {
        removeClient(removeTargets[i], true, isBroadcastTaskContext);
    }
}

bool WsClientManager::hasClients() const {
    return _clientCount.load(std::memory_order_acquire) > 0;
}

size_t WsClientManager::getClientCount() const {
    return _clientCount.load(std::memory_order_acquire);
}

httpd_handle_t WsClientManager::getServerHandle() const {
    return _serverHandle;
}

size_t WsClientManager::snapshotClients(int* outTargets, size_t maxCount) const {
    if (!outTargets || maxCount == 0) {
        return 0;
    }

    size_t count = 0;
    SYSTEM::ScopeLock lock(_lock, kClientMapLockTimeout);
    if (!lock.isLocked()) {
        return 0;
    }

    for (const auto& kv : _clients) {
        if (count >= maxCount) {
            break;
        }
        outTargets[count++] = kv.first;
    }

    return count;
}

} // namespace WEBSOCKET
} // namespace API
