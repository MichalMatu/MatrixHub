#pragma once

#include <Arduino.h>
#include <map>
#include <memory>
#include <string>
#include "esp_http_server.h"
#include "IPAddress.h"

class PsychicWebParameter {
public:
    PsychicWebParameter(const String& name, const String& value) : _name(name), _value(value) {}
    const String& value() const { return _value; }
    const String& name() const { return _name; }

private:
    String _name;
    String _value;
};

class PsychicClient {
public:
    IPAddress remoteIP() const { return _remoteIp; }
    void setRemoteIP(const IPAddress& remoteIp) { _remoteIp = remoteIp; }

    int socket() const { return _socket; }
    void setSocket(int socket) { _socket = socket; }

    bool close() {
        _closed = true;
        _closeCalls++;
        return true;
    }

    bool closed() const { return _closed; }
    int closeCalls() const { return _closeCalls; }

private:
    IPAddress _remoteIp{127, 0, 0, 1};
    int _socket = 1;
    bool _closed = false;
    int _closeCalls = 0;
};

class PsychicRequest {
public:
    PsychicRequest() = default;

    httpd_req_t* request() { return _req; }
    const String contentType() { return _contentType; }
    size_t contentLength() { return _body.size(); }
    const char* body() const { return _body.c_str(); }
    size_t bodyLength() const { return _body.size(); }

    bool hasParam(const char *key) {
        if (!key) return false;
        return _params.count(key) > 0;
    }

    bool hasHeader(const char* key) const {
        if (!key) return false;
        return _headers.count(key) > 0;
    }

    String header(const char* key) const {
        if (!key) return String();
        auto it = _headers.find(key);
        return it == _headers.end() ? String() : it->second;
    }

    PsychicWebParameter* getParam(const char *name) {
        if (!name) return nullptr;
        auto it = _params.find(name);
        if (it == _params.end()) return nullptr;
        return it->second.get();
    }

    PsychicClient* client() { return &_client; }
    const PsychicClient* client() const { return &_client; }

    void loadParams() {}

    String uri() const { return _uri; }
    String path() const { return _uri; }
    void setUri(const std::string& uri) { _uri = uri.c_str(); }

    http_method method() const { return _method; }
    void setMethod(http_method method) { _method = method; }

    bool responseSent() const {
        return lastStatusCode != 0 || !lastResponseBody.empty() || !_lastReply.empty();
    }

    void setBody(const std::string& body) { _body = body; }
    void setContentType(const std::string& contentType) { _contentType = contentType.c_str(); }
    void setParam(const std::string& name, const std::string& value) {
        _params[name] = std::make_unique<PsychicWebParameter>(name.c_str(), value.c_str());
    }
    void setHeader(const std::string& name, const std::string& value) {
        _headers[name] = value.c_str();
    }
    void setRemoteIP(const IPAddress& remoteIp) { _client.setRemoteIP(remoteIp); }
    void setSocket(int socket) { _client.setSocket(socket); }

    esp_err_t reply(const char* content) {
        _lastReply = content ? content : "";
        return ESP_OK;
    }

    esp_err_t reply(int statusCode) {
        lastStatusCode = statusCode;
        return ESP_OK;
    }

    // Test inspection helpers
    int lastStatusCode = 0;
    std::string lastResponseBody;
    std::string _lastReply;

private:
    httpd_req_t* _req = nullptr;
    std::string _body;
    String _contentType;
    std::map<std::string, std::unique_ptr<PsychicWebParameter>> _params;
    std::map<std::string, String> _headers;
    PsychicClient _client;
    String _uri;
    http_method _method = HTTP_GET;
};
