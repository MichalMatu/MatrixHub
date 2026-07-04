#pragma once

namespace NOTIFICATIONS::PUSHOVER {

struct PushoverSendValidationInput {
    bool settingsAvailable = false;
    bool enabled = false;
    bool configured = false;
    const char* userKey = nullptr;
    const char* apiToken = nullptr;
    const char* missingSettingsError = "settings_not_set";
    const char* disabledError = "disabled";
};

struct PushoverSendValidationResult {
    bool ok = false;
    const char* error = nullptr;
    const char* userKey = nullptr;
    const char* apiToken = nullptr;
};

class PushoverSendValidator {
public:
    static PushoverSendValidationResult validate(const PushoverSendValidationInput& input) {
        if (!input.settingsAvailable) {
            return {
                false,
                input.missingSettingsError ? input.missingSettingsError : "settings_not_set",
                nullptr,
                nullptr,
            };
        }

        if (!input.enabled) {
            return {
                false,
                input.disabledError ? input.disabledError : "disabled",
                nullptr,
                nullptr,
            };
        }

        if (!input.configured || !input.userKey || input.userKey[0] == '\0' ||
            !input.apiToken || input.apiToken[0] == '\0') {
            return {false, "not_configured", nullptr, nullptr};
        }

        return {true, nullptr, input.userKey, input.apiToken};
    }
};

}  // namespace NOTIFICATIONS::PUSHOVER
