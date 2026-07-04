#pragma once

#include "../../config/System.h"

namespace NOTIFICATIONS::WEBHOOK {

struct WebhookSendValidationInput {
    bool settingsAvailable = false;
    bool enabled = false;
    bool configured = false;
    const char* url = nullptr;
    size_t payloadLen = 0;
    const char* missingSettingsError = "settings_not_set";
};

struct WebhookSendValidationResult {
    bool ok = false;
    const char* error = nullptr;
    const char* url = nullptr;
};

class WebhookSendValidator {
public:
    static WebhookSendValidationResult validate(const WebhookSendValidationInput& input) {
        if (!input.settingsAvailable) {
            return {
                false,
                input.missingSettingsError ? input.missingSettingsError : "settings_not_set",
                nullptr,
            };
        }

        if (!input.enabled) {
            return {false, "mode_not_webhook", nullptr};
        }

        if (!input.configured || !input.url || input.url[0] == '\0') {
            return {false, "not_configured", nullptr};
        }

        if (input.payloadLen == 0 || input.payloadLen >= CONFIG::NOTIFICATIONS::WEBHOOK::MAX_PAYLOAD) {
            return {false, "invalid_length", nullptr};
        }

        return {true, nullptr, input.url};
    }
};

}  // namespace NOTIFICATIONS::WEBHOOK
