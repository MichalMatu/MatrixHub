#pragma once

#include <Arduino.h>
#include <core/HttpEndpoint.h>
#include <memory>

#include "../BaseApiService.h"
#include "../../macros/MacroService.h"
#include "../../system/rtc/RtcConfig.h"

namespace MACROS {
class MacroSettingsService;
}

namespace MACROS {

class MacroApiService : public API::BaseApiService {
public:
    MacroApiService(PsychicHttpServer* server,
                    SecurityManager* securityManager,
                    POWER::PowerManager* powerManager,
                    MacroService* service,
                    MacroSettingsService* settings);

    void begin() override;

private:
    // Borrowed runtime macro engine owned by ServiceRegistry.
    MacroService* _service;
    // Borrowed RTC-backed settings service owned by ServiceRegistry. This API
    // keeps only transport and validation such as boot-script existence checks.
    MacroSettingsService* _settings;
    std::unique_ptr<HttpEndpoint<RTC::MacroData>> _configEndpoint;

    esp_err_t handleList(PsychicRequest* request);
    esp_err_t handleUpload(PsychicRequest* request);
    esp_err_t handleDelete(PsychicRequest* request);
    esp_err_t handleRun(PsychicRequest* request);
    esp_err_t handleStop(PsychicRequest* request);
    esp_err_t handleStatus(PsychicRequest* request);
    esp_err_t handleGetContent(PsychicRequest* request);
    StateHandlerResult validateSettingsUpdate(PsychicRequest* request, JsonObject& jsonObject);
};

} // namespace MACROS
