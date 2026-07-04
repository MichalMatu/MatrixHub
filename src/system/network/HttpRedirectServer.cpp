#include "HttpRedirectServer.h"

#include <cstdio>
#include <cstring>

#include <WiFi.h>

#include "../../config/Network.h"
#include "../logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "HttpRedirect"

namespace SYSTEM::NETWORK {
namespace {

constexpr const char* kHttpsPrefix = "https://";
constexpr size_t kHttpsPrefixLen = 8;
constexpr size_t kMaxRedirectHostLen = 96;
constexpr size_t kMaxRedirectUriLen = 384;
constexpr size_t kRedirectLocationBufferLen = 512;
constexpr size_t kFallbackHostBufferLen = 96;
constexpr size_t kHostHeaderBufferLen = 128;

size_t boundedLen(const char* text, size_t maxLen) {
    if (!text) {
        return 0;
    }
    size_t len = 0;
    while (len <= maxLen && text[len] != '\0') {
        ++len;
    }
    return len;
}

bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

void trimHost(const char* rawHost, size_t rawLen, size_t& start, size_t& len) {
    start = 0;
    len = rawLen;

    while (start < rawLen && isSpace(rawHost[start])) {
        ++start;
    }
    while (len > start && isSpace(rawHost[len - 1])) {
        --len;
    }
    len -= start;

    if (len >= 7 && strncmp(rawHost + start, "http://", 7) == 0) {
        start += 7;
        len -= 7;
    } else if (len >= kHttpsPrefixLen && strncmp(rawHost + start, kHttpsPrefix, kHttpsPrefixLen) == 0) {
        start += kHttpsPrefixLen;
        len -= kHttpsPrefixLen;
    }

    for (size_t i = 0; i < len; ++i) {
        if (rawHost[start + i] == '/') {
            len = i;
            break;
        }
    }

    if (len > 0 && rawHost[start] == '[') {
        for (size_t i = 1; i < len; ++i) {
            if (rawHost[start + i] == ']') {
                len = i + 1;
                break;
            }
        }
    } else if (len > 0) {
        for (size_t i = 0; i < len; ++i) {
            if (rawHost[start + i] == ':') {
                len = i;
                break;
            }
        }
    }
}

void formatIp(char* out, size_t outSize, IPAddress ip) {
    if (!out || outSize == 0) {
        return;
    }
    snprintf(out,
             outSize,
             "%u.%u.%u.%u",
             static_cast<unsigned>(ip[0]),
             static_cast<unsigned>(ip[1]),
             static_cast<unsigned>(ip[2]),
             static_cast<unsigned>(ip[3]));
}

void buildFallbackHost(char* out, size_t outSize) {
    if (!out || outSize == 0) {
        return;
    }
    out[0] = '\0';

    const char* hostname = WiFi.getHostname();
    if (hostname && hostname[0] != '\0') {
        snprintf(out, outSize, "%s.local", hostname);
        return;
    }

    formatIp(out, outSize, WiFi.localIP());
}

esp_err_t sendPlainResponse(httpd_req_t* req, const char* status, const char* message) {
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, message, static_cast<ssize_t>(strlen(message)));
}

}  // namespace

RedirectLocationStatus buildHttpsRedirectLocation(const char* hostHeader,
                                                  const char* uri,
                                                  const char* fallbackHost,
                                                  char* out,
                                                  size_t outSize) {
    if (!out || outSize == 0) {
        return RedirectLocationStatus::BufferTooSmall;
    }
    out[0] = '\0';

    const char* selectedHost = (hostHeader && hostHeader[0] != '\0') ? hostHeader : fallbackHost;
    if (!selectedHost || selectedHost[0] == '\0') {
        return RedirectLocationStatus::MissingHost;
    }

    const size_t rawHostLen = boundedLen(selectedHost, kMaxRedirectHostLen);
    if (rawHostLen > kMaxRedirectHostLen) {
        return RedirectLocationStatus::HostTooLong;
    }

    size_t hostStart = 0;
    size_t hostLen = 0;
    trimHost(selectedHost, rawHostLen, hostStart, hostLen);
    if (hostLen == 0) {
        return RedirectLocationStatus::MissingHost;
    }

    const char* selectedUri = (uri && uri[0] == '/') ? uri : "/";
    const size_t uriLen = boundedLen(selectedUri, kMaxRedirectUriLen);
    if (uriLen > kMaxRedirectUriLen) {
        return RedirectLocationStatus::UriTooLong;
    }

    const size_t needed = kHttpsPrefixLen + hostLen + uriLen + 1;
    if (needed > outSize) {
        return RedirectLocationStatus::BufferTooSmall;
    }

    memcpy(out, kHttpsPrefix, kHttpsPrefixLen);
    memcpy(out + kHttpsPrefixLen, selectedHost + hostStart, hostLen);
    memcpy(out + kHttpsPrefixLen + hostLen, selectedUri, uriLen);
    out[needed - 1] = '\0';
    return RedirectLocationStatus::Ok;
}

