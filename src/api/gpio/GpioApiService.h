#pragma once

#include <PsychicHttpServer.h>
#include <security/SecurityManager.h>

#include "../BaseApiService.h"
#include "../../gpio/GpioTypes.h"

namespace GPIO {
class GpioService;
}

namespace Utils {
class JsonResponseWriter;
}

namespace API {

class GpioApiService : public BaseApiService {
public:
    GpioApiService(PsychicHttpServer* server,
                   SecurityManager* securityManager,
                   POWER::PowerManager* powerManager,
                   GPIO::GpioService* service);

    void begin() override;

private:
    GPIO::GpioService* _service;

    esp_err_t handlePins(PsychicRequest* request);
    esp_err_t handleConfigGet(PsychicRequest* request);
    esp_err_t handleConfigPost(PsychicRequest* request);
    esp_err_t handleStatus(PsychicRequest* request);
    esp_err_t handleOutput(PsychicRequest* request);

    static bool writeChannelConfig(Utils::JsonResponseWriter& w,
                                   const GPIO::GpioChannelConfig& channel);
    static bool writeChannelStatus(Utils::JsonResponseWriter& w,
                                   const GPIO::GpioChannelStatus& status);
};

}  // namespace API
