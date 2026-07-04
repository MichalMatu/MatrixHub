#include "WsEndpointRuntime.h"

#include "../../system/logging/Logging.h"

namespace API {

WsEndpointRuntime::WsEndpointRuntime(PsychicHttpServer* server,
                                     const char* uri,
                                     const char* logTag,
                                     IWebSocketAuthenticator* authenticator,
                                     WebSocketBroadcaster::StateChangeCallback onStateChange,
                                     uint32_t sendTimeoutMs)
    : _server(server),
      _uri(uri),
      _broadcaster(logTag, authenticator, onStateChange, sendTimeoutMs) {
}

void WsEndpointRuntime::setFrameHandler(FrameHandler handler) {
    _frameHandler = std::move(handler);
}

void WsEndpointRuntime::setOpenCallback(ClientCallback callback) {
    _openCallback = std::move(callback);
}

void WsEndpointRuntime::setCleanupCallback(ClientCallback callback) {
    _cleanupCallback = std::move(callback);
}

void WsEndpointRuntime::setRequestCallback(RequestCallback callback) {
    _requestCallback = std::move(callback);
}

void WsEndpointRuntime::begin() {
    if (!_server || !_uri) {
        LOGE("WS endpoint runtime missing server or uri");
        return;
    }

    httpd_uri_t wsUri = {
        .uri = _uri,
        .method = HTTP_GET,
        .handler = wsHandler,
        .user_ctx = this,
        .is_websocket = true
    };

    const esp_err_t err = httpd_register_uri_handler(_server->server, &wsUri);
    if (err != ESP_OK) {
        LOGE("Failed to register %s handler: %s", _uri, esp_err_to_name(err));
    }
}

void WsEndpointRuntime::begin(size_t queueSize, uint32_t stackSize, size_t payloadSlotSize) {
    begin();
    if (queueSize > 0) {
        _broadcaster.enableQueue(queueSize, stackSize, payloadSlotSize);
    }
}

esp_err_t WsEndpointRuntime::handleRequest(httpd_req_t* req) {
    if (!req) {
        return ESP_FAIL;
    }

    // Keep request-side bookkeeping in one place so every endpoint reports the
    // same activity and future diagnostics can be added without touching all
    // websocket services separately.
    if (_requestCallback) {
        _requestCallback();
    }

    if (req->method == HTTP_GET) {
        // HTTP_GET is the websocket upgrade path. Domain handlers should not
        // need to know about auth or client registration details.
        const esp_err_t err = _broadcaster.handleHandshake(req);
        if (err == ESP_OK && _openCallback) {
            _openCallback(httpd_req_to_sockfd(req));
        }
        return err;
    }

    if (_frameHandler) {
        // After the socket is open, only payload parsing remains endpoint-
        // specific. This split is intentional: transport logic lives here,
        // business logic lives in the endpoint service.
        return _frameHandler(req, httpd_req_to_sockfd(req));
    }

    return _broadcaster.handleHandshake(req);
}

void WsEndpointRuntime::cleanupClient(int fd) {
    // Cleanup order matters: first remove the transport-level client so no more
    // sends target it, then let the endpoint release domain state such as
    // channel subscriptions or terminal ownership.
    _broadcaster.removeClient(fd);
    if (_cleanupCallback) {
        _cleanupCallback(fd);
    }
}

WebSocketBroadcaster& WsEndpointRuntime::broadcaster() {
    return _broadcaster;
}

const WebSocketBroadcaster& WsEndpointRuntime::broadcaster() const {
    return _broadcaster;
}

esp_err_t WsEndpointRuntime::wsHandler(httpd_req_t* req) {
    auto* runtime = static_cast<WsEndpointRuntime*>(req ? req->user_ctx : nullptr);
    if (!runtime) {
        return ESP_FAIL;
    }

    return runtime->handleRequest(req);
}

} // namespace API
