#pragma once

#include <ArduinoJson.h>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "esp_http_server.h"
#include "FS.h"
#include "PsychicRequest.h"

using PsychicHttpRequestCallback = std::function<esp_err_t(PsychicRequest*)>;
using PsychicJsonRequestCallback = std::function<esp_err_t(PsychicRequest*, JsonVariant&)>;
using PsychicRequestFilterFunction = std::function<bool(PsychicRequest*)>;
using PsychicClientCallback = std::function<void(PsychicClient*)>;

class PsychicWebHandler {
public:
    PsychicWebHandler* onRequest(PsychicHttpRequestCallback fn) {
        _requestCallback = std::move(fn);
        return this;
    }

    esp_err_t handleRequest(PsychicRequest* request) {
        return _requestCallback ? _requestCallback(request) : ESP_OK;
    }

private:
    PsychicHttpRequestCallback _requestCallback;
};

class PsychicStaticFileHandler {
public:
    void addHeader(const char* field, const char* value) {
        headers.emplace_back(field ? field : "", value ? value : "");
    }

    std::vector<std::pair<std::string, std::string>> headers;
};

class PsychicHttpServer {
public:
    using JsonCallback = std::function<esp_err_t(PsychicRequest*, JsonVariant&)>;
    httpd_handle_t server = reinterpret_cast<httpd_handle_t>(0x1);
    httpd_config_t config{};
    unsigned long maxRequestBodySize = 16384;

    template <typename Fn>
    void on(const char* path, http_method method, Fn&& handler) {
        const std::string key = routeKey(path, method);

        if constexpr (std::is_invocable_r_v<esp_err_t, Fn, PsychicRequest*, JsonVariant&>) {
            _jsonHandlers[key] = JsonCallback(std::forward<Fn>(handler));
        } else {
            _requestHandlers[key] = PsychicHttpRequestCallback(std::forward<Fn>(handler));
        }
    }

    void on(const char* path, http_method method, PsychicWebHandler* handler) {
        const std::string key = routeKey(path, method);
        _requestHandlers[key] = [handler](PsychicRequest* request) {
            return handler ? handler->handleRequest(request) : ESP_OK;
        };
        _ownedHandlers.emplace_back(handler);
    }

    PsychicStaticFileHandler* serveStatic(const char* uri, fs::FS& fs, const char* path, const char* cache_control = nullptr) {
        (void)uri;
        (void)fs;
        (void)path;
        auto handler = std::make_unique<PsychicStaticFileHandler>();
        if (cache_control) {
            handler->addHeader("Cache-Control", cache_control);
        }
        auto* raw = handler.get();
        _staticHandlers.emplace_back(std::move(handler));
        return raw;
    }

    void onNotFound(PsychicHttpRequestCallback fn) {
        _notFoundHandler = std::move(fn);
    }

    void onOpen(PsychicClientCallback handler) {
        _onOpen = std::move(handler);
    }

    void onClose(PsychicClientCallback handler) {
        _onClose = std::move(handler);
    }

    esp_err_t listen(int port) {
        _lastListenPort = port;
        config.server_port = static_cast<uint16_t>(port);
        return ESP_OK;
    }

    esp_err_t listen(int port, const char* cert, const char* key) {
        _lastListenPort = port;
        _lastListenCert = cert;
        _lastListenKey = key;
        return ESP_OK;
    }

    void stop() {
        _lastListenPort = 0;
    }

    esp_err_t invoke(const char* path, http_method method, PsychicRequest* request) const {
        const auto it = _requestHandlers.find(routeKey(path, method));
        if (it == _requestHandlers.end()) {
            return ESP_ERR_NOT_FOUND;
        }
        return it->second(request);
    }

    esp_err_t invokeJson(const char* path, http_method method, PsychicRequest* request, JsonVariant& json) const {
        const auto it = _jsonHandlers.find(routeKey(path, method));
        if (it == _jsonHandlers.end()) {
            return ESP_ERR_NOT_FOUND;
        }
        return it->second(request, json);
    }

    bool hasRequestHandler(const char* path, http_method method) const {
        return _requestHandlers.count(routeKey(path, method)) > 0;
    }

    bool hasJsonHandler(const char* path, http_method method) const {
        return _jsonHandlers.count(routeKey(path, method)) > 0;
    }

    template <typename Fn>
    void applyToAllClients(Fn&& callback) {
        for (auto& [socket, client] : _clients) {
            (void)socket;
            callback(client);
        }
    }

    PsychicClient* getClient(int socket) {
        const auto it = _clients.find(socket);
        return it == _clients.end() ? nullptr : it->second;
    }

    void registerClient(PsychicClient* client) {
        if (!client) {
            return;
        }
        _clients[client->socket()] = client;
    }

    void clearClients() {
        _clients.clear();
    }

    int lastListenPort() const {
        return _lastListenPort;
    }

private:
    static std::string routeKey(const char* path, http_method method) {
        return std::string(path ? path : "") + "#" + std::to_string(static_cast<int>(method));
    }

    std::map<std::string, PsychicHttpRequestCallback> _requestHandlers;
    std::map<std::string, JsonCallback> _jsonHandlers;
    std::map<int, PsychicClient*> _clients;
    std::vector<std::unique_ptr<PsychicWebHandler>> _ownedHandlers;
    std::vector<std::unique_ptr<PsychicStaticFileHandler>> _staticHandlers;
    PsychicHttpRequestCallback _notFoundHandler;
    PsychicClientCallback _onOpen;
    PsychicClientCallback _onClose;
    int _lastListenPort = 0;
    const char* _lastListenCert = nullptr;
    const char* _lastListenKey = nullptr;
};
