#pragma once

#include <ArduinoJson.h>
#include <PsychicHttpServer.h>
#include <core/HttpEndpoint.h>
#include <freertos/FreeRTOS.h>
#include <memory>

#include "../BaseApiService.h"
#include "../common/JwtAuthenticator.h"
#include "../common/WsEndpointRuntime.h"
#include "internal/UsbTerminalWireCodec.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../usb_terminal/UsbTerminalTypes.h"

namespace USB_TERMINAL {
class UsbTerminalService;
class UsbTerminalSettingsService;
}

namespace API {

class UsbTerminalApiService : public BaseApiService {
public:
    UsbTerminalApiService(PsychicHttpServer* server,
                          SecurityManager* securityManager,
                          POWER::PowerManager* powerManager,
                          USB_TERMINAL::UsbTerminalService* terminalService,
                          USB_TERMINAL::UsbTerminalSettingsService* settings);
    ~UsbTerminalApiService() override;

    void begin() override;
    void cleanupClient(int fd) override;

private:
    StateHandlerResult validateConfigUpdate(PsychicRequest* request, JsonObject& jsonObject);

    void broadcastSessionToAll();
    void sendSessionToFd(int fd);
    void sendAck(int fd, const char* action, const USB_TERMINAL::CommandAck& ack);
    void sendError(int fd, const char* message);
    void sendOutputToFd(int fd, USB_TERMINAL::OutputPhase phase, const char* text);
    esp_err_t handleFrame(httpd_req_t* req, int fd);
    esp_err_t rejectOversizeFrame(int fd, size_t frameLen, size_t maxLen);
    void handleClientCleanup(int fd);

    // Borrowed runtime service owned by ServiceRegistry.
    USB_TERMINAL::UsbTerminalService* _terminalService;
    // Borrowed settings service owned by ServiceRegistry so persistent terminal
    // config follows the same lifecycle as the websocket/runtime service.
    USB_TERMINAL::UsbTerminalSettingsService* _settings;
    std::unique_ptr<HttpEndpoint<RTC::UsbTerminalData>> _configEndpoint;

    JwtAuthenticator _wsAuthenticator;
    WsEndpointRuntime _wsEndpoint;
};

} // namespace API
