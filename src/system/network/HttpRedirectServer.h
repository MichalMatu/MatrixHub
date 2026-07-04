#pragma once

#include <cstddef>
#include <cstdint>

#include <esp_err.h>
#include <esp_http_server.h>
#include <PsychicHttpServer.h>

class PsychicRequest;

namespace SYSTEM::NETWORK {

enum class RedirectLocationStatus : uint8_t {
    Ok,
    MissingHost,
    HostTooLong,
    UriTooLong,
    BufferTooSmall,
};

RedirectLocationStatus buildHttpsRedirectLocation(const char* hostHeader,
                                                  const char* uri,
                                                  const char* fallbackHost,
                                                  char* out,
                                                  size_t outSize);

class HttpRedirectServer {
public:
    bool begin(const httpd_config_t& baseConfig);
    void stop();
    bool isStarted() const { return _started; }

private:
    static esp_err_t handleRedirect(PsychicRequest* request);
    static void applySmallServerConfig(PsychicHttpServer& server, const httpd_config_t& baseConfig);

    PsychicHttpServer _server;
    bool _started = false;
};

}  // namespace SYSTEM::NETWORK
