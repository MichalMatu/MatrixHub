#include "ConfigApiService.h"

#include "../../config/App.h"
#include "../../config/json/SystemConfigJson.h"
#include "../../system/power/PowerManager.h"
#include "../../system/logging/Logging.h"
#include <ArduinoJson.h>
#include "../../system/memory/PsramAllocator.h"
#include <PsychicHttpServer.h>
#include <PsychicJson.h>
#include <PsychicStreamResponse.h>
#include <utils/ResponseUtils.h>
#include "../../system/utils/json/JsonResponseWriter.h"
#include <cstring>

namespace API {

ConfigApiService::ConfigApiService(PsychicHttpServer* server, SecurityManager* securityManager, POWER::PowerManager* powerManager, 
                                   GetLoggingConfigFn getLoggingConfig,
                                   SetLoggingLevelFn setLoggingLevel)
    : BaseApiService(server, securityManager, powerManager, "api/config")
    , _getLoggingConfig(getLoggingConfig)
    , _setLoggingLevel(setLoggingLevel) {}

void ConfigApiService::begin() {
    _server->on(
        "/api/config",
        HTTP_GET,
        wrapAdmin([this](PsychicRequest *request) -> esp_err_t {
            return this->handleGetConfig(request);
        })
    );

    _server->on(
        "/api/config",
        HTTP_POST,
        wrapAdmin([this](PsychicRequest *request) -> esp_err_t {
            return this->handleSaveConfig(request);
        })
    );
}

esp_err_t ConfigApiService::handleGetConfig(PsychicRequest *request) {
    Utils::JsonResponseWriter w(request->request());
    if (!w.beginResponse()) return ESP_FAIL;

    const auto cfg = _getLoggingConfig();
    
    w.raw("{"); 
    w.key(CONFIG::Keys::kLogging); w.raw("{");
    w.key(CONFIG::Keys::kLevel); w.string(LOG::Logging::levelToString(cfg.level));
    w.raw("}}");
    w.finish();

    return ESP_OK;
}

esp_err_t ConfigApiService::handleSaveConfig(PsychicRequest *request) {
    return this->parseJsonBody(
        request,
        LIMITS::API::JSON_DOC::LOGGING_CONFIG,
        [this, request](JsonDocument& doc) -> esp_err_t {
        if (doc[CONFIG::Keys::kLogging].is<JsonObject>()) {
            JsonObject logObj = doc[CONFIG::Keys::kLogging].as<JsonObject>();
            auto current = _getLoggingConfig();
            auto next = current;  // copy
            CONFIG::JSON::deserializeLogging(logObj, next);
            if (memcmp(&current, &next, sizeof(RTC::LoggingData)) != 0) {
                // Persist to RTC + JSON config via injected dependency
                if (!_setLoggingLevel(next.level)) {
                    return Response::error(request, 500, "config/save_failed");
                }
                // Apply immediately
                LOG::Logging::setLevel(next.level);
            }
        }

        // Response using PSRAM buffer
        Utils::JsonResponseWriter w(request->request());
        if (!w.beginResponse()) return ESP_FAIL;
        
        w.raw("{");
        w.key("ok"); w.value(true);
        w.raw(","); 
        w.key(CONFIG::Keys::kLogging); w.raw("{");
        w.key(CONFIG::Keys::kLevel); w.string(LOG::Logging::levelToString(_getLoggingConfig().level));
        w.raw("}}");
        w.finish();
        
        return ESP_OK;
    },
    LIMITS::API::JSON_DOC::LOGGING_CONFIG);
}

}  // namespace API
