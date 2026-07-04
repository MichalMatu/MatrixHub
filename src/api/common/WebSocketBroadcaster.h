#pragma once

#include <cstdint>
#include <functional>
#include <esp_http_server.h>
#include "websocket/WsTypes.h"
#include "websocket/WsPayloadPool.h"
#include "websocket/WsTaskQueue.h"
#include "websocket/WsClientManager.h"

namespace API {
class IWebSocketAuthenticator;

/**
 * @brief Thread-safe WebSocket broadcaster facade (Phase 4 Refactoring).
 */
class WebSocketBroadcaster {
public:
    using StateChangeCallback = WEBSOCKET::WsClientManager::StateChangeCallback;
    using PayloadWriter = std::function<size_t(uint8_t* buffer, size_t capacity)>;

    WebSocketBroadcaster(const char* logTag, 
                         IWebSocketAuthenticator* authenticator = nullptr, 
                         StateChangeCallback onStateChange = nullptr,
                         uint32_t sendTimeoutMs = 500);
    ~WebSocketBroadcaster();

    esp_err_t handleHandshake(httpd_req_t *req);
    void removeClient(int fd, bool triggerClose = false);

    void broadcast(uint8_t* data, size_t len, httpd_ws_type_t type = HTTPD_WS_TYPE_BINARY);
    void broadcast(int* fds, size_t count, uint8_t* data, size_t len, httpd_ws_type_t type = HTTPD_WS_TYPE_BINARY);
    // The direct-serialize path exists to avoid building a temporary payload
    // first and copying it into the websocket queue later. This is the intended
    // hot path for snapshots, CSI frames and terminal JSON once payload shape is
    // known up front.
    bool broadcastSerialized(size_t reserveLen, PayloadWriter writer, httpd_ws_type_t type = HTTPD_WS_TYPE_BINARY);
    bool broadcastSerialized(int* fds, size_t count, size_t reserveLen, PayloadWriter writer, httpd_ws_type_t type = HTTPD_WS_TYPE_BINARY);

    void enableQueue(size_t queueSize = 10, uint32_t stackSize = 4096, size_t payloadSlotSize = 0);
    bool disableQueue();

    bool hasClients() const;
    size_t getClientCount() const;
    httpd_handle_t getServerHandle() const;
    bool isQueueEnabled() const;
    // Snapshotting active client fds lets higher layers do targeted sends
    // without keeping their own duplicate client registries.
    size_t snapshotClients(int* outTargets, size_t maxCount) const;

private:
    const char* _logTag;
    WEBSOCKET::WsPayloadPool _pool;
    WEBSOCKET::WsClientManager _clientMgr;
    WEBSOCKET::WsTaskQueue _taskQueue;

    void processBroadcast(WEBSOCKET::WsMessage& msg);
    bool acquirePayload(size_t reserveLen, uint8_t** payload, int16_t* payloadSlot, bool* isAllocated);
    bool broadcastPrepared(int* fds, size_t count, uint8_t* payload, size_t len, int16_t payloadSlot, bool isAllocated, httpd_ws_type_t type);
};

} // namespace API
