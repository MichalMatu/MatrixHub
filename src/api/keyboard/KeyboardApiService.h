#pragma once

#include <core/HttpEndpoint.h>
#include <PsychicHttpServer.h>
#include <security/SecurityManager.h>
#include <memory>

#include "../BaseApiService.h"
#include "../../keyboard/KeyboardSettingsService.h"

namespace KEYBOARD { class KeyboardService; }

namespace API {

/**
 * @class KeyboardApiService
 * @brief HTTP API for Keyboard control
 * 
 * Endpoints:
 *   GET  /api/keyboard/config - Read direct keyboard feature config
 *   POST /api/keyboard/config - Update direct keyboard feature config
 *   POST /api/keyboard/type   - Type text string
 *   POST /api/keyboard/press  - Press single key (code)
 */
class KeyboardApiService : public BaseApiService {
public:
    KeyboardApiService(PsychicHttpServer* server,
                       SecurityManager* securityManager,
                       POWER::PowerManager* powerManager,
                       KEYBOARD::KeyboardService* service,
                       KEYBOARD::KeyboardSettingsService* settings);
    void begin() override;

private:
    bool isKeyboardFeatureEnabled() const;
    esp_err_t ensureKeyboardAvailable(PsychicRequest* request) const;

    KEYBOARD::KeyboardService* _service;
    // Ownership lives above the API layer so keyboard config follows the same
    // lifecycle pattern as other registry-managed services.
    KEYBOARD::KeyboardSettingsService* _settings;
    std::unique_ptr<HttpEndpoint<RTC::KeyboardData>> _configEndpoint;
};

}  // namespace API
