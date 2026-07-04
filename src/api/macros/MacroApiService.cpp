#include "MacroApiService.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "../../system/memory/PsramAllocator.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../config/App.h"
#include "../../config/json/MacroConfigJson.h"
#include "../../macros/MacroSettingsService.h"
#include "../../system/utils/json/JsonResponseWriter.h"
#include <utils/ResponseUtils.h>
#include <cctype>

namespace MACROS {

    namespace {
        constexpr const char* kMacroSettingsPath = "/api/macros/settings";

        bool isValidScriptFilename(const String& filename) {
            if (filename.isEmpty() || filename.length() > LIMITS::MAX_FILENAME_LENGTH) {
                return false;
            }
            if (filename.indexOf('/') >= 0 || filename.indexOf('\\') >= 0 || filename.indexOf("..") >= 0) {
                return false;
            }
            if (filename.endsWith(".tmp")) {
                return false;
            }
            for (const char c : filename) {
                const unsigned char uc = static_cast<unsigned char>(c);
                if (!std::isalnum(uc) && c != '_' && c != '-' && c != '.') {
                    return false;
                }
            }
            return true;
        }

        bool requireScriptFilename(PsychicRequest* request, const String& filename) {
            if (filename.isEmpty()) {
                Response::error(request, 400, "input/missing_filename");
                return false;
            }
            if (!isValidScriptFilename(filename)) {
                Response::error(request, 400, "input/invalid_filename");
                return false;
            }
            return true;
        }
    }

    MacroApiService::MacroApiService(PsychicHttpServer* server,
                                     SecurityManager* securityManager,
                                     POWER::PowerManager* powerManager,
                                     MacroService* service,
                                     MacroSettingsService* settings)
         : API::BaseApiService(server, securityManager, powerManager, "api/macros"),
           _service(service),
           _settings(settings) {
        if (_settings) {
            // Persistent macro config is registry-owned now; this facade keeps
            // only transport and API-specific validation such as boot script
            // existence checks.
            _configEndpoint = std::make_unique<HttpEndpoint<RTC::MacroData>>(
                MacroSettingsService::readState,
                MacroSettingsService::updateState,
                _settings,
                _server,
                kMacroSettingsPath,
                _securityManager,
                AuthenticationPredicates::IS_ADMIN,
                [this](PsychicRequest* request, JsonObject& jsonObject) {
                    return validateSettingsUpdate(request, jsonObject);
                },
                [this]() {
                    if (_powerManager) {
                        _powerManager->notifyActivity(_activityTag);
                    }
                });
        }
    }

    void MacroApiService::begin() {
        // List scripts
        _server->on("/api/macros", HTTP_GET, 
            wrapAdmin([this](PsychicRequest* r) { return handleList(r); }));

        // Upload script
        _server->on("/api/macros", HTTP_POST,
            wrapAdmin([this](PsychicRequest* r) { return handleUpload(r); }));

        // Run script
        _server->on("/api/macros/run", HTTP_POST,
            wrapAdmin([this](PsychicRequest* r) { return handleRun(r); }));

        // Stop script
        _server->on("/api/macros/stop", HTTP_POST,
            wrapAdmin([this](PsychicRequest* r) { return handleStop(r); }));

        // Status
        _server->on("/api/macros/status", HTTP_GET,
            wrapAdmin([this](PsychicRequest* r) { return handleStatus(r); }));

        // Get Content
        _server->on("/api/macros/content", HTTP_GET,
            wrapAdmin([this](PsychicRequest* r) { return handleGetContent(r); }));

        if (_configEndpoint) {
            _configEndpoint->begin();
        }

        // Delete script
        _server->on("/api/macros/delete", HTTP_POST, 
            wrapAdmin([this](PsychicRequest* r) { return handleDelete(r); }));
    }

