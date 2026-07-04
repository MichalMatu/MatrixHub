#pragma once

#include "PsychicHttpServer.h"

class PsychicHttpsServer : public PsychicHttpServer {
public:
    struct {
        httpd_config_t httpd{};
        uint32_t tls_handshake_timeout_ms = 0;
        int port_secure = 443;
    } ssl_config;
};
