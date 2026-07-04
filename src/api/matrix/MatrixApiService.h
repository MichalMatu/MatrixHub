#pragma once

#include <memory>

#include <core/HttpEndpoint.h>

#include "../BaseApiService.h"
#include "../../matrix/MatrixSettingsService.h"

namespace WIFISENSING::CSI { class CsiService; }

namespace API {

class MatrixApiService : public BaseApiService {
public:
    MatrixApiService(PsychicHttpServer* server,
                     SecurityManager* securityManager,
                     POWER::PowerManager* powerManager,
                     MATRIX::MatrixSettingsService* matrixSettings,
                     WIFISENSING::CSI::CsiService* csiService = nullptr);

    void begin() override;

private:
    esp_err_t handleCsiCalibration(PsychicRequest* request);

    MATRIX::MatrixSettingsService* _matrixSettings;
    WIFISENSING::CSI::CsiService* _csiService;
    std::unique_ptr<HttpEndpoint<MATRIX::MatrixSettingsState>> _configEndpoint;
};

} // namespace API