    StateHandlerResult MacroApiService::validateSettingsUpdate(PsychicRequest* request, JsonObject& jsonObject) {
        (void)request;

        RTC::MacroData nextState{};
        bool loaded = false;
        if (_settings) {
            const StateHandlerResult readResult = _settings->read([&](RTC::MacroData& state) {
                nextState = state;
                loaded = true;
            });
            if (!readResult.ok) {
                return readResult.errorCode
                    ? readResult
                    : StateHandlerResult::failure("internal/update_failed", readResult.httpStatus);
            }
        }

        if (!loaded) {
            return StateHandlerResult::failure("internal/update_failed");
        }

        const JsonObjectConst input = jsonObject;
        CONFIG::JSON::deserializeMacros(input, nextState);

        if (nextState.bootScript[0] == '\0') {
            return StateHandlerResult::success();
        }

        if (!isValidScriptFilename(String(nextState.bootScript))) {
            return StateHandlerResult::failure("input/invalid_boot_script", 400);
        }

        char path[128];
        snprintf(path, sizeof(path), "/scripts/%s", nextState.bootScript);
        if (!LittleFS.exists(path)) {
            return StateHandlerResult::failure("input/boot_script_not_found", 400);
        }

        return StateHandlerResult::success();
    }

    esp_err_t MacroApiService::handleList(PsychicRequest* request) {
        auto scripts = _service->listScripts();
        
        Utils::JsonResponseWriter w(request->request());
        w.beginResponse();
        w.raw("[");
        bool first = true;
        for (const auto& s : scripts) {
            if (!first) w.raw(",");
            first = false;
            w.raw("{");
            w.key(CONFIG::Keys::kName); w.string(s.c_str());
            w.raw("}");
        }
        w.raw("]");
        w.finish();
        return ESP_OK;
    }

    esp_err_t MacroApiService::handleUpload(PsychicRequest* request) {
        if (request->contentType().startsWith("application/json")) {
            // Safety: Check body size before parsing to prevent RAM exhaustion
            if (request->contentLength() > LIMITS::MAX_UPLOAD_PAYLOAD_BYTES) {
                return Response::payloadTooLarge(request);
            }
            return this->parseJsonBody(
                request,
                ::LIMITS::API::JSON_DOC::MACROS_UPLOAD,
                [this, request](JsonDocument& doc) -> esp_err_t {
                if (!doc[CONFIG::Keys::kFilename].is<const char*>()) {
                    return Response::error(request, 400, "input/missing_filename");
                }
                if (!doc[CONFIG::Keys::kContent].is<const char*>()) {
                    return Response::error(request, 400, "input/missing_content");
                }

                String filename = doc[CONFIG::Keys::kFilename].as<const char*>();
                String content = doc[CONFIG::Keys::kContent].as<const char*>();

                if (!isValidScriptFilename(filename)) {
                    LOGE("Upload Invalid Filename: '%s'", filename.c_str());
                    return Response::error(request, 400, "input/invalid_filename");
                }
                
                if (content.length() > LIMITS::MAX_SCRIPT_SIZE_BYTES) {
                    return Response::error(request, 400, "input/script_too_large");
                }
                
                if (_service->saveScript(filename.c_str(), content.c_str())) {
                    Utils::JsonResponseWriter w(request->request());
                    if (!w.beginResponse()) return ESP_FAIL;
                    w.raw("{");
                    w.key("ok"); w.value(true);
                    w.raw(","); w.key("status"); w.string("saved");
                    w.raw("}");
                    w.finish();
                    return ESP_OK;
                } else {
                    return Response::error(request, 500, "internal/save_failed");
                }
            },
            LIMITS::MAX_UPLOAD_PAYLOAD_BYTES);
        }
        return Response::error(request, 400, "input/unsupported_content_type");
    }

    esp_err_t MacroApiService::handleDelete(PsychicRequest* request) {
        String filename = "";
        if (request->hasParam("name")) {
            filename = request->getParam("name")->value();
        } else if (request->bodyLength() > 0) {
            esp_err_t err = this->parseJsonBody(
                request,
                ::LIMITS::API::JSON_DOC::MACROS_DEFAULT,
                [&filename](JsonDocument& doc) -> esp_err_t {
                    filename = doc[CONFIG::Keys::kName].as<String>();
                    return ESP_OK;
                },
                ::LIMITS::API::JSON_DOC::MACROS_DEFAULT);
            if (err != ESP_OK) return err;
        }

        if (!requireScriptFilename(request, filename)) return ESP_OK;

        if (!_service->scriptExists(filename.c_str())) {
            return Response::error(request, 404, "input/script_not_found");
        }

        if (_service->deleteScript(filename.c_str())) {
            Utils::JsonResponseWriter w(request->request());
            if (!w.beginResponse()) return ESP_FAIL;
            w.raw("{");
            w.key("ok"); w.value(true);
            w.raw(","); w.key("status"); w.string("deleted");
            w.raw("}");
            w.finish();
            return ESP_OK;
        } else {
            return Response::error(request, 500, "internal/delete_failed");
        }
    }

