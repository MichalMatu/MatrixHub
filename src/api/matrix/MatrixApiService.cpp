#include "MatrixApiService.h"

#include <PsychicJson.h>
#include <utils/ResponseUtils.h>

#include "../../system/logging/Logging.h"
#include "../../wifisensing/csi/core/CsiService.h"

#undef LOG_TAG
#define LOG_TAG "MatrixApi"

namespace API {

namespace {
constexpr const char* kMatrixSettingsPath = "/api/matrix/settings";
constexpr const char* kMatrixCsiCalibratePath = "/api/matrix/data-visualization/csi/calibrate";
}

MatrixApiService::MatrixApiService(
    PsychicHttpServer* server,
    SecurityManager* securityManager,
    POWER::PowerManager* powerManager,
    MATRIX::MatrixSettingsService* matrixSettings,
    WIFISENSING::CSI::CsiService* csiService)
    : BaseApiService(server, securityManager, powerManager, "api/matrix"),
      _matrixSettings(matrixSettings),
      _csiService(csiService) {
    if (_matrixSettings) {
        _configEndpoint = std::make_unique<HttpEndpoint<MATRIX::MatrixSettingsState>>(
            MATRIX::MatrixSettingsService::readState,
            MATRIX::MatrixSettingsService::updateState,
            _matrixSettings,
            _server,
            kMatrixSettingsPath,
            _securityManager,
            AuthenticationPredicates::IS_ADMIN,
            AuthenticationPredicates::IS_AUTHENTICATED,
            nullptr,
            [this]() {
                if (_powerManager) {
                    _powerManager->notifyActivity(_activityTag);
                }
            });
    }
}

void MatrixApiService::begin() {
    if (!_configEndpoint || !_matrixSettings) {
        LOGE("Matrix settings endpoint was not initialized");
        return;
    }

    _configEndpoint->begin();

    _server->on(kMatrixCsiCalibratePath, HTTP_POST, wrapAdmin([this](PsychicRequest* request) {
        return handleCsiCalibration(request);
    }));
}

esp_err_t MatrixApiService::handleCsiCalibration(PsychicRequest* request) {
    (void)_csiService;
    return Response::success(request, [](JsonVariant& root) {
        root["ok"] = true;
        root["status"] = "baseline_not_required";
    });
}

}  // namespace API
