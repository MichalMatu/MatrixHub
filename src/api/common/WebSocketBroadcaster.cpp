#include "WebSocketBroadcaster.h"
#include "../../system/health/network/HttpServerHealthTracker.h"
#include "../../system/logging/Logging.h"
#include <cstring>
#include <esp_heap_caps.h>
#include <freertos/task.h>

#undef LOG_TAG
#define LOG_TAG "WSBroadcast"

namespace API {

namespace {

bool prepareInlineMessage(WEBSOCKET::WsMessage& msg,
                          const uint8_t* data,
                          size_t len,
                          httpd_ws_type_t type) {
    if (!data || len == 0 || len > WEBSOCKET::INLINE_PAYLOAD_CAPACITY) {
        return false;
    }

    memset(&msg, 0, sizeof(msg));
    memcpy(msg.inlineData, data, len);
    msg.data = nullptr;
    msg.len = len;
    msg.type = type;
    msg.isAllocated = false;
    msg.payloadSlot = -1;
    msg.targetCount = 0;
    msg.isInline = true;
    return true;
}

}

WebSocketBroadcaster::WebSocketBroadcaster(const char* logTag, 
                                           IWebSocketAuthenticator* authenticator,
                                           StateChangeCallback onStateChange, 
                                           uint32_t sendTimeoutMs)
    : _logTag(logTag),
      _pool(logTag),
      _clientMgr(logTag, authenticator, onStateChange, sendTimeoutMs),
      _taskQueue(logTag, [this](WEBSOCKET::WsMessage& msg) { this->processBroadcast(msg); }, &_pool) {
}

WebSocketBroadcaster::~WebSocketBroadcaster() {
    while (!disableQueue()) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t WebSocketBroadcaster::handleHandshake(httpd_req_t *req) {
    return _clientMgr.handleHandshake(req);
}

void WebSocketBroadcaster::removeClient(int fd, bool triggerClose) {
    _clientMgr.removeClient(fd, triggerClose, false);
}

void WebSocketBroadcaster::broadcast(uint8_t* data, size_t len, httpd_ws_type_t type) {
    if (!_clientMgr.hasClients()) return;
    
    if (!data || len == 0) {
        _clientMgr.performBroadcast(data, len, type);
        return;
    }

    if (_taskQueue.isEnabled()) {
        WEBSOCKET::WsMessage msg;
        if (prepareInlineMessage(msg, data, len, type)) {
            (void)_taskQueue.enqueue(msg);
            return;
        }

        uint8_t* payload = nullptr;
        int16_t payloadSlot = -1;
        bool isAllocated = false;
        const size_t slotSize = _pool.getSlotSize();

        if (slotSize > 0 && len <= slotSize) {
            if (!_pool.acquireSlot(len, &payload, &payloadSlot)) return;
        } else {
            // Stay compatible with payloads larger than the fixed slot size.
            // This keeps less common channels alive while /ws/system benefits
            // from fixed buffers on the common path.
            SYSTEM::HEALTH::HttpServerHealthTracker::recordWsHeapFallback(len);
            payload = (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
            if (!payload) {
                SYSTEM::HEALTH::HttpServerHealthTracker::recordWsQueueDrop(len);
                LOGW("[%s] Queue Drop: OOM", _logTag);
                return;
            }
            isAllocated = true;
        }

        memcpy(payload, data, len);

        msg = { payload, len, type, isAllocated, payloadSlot, {0}, 0 };
        if (!_taskQueue.enqueue(msg)) {
            // Pool release is handled inside WsTaskQueue::enqueue on failure
        }
    } else {
        _clientMgr.performBroadcast(data, len, type);
    }
}

void WebSocketBroadcaster::broadcast(int* fds, size_t count, uint8_t* data, size_t len, httpd_ws_type_t type) {
    if (!_clientMgr.hasClients() || !data || len == 0 || count == 0) return;

    if (_taskQueue.isEnabled()) {
        WEBSOCKET::WsMessage msg;
        if (prepareInlineMessage(msg, data, len, type)) {
            msg.targetCount = snapshotTargetSessions(
                fds, count, msg.targets, msg.targetGenerations);
            if (msg.targetCount == 0) return;
            (void)_taskQueue.enqueue(msg);
            return;
        }

        int targetSnapshots[WEBSOCKET::MAX_BROADCAST_TARGETS]{};
        WEBSOCKET::WsClientGeneration targetGenerations[WEBSOCKET::MAX_BROADCAST_TARGETS]{};
        const size_t targetCount = snapshotTargetSessions(
            fds, count, targetSnapshots, targetGenerations);
        if (targetCount == 0) return;

        uint8_t* payload = nullptr;
        int16_t payloadSlot = -1;
        bool isAllocated = false;
        const size_t slotSize = _pool.getSlotSize();

        if (slotSize > 0 && len <= slotSize) {
            if (!_pool.acquireSlot(len, &payload, &payloadSlot)) return;
        } else {
            // Multi-cast uses the same rule: fixed slot when it fits, heap
            // fallback only for oversized payloads.
            SYSTEM::HEALTH::HttpServerHealthTracker::recordWsHeapFallback(len);
            payload = (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
            if (!payload) {
                SYSTEM::HEALTH::HttpServerHealthTracker::recordWsQueueDrop(len);
                LOGW("[%s] Queue Drop: OOM", _logTag);
                return;
            }
            isAllocated = true;
        }

        memcpy(payload, data, len);

        msg = { payload, len, type, isAllocated, payloadSlot, {0}, 0 };
        memcpy(msg.targets, targetSnapshots, targetCount * sizeof(int));
        memcpy(msg.targetGenerations,
               targetGenerations,
               targetCount * sizeof(WEBSOCKET::WsClientGeneration));
        msg.targetCount = targetCount;

        if (!_taskQueue.enqueue(msg)) {
            // Pool release is handled inside WsTaskQueue::enqueue on failure
        }
    } else {
        _clientMgr.performBroadcast(data, len, type, fds, count);
    }
}

bool WebSocketBroadcaster::broadcastSerialized(size_t reserveLen,
                                               PayloadWriter writer,
                                               httpd_ws_type_t type) {
    if (!hasClients() || reserveLen == 0 || !writer) {
        return false;
    }

    uint8_t* payload = nullptr;
    int16_t payloadSlot = -1;
    bool isAllocated = false;
    if (!acquirePayload(reserveLen, &payload, &payloadSlot, &isAllocated)) {
        return false;
    }

    const size_t written = writer(payload, reserveLen);
    if (written == 0 || written > reserveLen) {
        WEBSOCKET::WsMessage msg = { payload, reserveLen, type, isAllocated, payloadSlot, {0}, 0 };
        _pool.releaseMessageResources(msg);
        return false;
    }

    return broadcastPrepared(
        nullptr, nullptr, 0, payload, written, payloadSlot, isAllocated, type);
}

bool WebSocketBroadcaster::broadcastSerialized(int* fds,
                                               size_t count,
                                               size_t reserveLen,
                                               PayloadWriter writer,
                                               httpd_ws_type_t type) {
    if (!fds || count == 0 || reserveLen == 0 || !writer || !hasClients()) {
        return false;
    }

    int targetSnapshots[WEBSOCKET::MAX_BROADCAST_TARGETS]{};
    WEBSOCKET::WsClientGeneration targetGenerations[WEBSOCKET::MAX_BROADCAST_TARGETS]{};
    const size_t targetCount = snapshotTargetSessions(
        fds, count, targetSnapshots, targetGenerations);
    if (targetCount == 0) {
        return false;
    }

    uint8_t* payload = nullptr;
    int16_t payloadSlot = -1;
    bool isAllocated = false;
    if (!acquirePayload(reserveLen, &payload, &payloadSlot, &isAllocated)) {
        return false;
    }

    const size_t written = writer(payload, reserveLen);
    if (written == 0 || written > reserveLen) {
        WEBSOCKET::WsMessage msg = { payload, reserveLen, type, isAllocated, payloadSlot, {0}, 0 };
        _pool.releaseMessageResources(msg);
        return false;
    }

    return broadcastPrepared(
        targetSnapshots,
        targetGenerations,
        targetCount,
        payload,
        written,
        payloadSlot,
        isAllocated,
        type);
}

void WebSocketBroadcaster::processBroadcast(WEBSOCKET::WsMessage& msg) {
    if (msg.targetCount > 0) {
        _clientMgr.performBroadcast(
            msg.data,
            msg.len,
            msg.type,
            msg.targets,
            msg.targetCount,
            true,
            msg.targetGenerations);
    } else {
        _clientMgr.performBroadcast(msg.data, msg.len, msg.type, nullptr, 0, true);
    }
}

void WebSocketBroadcaster::enableQueue(size_t queueSize, uint32_t stackSize, size_t payloadSlotSize) {
    if (_taskQueue.isEnabled()) {
        return;
    }

    _taskQueue.enable(queueSize, stackSize);
    if (!_taskQueue.isEnabled()) {
        return;
    }

    if (!_pool.init(queueSize, payloadSlotSize)) {
        LOGE("[%s] Failed to initialize WebSocket payload pool", _logTag);
        (void)_taskQueue.disable();
    }
}

bool WebSocketBroadcaster::disableQueue() {
    if (!_taskQueue.disable()) {
        return false;
    }
    _pool.deinit();
    return true;
}

bool WebSocketBroadcaster::hasClients() const {
    return _clientMgr.hasClients();
}

size_t WebSocketBroadcaster::getClientCount() const {
    return _clientMgr.getClientCount();
}

httpd_handle_t WebSocketBroadcaster::getServerHandle() const {
    return _clientMgr.getServerHandle();
}

bool WebSocketBroadcaster::isQueueEnabled() const {
    return _taskQueue.isEnabled();
}

size_t WebSocketBroadcaster::snapshotClients(int* outTargets, size_t maxCount) const {
    return _clientMgr.snapshotClients(outTargets, maxCount);
}

bool WebSocketBroadcaster::markClientReady(int fd) {
    return _clientMgr.markClientReady(fd);
}

size_t WebSocketBroadcaster::snapshotTargetSessions(
    const int* fds,
    size_t count,
    int* outTargets,
    WEBSOCKET::WsClientGeneration* outGenerations) const {
    return _clientMgr.snapshotTargetSessions(
        fds,
        count,
        outTargets,
        outGenerations,
        WEBSOCKET::MAX_BROADCAST_TARGETS);
}

bool WebSocketBroadcaster::acquirePayload(size_t reserveLen,
                                          uint8_t** payload,
                                          int16_t* payloadSlot,
                                          bool* isAllocated) {
    if (!payload || !payloadSlot || !isAllocated || reserveLen == 0) {
        return false;
    }

    *payload = nullptr;
    *payloadSlot = -1;
    *isAllocated = false;

    if (_taskQueue.isEnabled()) {
        const size_t slotSize = _pool.getSlotSize();
        if (slotSize > 0 && reserveLen <= slotSize) {
            if (_pool.acquireSlot(reserveLen, payload, payloadSlot)) {
                return true;
            }
            return false;
        }

        SYSTEM::HEALTH::HttpServerHealthTracker::recordWsHeapFallback(reserveLen);
    }

    *payload = static_cast<uint8_t*>(heap_caps_malloc(reserveLen, MALLOC_CAP_SPIRAM));
    if (!*payload) {
        if (_taskQueue.isEnabled()) {
            SYSTEM::HEALTH::HttpServerHealthTracker::recordWsQueueDrop(reserveLen);
            LOGW("[%s] Queue Drop: OOM", _logTag);
        }
        return false;
    }

    *isAllocated = true;
    return true;
}

bool WebSocketBroadcaster::broadcastPrepared(
    int* fds,
    const WEBSOCKET::WsClientGeneration* targetGenerations,
    size_t count,
    uint8_t* payload,
    size_t len,
    int16_t payloadSlot,
    bool isAllocated,
    httpd_ws_type_t type) {
    if (!payload || len == 0) {
        WEBSOCKET::WsMessage msg = { payload, len, type, isAllocated, payloadSlot, {0}, 0 };
        _pool.releaseMessageResources(msg);
        return false;
    }

    if (_taskQueue.isEnabled()) {
        WEBSOCKET::WsMessage msg = { payload, len, type, isAllocated, payloadSlot, {0}, 0 };
        if (fds && count > 0) {
            const size_t actualCount =
                (count > WEBSOCKET::MAX_BROADCAST_TARGETS) ? WEBSOCKET::MAX_BROADCAST_TARGETS : count;
            memcpy(msg.targets, fds, actualCount * sizeof(int));
            memcpy(msg.targetGenerations,
                   targetGenerations,
                   actualCount * sizeof(WEBSOCKET::WsClientGeneration));
            msg.targetCount = actualCount;
        }
        return _taskQueue.enqueue(msg);
    }

    if (fds && count > 0) {
        _clientMgr.performBroadcast(
            payload, len, type, fds, count, false, targetGenerations);
    } else {
        _clientMgr.performBroadcast(payload, len, type);
    }

    WEBSOCKET::WsMessage msg = { payload, len, type, isAllocated, payloadSlot, {0}, 0 };
    _pool.releaseMessageResources(msg);
    return true;
}

} // namespace API