    esp_err_t MacroApiService::handleRun(PsychicRequest* request) {
        String filename = "";
        if (request->hasParam("name")) {
            filename = request->getParam("name")->value();
        } else {
            if (request->bodyLength() > 0) {
                esp_err_t err = this->parseJsonBody(
                    request,
                    ::LIMITS::API::JSON_DOC::MACROS_DEFAULT,
                    [&filename](JsonDocument& doc) -> esp_err_t {
                        filename = doc[CONFIG::Keys::kName].as<String>();
                        return ESP_OK;
                    },
                    ::LIMITS::API::JSON_DOC::MACROS_DEFAULT);
                if (err != ESP_OK) return err;
            }
        }

        if (!requireScriptFilename(request, filename)) return ESP_OK;

        if (!_service->scriptExists(filename.c_str())) {
            return Response::error(request, 404, "input/script_not_found");
        }

        if (_service->startScript(filename.c_str())) {
            Utils::JsonResponseWriter w(request->request());
            if (!w.beginResponse()) return ESP_FAIL;
            w.raw("{");
            w.key("ok"); w.value(true);
            w.raw(","); w.key("status"); w.string("started");
            w.raw("}");
            w.finish();
            return ESP_OK;
        } else {
            return Response::error(request, 500, "service/busy_or_missing");
        }
    }

    esp_err_t MacroApiService::handleStop(PsychicRequest* request) {
        _service->stopScript();
        Utils::JsonResponseWriter w(request->request());
        if (!w.beginResponse()) return ESP_FAIL;
        w.raw("{");
        w.key("ok"); w.value(true);
        w.raw(","); w.key("status"); w.string("stopped");
        w.raw("}");
        w.finish();
        return ESP_OK;
    }

    esp_err_t MacroApiService::handleStatus(PsychicRequest* request) {
        MacroState state = _service->getStatus();
        
        const char* statusStr = "IDLE";
        switch(state.status) {
            case MacroStatus::RUNNING: statusStr = "RUNNING"; break;
            case MacroStatus::PAUSED: statusStr = "PAUSED"; break;
            case MacroStatus::ERROR: statusStr = "ERROR"; break;
            case MacroStatus::COMPLETED: statusStr = "COMPLETED"; break;
            default: break;
        }
        
        Utils::JsonResponseWriter w(request->request());
        w.beginResponse();
        w.raw("{");
        w.key(CONFIG::Keys::kCurrentScript); w.string(state.currentScript.c_str());
        w.raw(","); w.key(CONFIG::Keys::kStatus); w.string(statusStr);
        w.raw(","); w.key(CONFIG::Keys::kCurrentLine); w.value((unsigned)state.currentLine);
        w.raw(","); w.key(CONFIG::Keys::kUptimeMs); w.value((unsigned long)((state.status == MacroStatus::RUNNING) ? (millis() - state.startTime) : 0));
        w.raw(","); w.key(CONFIG::Keys::kLastError); w.string(state.lastError.c_str());
        w.raw("}");
        w.finish();
        return ESP_OK;
    }

    esp_err_t MacroApiService::handleGetContent(PsychicRequest* request) {
        if (!request->hasParam("name")) return Response::error(request, 400, "input/missing_filename");
        String filename = request->getParam("name")->value();

        if (!requireScriptFilename(request, filename)) return ESP_OK;
        if (!_service->scriptExists(filename.c_str())) {
            return Response::error(request, 404, "input/script_not_found");
        }
        
        PsramString content = _service->getScriptContent(filename.c_str());
        return request->reply(content.c_str());
    }

} // namespace MACROS
