#pragma once

#include <PsychicHttpServer.h>
#include <security/SecurityManager.h>
#include "../BaseApiService.h"
#include "../../system/rtc/RtcConfig.h" // dla RTC::LoggingData
#include <functional> // dla std::function

namespace API {

using GetLoggingConfigFn = RTC::LoggingData (*)();
using SetLoggingLevelFn = bool (*)(uint8_t);

class ConfigApiService : public BaseApiService {
public:
    ConfigApiService(PsychicHttpServer* server, SecurityManager* securityManager, POWER::PowerManager* powerManager, 
                     GetLoggingConfigFn getLoggingConfig,
                     SetLoggingLevelFn setLoggingLevel);
    void begin() override;

private:
    GetLoggingConfigFn _getLoggingConfig;
    SetLoggingLevelFn _setLoggingLevel;
    
    esp_err_t handleGetConfig(PsychicRequest *request);
    esp_err_t handleSaveConfig(PsychicRequest *request);
};

}  // namespace API
