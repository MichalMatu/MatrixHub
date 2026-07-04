#include "PowerApiService.h"

#include <PsychicJson.h>
#include <core/HttpEndpoint.h>
#include <utils/ResponseUtils.h>

#include "../../system/power/PowerManager.h"
#include "../../system/power/PowerSettingsService.h"
#include "../../system/power/PowerWakeController.h"
#include "../../system/thermal/ThermalMonitor.h"
#include "../../system/boot/BootTracker.h"
#include "../../system/logging/Logging.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../config/System.h"
#include "../../config/Hardware.h"
#include "../../system/memory/PsramAllocator.h"
#include "../../system/utils/json/JsonResponseWriter.h"
#include <ArduinoJson.h>

namespace API {

#undef LOG_TAG
#define LOG_TAG "PowerApi"

namespace {
constexpr const char* kPowerConfigPath = "/rest/power/config";

const char* thermalStateToString(SYSTEM::ThermalState state) {
    switch (state) {
        case SYSTEM::ThermalState::NORMAL:
            return "normal";
        case SYSTEM::ThermalState::SOFT_THROTTLE:
            return "soft_throttle";
        case SYSTEM::ThermalState::HARD_THROTTLE:
            return "hard_throttle";
        case SYSTEM::ThermalState::CRITICAL:
            return "critical";
    }
    return "unknown";
}
}

PowerApiService::PowerApiService(PsychicHttpServer* server,
                                 SecurityManager* securityManager,
                                 POWER::PowerManager* powerManager,
                                 POWER::PowerSettingsService* settings)
    : BaseApiService(server, securityManager, powerManager, "rest/power"),
      _settings(settings) {
    // Registry ownership means the API no longer owns persistent power config.
    // If settings are unavailable, keep status/control routes alive and simply
    // skip the config endpoint instead of constructing a second local owner.
    if (_settings) {
        _configEndpoint = std::make_unique<HttpEndpoint<RTC::PowerData>>(
            POWER::PowerSettingsService::readState,
            POWER::PowerSettingsService::updateState,
            _settings,
            _server,
            kPowerConfigPath,
            _securityManager,
            AuthenticationPredicates::IS_ADMIN,
            AuthenticationPredicates::IS_AUTHENTICATED,
            [](PsychicRequest* request, JsonObject& root) {
                (void)request;
                return POWER::PowerSettingsService::validateStateUpdate(root);
            },
            [this]() {
                if (_powerManager) {
                    _powerManager->notifyActivity(_activityTag);
                }
            });
    }
}

void PowerApiService::begin() {
    _server->on("/rest/power/status", HTTP_GET,
        _securityManager->wrapRequest([this](PsychicRequest *request) -> esp_err_t {
            if (!_powerManager) return request->reply(500);
            auto reason = _powerManager->wakeReason();
            auto powerConfig = _powerManager->powerConfig();
            auto& thermal = SYSTEM::ThermalMonitor::instance();
            const auto thermalState = thermal.getState();

            const char* reasonStr = "unknown";
            switch (reason) {
                case POWER::WakeReason::Timer: reasonStr = "timer"; break;
                case POWER::WakeReason::Touch: reasonStr = "touch"; break;
                case POWER::WakeReason::Button: reasonStr = "button"; break;
                case POWER::WakeReason::Other: reasonStr = "other"; break;
                default: break;
            }

            Utils::JsonResponseWriter w(request->request());
            if (!w.beginResponse()) return ESP_FAIL;

            w.raw("{");
            w.key(CONFIG::Keys::kWakeReason); w.string(reasonStr); w.raw(",");
            w.key(CONFIG::Keys::kWakeCauseRaw); w.value(static_cast<uint32_t>(_powerManager->getWakeupCauseRaw())); w.raw(",");
            
            char hexMask[32];
            snprintf(hexMask, sizeof(hexMask), "0x%016llX", static_cast<unsigned long long>(_powerManager->getGpioWakeupMask()));
            w.key(CONFIG::Keys::kWakeGpioMask); w.string(hexMask); w.raw(",");
            
            snprintf(hexMask, sizeof(hexMask), "0x%016llX", static_cast<unsigned long long>(_powerManager->getExt1WakeupMask()));
            w.key(CONFIG::Keys::kWakeExt1Mask); w.string(hexMask); w.raw(",");
            
            w.key(CONFIG::Keys::kSleepRequested); w.value(_powerManager->isSleepRequested()); w.raw(",");
            w.key(CONFIG::Keys::kSleepEtaMs); w.value(_powerManager->sleepEtaMs()); w.raw(",");
            w.key(CONFIG::Keys::kSleepEnabled); w.value(powerConfig.sleepEnabled); w.raw(",");
            w.key(CONFIG::Keys::kInactivityTimeoutMs); w.value(powerConfig.inactivityTimeoutMs); w.raw(",");
            w.key(CONFIG::Keys::kGraceAfterBootMs); w.value(powerConfig.graceAfterBootMs); w.raw(",");
            w.key(CONFIG::Keys::kWakeTimerEnabled); w.value(powerConfig.wakeTimerEnabled); w.raw(",");
            w.key(CONFIG::Keys::kWakeButtonEnabled); w.value(powerConfig.wakeButtonEnabled); w.raw(",");
            w.key(CONFIG::Keys::kWakeTouchEnabled); w.value(powerConfig.wakeTouchEnabled); w.raw(",");
            w.key(CONFIG::Keys::kWakeIntervalMs); w.value(powerConfig.wakeIntervalMs); w.raw(",");
            w.key(CONFIG::Keys::kTimerWakeAwakeMs); w.value(powerConfig.timerWakeAwakeMs); w.raw(",");
            w.key(CONFIG::Keys::kButtonWakeAwakeMs); w.value(powerConfig.buttonWakeAwakeMs); w.raw(",");
            w.key(CONFIG::Keys::kWakeTouchGpio); w.value(powerConfig.wakeTouchGpio); w.raw(",");
            w.key(CONFIG::Keys::kWakeTouchThreshold); w.value(powerConfig.wakeTouchThreshold); w.raw(",");
            w.key(CONFIG::Keys::kWakeAwakeWindowMs); w.value(_powerManager->wakeAwakeWindowMs()); w.raw(",");
            w.key(CONFIG::Keys::kWakeAwakeEtaMs); w.value(_powerManager->wakeAwakeEtaMs()); w.raw(",");
            w.key(CONFIG::Keys::kLastActivityMs); w.value(_powerManager->lastActivityMs()); w.raw(",");
            w.key(CONFIG::Keys::kThermalState); w.string(thermalStateToString(thermalState)); w.raw(",");
            w.key(CONFIG::Keys::kThermalTempC); w.value(thermal.getTemperature(), 1); w.raw(",");
            w.key(CONFIG::Keys::kThermalCpuMhz); w.value(thermal.getCpuFrequency()); w.raw(",");
            w.key(CONFIG::Keys::kThermalThrottled); w.value(thermal.isThrottled()); w.raw(",");
            w.key(CONFIG::Keys::kThermalSoftC); w.value(THERMAL::TEMP_SOFT_THROTTLE, 1); w.raw(",");
            w.key(CONFIG::Keys::kThermalHardC); w.value(THERMAL::TEMP_HARD_THROTTLE, 1); w.raw(",");
            w.key(CONFIG::Keys::kThermalCriticalC); w.value(THERMAL::TEMP_CRITICAL, 1); w.raw(",");
            w.key(CONFIG::Keys::kUptimeMs); w.value((uint32_t)millis());
            w.raw("}");
            w.finish();

            return ESP_OK;
        }, AuthenticationPredicates::IS_AUTHENTICATED)
    );

    _server->on("/rest/power/hygieneSleep", HTTP_POST,
        wrapAdmin([this](PsychicRequest *request) -> esp_err_t {
            request->reply(200);
            
            // Give time for the HTTP response to be flushed
            vTaskDelay(pdMS_TO_TICKS(500));

            // Enter hygiene sleep (short 100ms deep sleep) to reset DRAM/TLS without full reboot
            if (_powerManager) {
                _powerManager->setWakeInterval(100);
                RTC::markMaintenanceSleepPending(millis());
                LOGI("[Hygiene] Flag set: active=%d, count=%u",
                     RTC::runtimeStats.hygieneSleepActive,
                     RTC::runtimeStats.hygieneSleepCount);
                _powerManager->requestSleep("manual-hygiene");
            }
            return ESP_OK;
        })
    );

    // Override framework's default sleep endpoint to ensure clean shutdown via PowerManager
    // Unregister first to avoid ESP_ERR_HTTPD_HANDLER_EXISTS (framework registers this by default)
    httpd_unregister_uri_handler(_server->server, "/rest/sleep", HTTP_POST);
    _server->on("/rest/sleep", HTTP_POST,
        wrapAdmin([this](PsychicRequest *request) -> esp_err_t {
            request->reply(200);
            // Use PowerManager to ensure stats are saved and shutdown sequence runs
            if (_powerManager) _powerManager->requestSleep("web-framework-override");
            return ESP_OK;
        })
    );

    if (_configEndpoint) {
        _configEndpoint->begin();
    }
}

}  // namespace API
