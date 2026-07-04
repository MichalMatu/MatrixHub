#include "KeyboardApiService.h"

#include <ArduinoJson.h>

#include "../../config/App.h"
#include "../../config/json/ConfigKeys.h"
#include "../../keyboard/KeyboardService.h"
#include "../../system/memory/PsramAllocator.h"
#include <utils/ResponseUtils.h>

namespace API {

namespace {

constexpr const char* kKeyboardConfigPath = "/api/keyboard/config";
constexpr const char* kKeyboardDisabledError = "keyboard/disabled";

} // namespace

KeyboardApiService::KeyboardApiService(PsychicHttpServer* server,
                                       SecurityManager* securityManager,
                                       POWER::PowerManager* powerManager,
                                       KEYBOARD::KeyboardService* service,
                                       KEYBOARD::KeyboardSettingsService* settings)
    : BaseApiService(server, securityManager, powerManager, "api/keyboard"),
      _service(service),
      _settings(settings) {
    if (_settings) {
        _configEndpoint = std::make_unique<HttpEndpoint<RTC::KeyboardData>>(
            KEYBOARD::KeyboardSettingsService::readState,
            KEYBOARD::KeyboardSettingsService::updateState,
            _settings,
            _server,
            kKeyboardConfigPath,
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

void KeyboardApiService::begin() {
    if (_configEndpoint) {
        _configEndpoint->begin();
    }

    // POST /api/keyboard/type
    // Body: { "text": "hello world", "enter": true/false }
    _server->on("/api/keyboard/type", HTTP_POST,
        wrapAdmin([this](PsychicRequest* request) -> esp_err_t {
                if (const esp_err_t availabilityErr = ensureKeyboardAvailable(request); availabilityErr != ESP_OK) {
                    return availabilityErr;
                }

                SYSTEM::SpiRamJsonDocument doc(LIMITS::API::JSON_DOC::KEYBOARD);
                if (auto err = Response::parseJsonBody(
                        request, doc, LIMITS::API::JSON_DOC::KEYBOARD); err != ESP_OK) return err;
                JsonObject obj = doc.as<JsonObject>();
                if (obj.isNull()) {
                    return Response::invalidJson(request);
                }
                
                if (obj[CONFIG::Keys::kText].is<const char*>()) {
                    const char* text = obj[CONFIG::Keys::kText].as<const char*>();
                    bool enter = obj[CONFIG::Keys::kEnter] | false;
                    
                    if (enter) {
                        _service->typeLn(text);
                    } else {
                        _service->type(text);
                    }
                    return Response::success(request, [](JsonVariant& root) {
                        root["status"] = "ok";
                    });
                }
                
                return Response::error(request, 400, "input/missing_text");
            })
    );

    // POST /api/keyboard/press
    // Body: { "key": 176 } OR { "keys": [129, 97] }
    _server->on("/api/keyboard/press", HTTP_POST,
        wrapAdmin([this](PsychicRequest* request) -> esp_err_t {
                if (const esp_err_t availabilityErr = ensureKeyboardAvailable(request); availabilityErr != ESP_OK) {
                    return availabilityErr;
                }

                SYSTEM::SpiRamJsonDocument doc(LIMITS::API::JSON_DOC::KEYBOARD);
                if (auto err = Response::parseJsonBody(
                        request, doc, LIMITS::API::JSON_DOC::KEYBOARD); err != ESP_OK) return err;
                JsonObject obj = doc.as<JsonObject>();
                if (obj.isNull()) {
                    return Response::invalidJson(request);
                }
                
                // Handle combo
                if (obj[CONFIG::Keys::kKeys].is<JsonArray>()) {
                    JsonArray keysArr = obj[CONFIG::Keys::kKeys].as<JsonArray>();
                    size_t count = keysArr.size();
                    if (count > 0 && count <= 8) {
                        uint8_t keys[8];
                        size_t i = 0;
                        for (JsonVariant k : keysArr) {
                            keys[i++] = k.as<uint8_t>();
                        }
                        _service->pressCombo(keys, count);
                        return Response::success(request, [](JsonVariant& root) {
                            root["status"] = "ok";
                        });
                    }
                    return Response::error(request, 400, "input/invalid_keys");
                }

                // Handle single key
                if (obj[CONFIG::Keys::kKey].is<uint8_t>()) {
                    uint8_t key = obj[CONFIG::Keys::kKey].as<uint8_t>();
                    
                    // Consumer Control Mapping (Media Keys)
                    uint16_t consumerUsage = 0;
                    switch(key) {
                        case 230: consumerUsage = 0x00B5; break; // Scan Next
                        case 231: consumerUsage = 0x00B6; break; // Scan Prev
                        case 233: consumerUsage = 0x00CD; break; // Play/Pause
                        case 234: consumerUsage = 0x00E2; break; // Mute
                        case 235: consumerUsage = 0x00E9; break; // Vol Up
                        case 236: consumerUsage = 0x00EA; break; // Vol Down
                    }
                    
                    if (consumerUsage != 0) {
                        _service->pressConsumer(consumerUsage);
                    } else {
                        _service->press(key);
                    }
                    
                    return Response::success(request, [](JsonVariant& root) {
                        root["status"] = "ok";
                    });
                }
                
                return Response::error(request, 400, "input/missing_key");
            })
    );
}

bool KeyboardApiService::isKeyboardFeatureEnabled() const {
    bool enabled = false;
    RTC::withConfig([&](const RTC::ConfigStore& cfg) {
        enabled = cfg.keyboard.enabled;
    });
    return enabled;
}

esp_err_t KeyboardApiService::ensureKeyboardAvailable(PsychicRequest* request) const {
    if (!isKeyboardFeatureEnabled() || !_service || !_service->isInitialized()) {
        return Response::error(request, 503, kKeyboardDisabledError);
    }
    return ESP_OK;
}

}  // namespace API
