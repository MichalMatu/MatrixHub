#include "WebSocketBroadcaster.h"
#include "../../system/health/network/HttpServerHealthTracker.h"
#include "../../system/logging/Logging.h"
#include <cstring>
#include <utility>
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

    auto producerLease = _taskQueue.acquireProducerLease();
    if (producerLease && !_poolReady.load(std::memory_order_acquire)) {
        producerLease = {};
    }
    if (producerLease) {
        WEBSOCKET::WsMessage msg;
        if (prepareInlineMessage(msg, data, len, type)) {
            (void)producerLease.enqueue(msg);
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
        if (!producerLease.enqueue(msg)) {
            // Pool release is handled inside WsTaskQueue::enqueue on failure
        }
    } else {
        _clientMgr.performBroadcast(data, len, type);
    }
}

void WebSocketBroadcaster::broadcast(int* fds, size_t count, uint8_t* data, size_t len, httpd_ws_type_t type) {
    if (!_clientMgr.hasClients() || !data || len == 0 || count == 0) return;

    auto producerLease = _taskQueue.acquireProducerLease();
    if (producerLease && !_poolReady.load(std::memory_order_acquire)) {
        producerLease = {};
    }
    if (producerLease) {
        WEBSOCKET::WsMessage msg;
        if (prepareInlineMessage(msg, data, len, type)) {
            msg.targetCount = snapshotTargetSessions(
                fds, count, msg.targets, msg.targetGenerations);
            if (msg.targetCount == 0) return;
            (void)producerLease.enqueue(msg);
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

        if (!producerLease.enqueue(msg)) {
            // Pool release is handled inside WsTaskQueue::enqueue on failure
        }
    } else {
        _clientMgr.performBroadcast(data, len, type, fds, count);
    }
}

bool WebSocketBroadcaster::broadcastSerialized(size_t reserveLen,
                                               PayloadWriter writer,
                                               httpd_ws_type_t type) {
    return broadcastSerializedInternal(
        nullptr, 0, false, reserveLen, std::move(writer), type, false);
}

bool WebSocketBroadcaster::broadcastSerialized(int* fds,
                                               size_t count,
                                               size_t reserveLen,
                                               PayloadWriter writer,
                                               httpd_ws_type_t type) {
    return broadcastSerializedInternal(
        fds, count, true, reserveLen, std::move(writer), type, false);
}

bool WebSocketBroadcaster::broadcastSerializedQueued(
    size_t reserveLen,
    PayloadWriter writer,
    httpd_ws_type_t type) {
    return broadcastSerializedInternal(
        nullptr, 0, false, reserveLen, std::move(writer), type, true);
}

bool WebSocketBroadcaster::broadcastSerializedQueued(
    int* fds,
    size_t count,
    size_t reserveLen,
    PayloadWriter writer,
    httpd_ws_type_t type) {
    return broadcastSerializedInternal(
        fds, count, true, reserveLen, std::move(writer), type, true);
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

    // A stopped-but-not-yet-reaped queue may still own pool slots, so lifecycle
    // cleanup must run before init() can replace pool storage. _poolReady keeps
    // producers out of the short queue-created/pool-initializing window.
    _poolReady.store(false, std::memory_order_release);
    _taskQueue.enable(queueSize, stackSize);
    if (!_taskQueue.isEnabled()) {
        return;
    }

    if (!_pool.init(queueSize, payloadSlotSize)) {
        LOGE("[%s] Failed to initialize WebSocket payload pool", _logTag);
        (void)_taskQueue.disable();
        return;
    }
    _poolReady.store(true, std::memory_order_release);
}

void WebSocketBroadcaster::requestQueueStop() {
    _taskQueue.requestStop();
    _poolReady.store(false, std::memory_order_release);
}

bool WebSocketBroadcaster::disableQueue() {
    if (!_taskQueue.disable()) {
        return false;
    }
    // acquireProducerLease() can no longer succeed after disable() returns, so
    // no producer can observe ready=true and then enter pool storage here.
    _poolReady.store(false, std::memory_order_release);
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
                                          bool queued,
                                          uint8_t** payload,
                                          int16_t* payloadSlot,
                                          bool* isAllocated) {
    if (!payload || !payloadSlot || !isAllocated || reserveLen == 0) {
        return false;
    }

    *payload = nullptr;
    *payloadSlot = -1;
    *isAllocated = false;

    if (queued) {
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
        if (queued) {
            SYSTEM::HEALTH::HttpServerHealthTracker::recordWsQueueDrop(reserveLen);
            LOGW("[%s] Queue Drop: OOM", _logTag);
        }
        return false;
    }

    *isAllocated = true;
    return true;
}

bool WebSocketBroadcaster::broadcastSerializedInternal(
    int* fds,
    size_t count,
    bool targeted,
    size_t reserveLen,
    PayloadWriter writer,
    httpd_ws_type_t type,
    bool queuedOnly) {
    if (reserveLen == 0 || !writer || !hasClients() ||
        (targeted && (!fds || count == 0))) {
        return false;
    }

    int targetSnapshots[WEBSOCKET::MAX_BROADCAST_TARGETS]{};
    WEBSOCKET::WsClientGeneration targetGenerations[WEBSOCKET::MAX_BROADCAST_TARGETS]{};
    size_t targetCount = 0;
    if (targeted) {
        targetCount = snapshotTargetSessions(
            fds, count, targetSnapshots, targetGenerations);
        if (targetCount == 0) {
            return false;
        }
    }

    auto producerLease = _taskQueue.acquireProducerLease();
    if (producerLease && !_poolReady.load(std::memory_order_acquire)) {
        producerLease = {};
    }
    if (queuedOnly && !producerLease) {
        return false;
    }

    uint8_t* payload = nullptr;
    int16_t payloadSlot = -1;
    bool isAllocated = false;
    if (!acquirePayload(
            reserveLen,
            static_cast<bool>(producerLease),
            &payload,
            &payloadSlot,
            &isAllocated)) {
        return false;
    }

    const size_t written = writer(payload, reserveLen);
    if (written == 0 || written > reserveLen) {
        WEBSOCKET::WsMessage msg = {
            payload, reserveLen, type, isAllocated, payloadSlot, {0}, 0};
        _pool.releaseMessageResources(msg);
        return false;
    }

    return broadcastPrepared(
        targeted ? targetSnapshots : nullptr,
        targeted ? targetGenerations : nullptr,
        targeted ? targetCount : 0,
        payload,
        written,
        payloadSlot,
        isAllocated,
        type,
        producerLease ? &producerLease : nullptr);
}

bool WebSocketBroadcaster::broadcastPrepared(
    int* fds,
    const WEBSOCKET::WsClientGeneration* targetGenerations,
    size_t count,
    uint8_t* payload,
    size_t len,
    int16_t payloadSlot,
    bool isAllocated,
    httpd_ws_type_t type,
    WEBSOCKET::WsTaskQueue::ProducerLease* producerLease) {
    if (!payload || len == 0) {
        WEBSOCKET::WsMessage msg = { payload, len, type, isAllocated, payloadSlot, {0}, 0 };
        _pool.releaseMessageResources(msg);
        return false;
    }

    if (producerLease && *producerLease) {
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
        return producerLease->enqueue(msg);
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
