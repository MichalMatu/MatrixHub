#pragma once

#include <memory>

#include <PsychicHttpServer.h>
#include <core/HttpEndpoint.h>
#include <security/SecurityManager.h>
#include "../BaseApiService.h"
#include "../../system/power/PowerSettingsService.h"

namespace API {

class PowerApiService : public BaseApiService {
public:
    PowerApiService(PsychicHttpServer* server,
                    SecurityManager* securityManager,
                    POWER::PowerManager* powerManager,
                    POWER::PowerSettingsService* settings);
    void begin() override;

private:
    POWER::PowerSettingsService* _settings = nullptr;
    std::unique_ptr<HttpEndpoint<RTC::PowerData>> _configEndpoint;
};

}  // namespace API
