#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include <PsychicHttpServer.h>
#include <esp_http_server.h>

#include "WebSocketBroadcaster.h"

namespace API {

class IWebSocketAuthenticator;

// Shared runtime for every websocket endpoint. The goal is that endpoint-specific
// classes own only domain logic (system channels, CSI frames, terminal commands),
// while registration, auth, open/close lifecycle and queue-backed broadcasting
// stay uniform across /ws/system, /ws/csi and /ws/usbterminal.
class WsEndpointRuntime {
public:
    using FrameHandler = std::function<esp_err_t(httpd_req_t* req, int fd)>;
    using ClientCallback = std::function<void(int fd)>;
    using RequestCallback = std::function<void()>;

    WsEndpointRuntime(PsychicHttpServer* server,
                      const char* uri,
                      const char* logTag,
                      IWebSocketAuthenticator* authenticator = nullptr,
                      WebSocketBroadcaster::StateChangeCallback onStateChange = nullptr,
                      uint32_t sendTimeoutMs = 500);

    void setFrameHandler(FrameHandler handler);
    void setOpenCallback(ClientCallback callback);
    void setCleanupCallback(ClientCallback callback);
    void setRequestCallback(RequestCallback callback);

    void begin();
    void begin(size_t queueSize, uint32_t stackSize, size_t payloadSlotSize = 0);

    esp_err_t handleRequest(httpd_req_t* req);
    void cleanupClient(int fd);

    WebSocketBroadcaster& broadcaster();
    const WebSocketBroadcaster& broadcaster() const;

private:
    PsychicHttpServer* _server = nullptr;
    const char* _uri = nullptr;
    WebSocketBroadcaster _broadcaster;
    FrameHandler _frameHandler;
    ClientCallback _openCallback;
    ClientCallback _cleanupCallback;
    RequestCallback _requestCallback;

    static esp_err_t wsHandler(httpd_req_t* req);
};

} // namespace API
