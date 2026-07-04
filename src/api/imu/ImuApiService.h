#pragma once

#include <PsychicHttpServer.h>
#include <core/HttpEndpoint.h>
#include <memory>

#include "../BaseApiService.h"
#include "../../system/rtc/RtcConfig.h"

namespace IMU {
class ImuRuntimeService;
}

namespace API {

class ImuApiService : public BaseApiService {
public:
    ImuApiService(PsychicHttpServer* server,
                  SecurityManager* securityManager,
                  POWER::PowerManager* powerManager,
                  IMU::ImuRuntimeService* runtime);

    void begin() override;

private:
    esp_err_t handleStatus(PsychicRequest* request);
    esp_err_t handleCalibrateOrientation(PsychicRequest* request);
    esp_err_t handleResetOrientationBaseline(PsychicRequest* request);

    IMU::ImuRuntimeService* _runtime = nullptr;
    std::unique_ptr<HttpEndpoint<RTC::ImuData>> _settingsEndpoint;
};

}  // namespace API
