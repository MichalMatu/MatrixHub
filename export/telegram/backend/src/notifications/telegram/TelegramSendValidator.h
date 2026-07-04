#pragma once

#include <Arduino.h>
#include "../../config/App.h"

namespace NOTIFICATIONS {
namespace TELEGRAM {

struct TelegramSendValidationInput {
    bool settingsAvailable = false;
    bool enabled = false;
    bool configured = false;
    const char* chatId = nullptr;
    size_t textLen = 0;
    const char* missingSettingsError = "settings_not_set";
};

struct TelegramSendValidationResult {
    bool ok = false;
    const char* error = nullptr;
    const char* chatId = nullptr;
};

class TelegramSendValidator {
public:
    static TelegramSendValidationResult validate(const TelegramSendValidationInput& input) {
        if (!input.settingsAvailable) {
            return {
                false,
                input.missingSettingsError ? input.missingSettingsError : "settings_not_set",
                nullptr,
            };
        }

        if (!input.enabled) {
            return {false, "mode_not_telegram", nullptr};
        }

        if (!input.configured || !input.chatId || input.chatId[0] == '\0') {
            return {false, "not_configured", nullptr};
        }

        if (input.textLen == 0) {
            return {false, "empty_text", nullptr};
        }

        if (input.textLen > APP::NOTIFICATIONS::TELEGRAM_MAX_TEXT_LEN) {
            return {false, "text_too_long", nullptr};
        }

        return {true, nullptr, input.chatId};
    }
};

}  // namespace TELEGRAM
}  // namespace NOTIFICATIONS
