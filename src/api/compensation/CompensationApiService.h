#pragma once

#include <PsychicHttpServer.h>
#include <core/HttpEndpoint.h>
#include <security/SecurityManager.h>
#include <memory>
#include "../BaseApiService.h"
#include "../../system/rtc/RtcConfig.h"

namespace COMPENSATION {
class CompensationSettingsService;
}

namespace API {

class CompensationApiService : public BaseApiService {
public:
    CompensationApiService(PsychicHttpServer* server,
                           SecurityManager* securityManager,
                           POWER::PowerManager* powerManager,
                           COMPENSATION::CompensationSettingsService* settings);
    void begin() override;

private:
    // Borrowed from ServiceRegistry. This API no longer owns persistent
    // compensation config; it only exposes the HTTP contract around the shared
    // settings object that already knows how to persist and apply changes.
    COMPENSATION::CompensationSettingsService* _settings;
    std::unique_ptr<HttpEndpoint<RTC::CompensationData>> _configEndpoint;
};

}  // namespace API
