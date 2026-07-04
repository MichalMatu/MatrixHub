#pragma once

#include <esp_http_server.h>

namespace API {

/**
 * @brief Interface for authenticating WebSocket connection requests.
 */
class IWebSocketAuthenticator {
public:
    virtual ~IWebSocketAuthenticator() = default;

    /**
     * @brief Authenticate the incoming HTTP request before upgrading to WebSocket.
     * @param req The HTTP request
     * @return true if authenticated, false otherwise
     */
    virtual bool authenticate(httpd_req_t *req) = 0;
};

} // namespace API
