#include "AirMouseApiService.h"



#include "../../airmouse/AirMouseService.h"
#include "../../config/System.h"
#include "../../config/Network.h"
#include "../../macros/MacroService.h"
#include <utils/ResponseUtils.h>
#include <PsychicJson.h>
#include "../../system/utils/json/JsonResponseWriter.h"
#include "../../system/memory/PsramAllocator.h"
#include "../../system/logging/Logging.h"

#include <Arduino.h>
#include <services/RestartService.h>

namespace API {

#undef LOG_TAG
#define LOG_TAG "AirMouseApi"

namespace {
    constexpr const char* kAirMouseConfigPath = "/api/airmouse/config";
    constexpr const char* kAirMouseUnavailableError = "airmouse/unavailable";

    bool readAirMouseConfigSnapshot(RTC::AirMouseData& cfg) {
        bool loaded = false;
        RTC::withConfig([&](const RTC::ConfigStore& store) {
            cfg = store.airMouse;
            loaded = true;
        });
        return loaded;
    }
}

AirMouseApiService::AirMouseApiService(PsychicHttpServer* server, SecurityManager* securityManager, POWER::PowerManager* powerManager, 
                                       AIRMOUSE::AirMouseService* service,
                                       MACROS::MacroService* macroService,
                                       AIRMOUSE::AirMouseSettingsService* settings)
    : BaseApiService(server, securityManager, powerManager, "api/airmouse"),
      _service(service),
      _macroService(macroService),
      _settings(settings) {
    if (_settings) {
        _configEndpoint = std::make_unique<HttpEndpoint<RTC::AirMouseData>>(
            AIRMOUSE::AirMouseSettingsService::readState,
            AIRMOUSE::AirMouseSettingsService::updateState,
            _settings,
            _server,
            kAirMouseConfigPath,
            _securityManager,
            AuthenticationPredicates::IS_ADMIN,
            nullptr,
            [this]() {
                if (_powerManager) {
                    _powerManager->notifyActivity(_activityTag);
                }
            });
    }
}

void AirMouseApiService::begin() {
    // GET /api/airmouse/status - Get current status and settings
    _server->on("/api/airmouse/status", HTTP_GET,
        wrapAuth([this](PsychicRequest* request) -> esp_err_t {
            RTC::AirMouseData cfg{};
            if (!readAirMouseConfigSnapshot(cfg)) {
                return Response::error(request, 503, "rtc/lock_timeout");
            }
            
            AIRMOUSE::AirMouseDebugSnapshot debugSnapshot{};
            bool running = false;
            bool calibrating = false;
            if (_service) {
                _service->getDebugSnapshot(debugSnapshot);
                running = _service->isRunning();
                calibrating = _service->isCalibrating();
            }
            
            // Use JsonResponseWriter for clean, buffer-safe JSON generation with Keys::
            Utils::JsonResponseWriter w(request->request());
            if (!w.beginResponse()) return ESP_FAIL;
            
            w.raw("{");
            w.key(CONFIG::Keys::kMovementEnabled); w.value(cfg.movementEnabled);
            w.raw(","); w.key(CONFIG::Keys::kClickEnabled); w.value(cfg.clickEnabled);
            w.raw(","); w.key(CONFIG::Keys::kRunning); w.value(running);
            w.raw(","); w.key(CONFIG::Keys::kCalibrating); w.value(calibrating);
            // Status intentionally omits the removed transport selector. The
            // only live ownership knobs are the USB-backed runtime toggles.
            w.raw(","); w.key(CONFIG::Keys::kSensitivityX); w.value(cfg.sensitivityX);
            w.raw(","); w.key(CONFIG::Keys::kSensitivityY); w.value(cfg.sensitivityY);
            w.raw(","); w.key(CONFIG::Keys::kDeadzone); w.value(cfg.deadzone);
            w.raw(","); w.key(CONFIG::Keys::kAccelerationEnabled); w.value(cfg.accelerationEnabled);
            w.raw(","); w.key(CONFIG::Keys::kAccelerationFactor); w.value(cfg.accelerationFactor);
            w.raw(","); w.key(CONFIG::Keys::kTapThresholdG); w.value(cfg.tapThresholdG);
            w.raw(","); w.key(CONFIG::Keys::kClickDebounceMs); w.value(cfg.clickDebounceMs);
            w.raw(","); w.key(CONFIG::Keys::kDoubleClickWindowMs); w.value(cfg.doubleClickWindowMs);
            w.raw(","); w.key(CONFIG::Keys::kClickSource); w.value((int)cfg.clickSource);
            w.raw(","); w.key(CONFIG::Keys::kSingleClickAction); w.value((int)cfg.singleClickAction);
            w.raw(","); w.key(CONFIG::Keys::kDoubleClickAction); w.value((int)cfg.doubleClickAction);
            w.raw(","); w.key(CONFIG::Keys::kTripleClickAction); w.value((int)cfg.tripleClickAction);
            w.raw(","); w.key(CONFIG::Keys::kSingleClickScript); w.string(cfg.singleClickScript);
            w.raw(","); w.key(CONFIG::Keys::kDoubleClickScript); w.string(cfg.doubleClickScript);
            w.raw(","); w.key(CONFIG::Keys::kTripleClickScript); w.string(cfg.tripleClickScript);
            w.raw(","); w.key(CONFIG::Keys::kEuroMinCutoff); w.value(cfg.euroMinCutoff);
            w.raw(","); w.key(CONFIG::Keys::kEuroBeta); w.value(cfg.euroBeta);
            w.raw(","); w.key(CONFIG::Keys::kEuroDCutoff); w.value(cfg.euroDCutoff);
            
            w.raw(","); w.key(CONFIG::Keys::kJiggler); 
            w.raw("{");
            w.key(CONFIG::Keys::kMode); w.value((int)cfg.jiggler.mode);
            w.raw(","); w.key(CONFIG::Keys::kInterval); w.value(cfg.jiggler.interval);
            w.raw(","); w.key(CONFIG::Keys::kMoveDistance); w.value(cfg.jiggler.moveDistance);
            w.raw(","); w.key(CONFIG::Keys::kRandomInterval); w.value(cfg.jiggler.randomInterval);
            w.raw("}");

            w.raw(","); w.key(CONFIG::Keys::kGyroOffsetX); w.value(debugSnapshot.offsetX);
            w.raw(","); w.key(CONFIG::Keys::kGyroOffsetY); w.value(debugSnapshot.offsetY);
            w.raw(","); w.key(CONFIG::Keys::kGyroOffsetZ); w.value(debugSnapshot.offsetZ);
            w.raw(","); w.key(CONFIG::Keys::kLastDeltaG); w.value(debugSnapshot.deltaG);

            w.raw(","); w.key(CONFIG::Keys::kImu);
            w.raw("{");
            w.key(CONFIG::Keys::kGx); w.value(debugSnapshot.gx);
            w.raw(","); w.key(CONFIG::Keys::kGy); w.value(debugSnapshot.gy);
            w.raw(","); w.key(CONFIG::Keys::kGz); w.value(debugSnapshot.gz);
            w.raw(","); w.key(CONFIG::Keys::kAx); w.value(debugSnapshot.ax);
            w.raw(","); w.key(CONFIG::Keys::kAy); w.value(debugSnapshot.ay);
            w.raw(","); w.key(CONFIG::Keys::kAz); w.value(debugSnapshot.az);
            w.raw("}");
            
            w.raw("}");
            w.finish();
            return ESP_OK;
        })
    );

    if (_configEndpoint) {
        _configEndpoint->begin();
    }

    // POST /api/airmouse/calibrate - Trigger calibration (with rate-limiting)
    _server->on("/api/airmouse/calibrate", HTTP_POST,
        wrapAdmin([this](PsychicRequest* request) -> esp_err_t {
            static uint32_t lastCalibrationTime = 0;

            if (!_service) {
                return Response::error(request, 503, kAirMouseUnavailableError);
            }
            
            if (!_service->isRunning()) {
                return Response::error(request, 400, "airmouse/not_running");
            }
            
            // Rate limiting - prevent calibration spam
            uint32_t now = millis();
            if (now - lastCalibrationTime < LIMITS::AIR_MOUSE::CALIBRATION_COOLDOWN_MS) {
                return Response::error(request, 429, "airmouse/calibration_cooldown");
            }
            lastCalibrationTime = now;
            
            _service->calibrate();
            return Response::success(request, [](JsonVariant& root) {
                root["status"] = "calibrating";
            });
        })
    );
}

void AirMouseApiService::cleanupClient(int fd) {
    (void)fd;
}

}  // namespace API