void HttpRedirectServer::applySmallServerConfig(PsychicHttpServer& server,
                                                const httpd_config_t& baseConfig) {
    server.config.task_priority = baseConfig.task_priority;
    server.config.max_open_sockets = NET::HTTP::REDIRECT_MAX_OPEN_SOCKETS;
    server.config.max_uri_handlers = NET::HTTP::REDIRECT_MAX_URI_HANDLERS;
    server.config.backlog_conn = NET::HTTP::REDIRECT_BACKLOG_CONNECTIONS;
    server.config.max_req_hdr_len = NET::HTTP::MAX_REQUEST_HEADER_BYTES;
    server.config.lru_purge_enable = NET::HTTP::LRU_PURGE_ENABLE;
    server.config.recv_wait_timeout = NET::HTTP::REDIRECT_SESSION_TIMEOUT_SEC;
    server.config.send_wait_timeout = NET::HTTP::REDIRECT_SESSION_TIMEOUT_SEC;
    server.config.stack_size = NET::HTTP::REDIRECT_SERVER_STACK_SIZE_BYTES;
    server.config.core_id = baseConfig.core_id;
    server.config.ctrl_port = NET::HTTP::REDIRECT_CTRL_PORT;
}

bool HttpRedirectServer::begin(const httpd_config_t& baseConfig) {
    if (_started) {
        return true;
    }

    applySmallServerConfig(_server, baseConfig);
    _server.onNotFound(HttpRedirectServer::handleRedirect);

    const esp_err_t err = _server.listen(API::DEFAULT_HTTP_PORT);
    if (err != ESP_OK) {
        LOGE("HTTP redirect server start failed on port %u: %s",
             static_cast<unsigned>(API::DEFAULT_HTTP_PORT),
             esp_err_to_name(err));
        return false;
    }

    _started = true;
    LOGI("Started HTTP redirect server on port %u -> HTTPS 443 (sockets=%d stack=%lu)",
         static_cast<unsigned>(API::DEFAULT_HTTP_PORT),
         _server.config.max_open_sockets,
         static_cast<unsigned long>(_server.config.stack_size));
    return true;
}

void HttpRedirectServer::stop() {
    if (!_started) {
        return;
    }
    _server.stop();
    _started = false;
}

esp_err_t HttpRedirectServer::handleRedirect(PsychicRequest* request) {
    if (!request || !request->request()) {
        return ESP_FAIL;
    }

    httpd_req_t* raw = request->request();
    char host[kHostHeaderBufferLen] = {0};
    char fallbackHost[kFallbackHostBufferLen] = {0};
    char location[kRedirectLocationBufferLen] = {0};

    const size_t hostLen = httpd_req_get_hdr_value_len(raw, "Host");
    if (hostLen > 0) {
        if (hostLen >= sizeof(host) ||
            httpd_req_get_hdr_value_str(raw, "Host", host, sizeof(host)) != ESP_OK) {
            return sendPlainResponse(raw, "400 Bad Request", "Invalid Host header");
        }
    }

    buildFallbackHost(fallbackHost, sizeof(fallbackHost));
    const RedirectLocationStatus status =
        buildHttpsRedirectLocation(host, raw->uri, fallbackHost, location, sizeof(location));

    if (status == RedirectLocationStatus::MissingHost) {
        return sendPlainResponse(raw, "400 Bad Request", "Missing Host header");
    }
    if (status != RedirectLocationStatus::Ok) {
        return sendPlainResponse(raw, "414 URI Too Long", "Redirect URL too long");
    }

    httpd_resp_set_status(raw, "301 Moved Permanently");
    httpd_resp_set_type(raw, "text/plain");
    httpd_resp_set_hdr(raw, "Location", location);
    httpd_resp_set_hdr(raw, "Cache-Control", "no-store");
    return httpd_resp_send(raw, "Redirecting to HTTPS", 20);
}

}  // namespace SYSTEM::NETWORK
