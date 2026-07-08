#include "MatrixApiService.h"

#include <PsychicJson.h>
#include <utils/ResponseUtils.h>

#include "../../system/logging/Logging.h"
#include "../../wifisensing/csi/core/CsiService.h"
#include "MatrixService.h"

#undef LOG_TAG
#define LOG_TAG "MatrixApi"

namespace API {

namespace {
constexpr const char* kMatrixSettingsPath = "/api/matrix/settings";
constexpr const char* kMatrixDataVizStatusPath = "/api/matrix/data-visualization/status";
constexpr const char* kMatrixCsiCalibratePath = "/api/matrix/data-visualization/csi/calibrate";
}

MatrixApiService::MatrixApiService(
    PsychicHttpServer* server,
    SecurityManager* securityManager,
    POWER::PowerManager* powerManager,
    MATRIX::MatrixSettingsService* matrixSettings,
    MatrixService* matrixService,
    WIFISENSING::CSI::CsiService* csiService)
    : BaseApiService(server, securityManager, powerManager, "api/matrix"),
      _matrixSettings(matrixSettings),
      _matrixService(matrixService),
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

    _server->on(kMatrixDataVizStatusPath, HTTP_GET, wrapAuth([this](PsychicRequest* request) {
        return handleDataVisualizationStatus(request);
    }));

    _server->on(kMatrixCsiCalibratePath, HTTP_POST, wrapAdmin([this](PsychicRequest* request) {
        return handleCsiCalibration(request);
    }));
}

esp_err_t MatrixApiService::handleDataVisualizationStatus(PsychicRequest* request) {
    return Response::success(request, [this](JsonVariant& root) {
        JsonObject obj = root.to<JsonObject>();
        MATRIX::MatrixDataVisualizationStatusSnapshot snapshot{};

        if (_matrixService) {
            snapshot = _matrixService->getDataVisualizationStatusSnapshot();
        } else {
            snapshot.input.reason = static_cast<uint8_t>(MATRIX::MatrixDataVisualizationReason::NoService);
        }

        const uint32_t now = millis();
        const uint32_t ageMs = snapshot.input.timestampMs == 0
            ? 0
            : now - snapshot.input.timestampMs;
        const bool active = snapshot.active && snapshot.config.enabled;
        const uint8_t reason = !active
            ? static_cast<uint8_t>(MATRIX::MatrixDataVisualizationReason::Disabled)
            : snapshot.input.reason;

        obj["active"] = active;
        obj["source"] = snapshot.config.source;
        obj["metric"] = snapshot.config.metric;
        obj["mode"] = snapshot.config.mode;
        obj["valid"] = snapshot.input.valid;
        obj["stale"] = snapshot.input.stale;
        obj["reason"] = MATRIX::matrixDataVisualizationReasonToString(reason);
        obj["value"] = snapshot.input.value;
        obj["secondary"] = snapshot.input.secondary;
        obj["last_update_ms"] = snapshot.input.timestampMs;
        obj["age_ms"] = ageMs;
        obj["bin_count"] = snapshot.input.binCount;

        JsonObject csi = obj["csi"].to<JsonObject>();
        if (_csiService) {
            const auto csiSnapshot = _csiService->getMetricsSnapshot();
            csi["available"] = true;
            csi["matrix_visualization_consumer_active"] =
                csiSnapshot.matrixVisualizationConsumerActive;
            csi["packets_per_sec"] = csiSnapshot.packetsPerSec;
            csi["last_packet_ms"] = csiSnapshot.lastPacketMs;
        } else {
            csi["available"] = false;
            csi["matrix_visualization_consumer_active"] = false;
            csi["packets_per_sec"] = 0;
            csi["last_packet_ms"] = 0;
        }
    });
}

esp_err_t MatrixApiService::handleCsiCalibration(PsychicRequest* request) {
    if (!_csiService) {
        return Response::success(request, [](JsonVariant& root) {
            root["ok"] = false;
            root["error"] = "csi_service_unavailable";
        });
    }

    _csiService->resetVisualizationState();
    return Response::success(request, [](JsonVariant& root) {
        root["ok"] = true;
        root["status"] = "visualization_reset_requested";
    });
}

}  // namespace API
