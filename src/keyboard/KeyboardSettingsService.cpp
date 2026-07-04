/**
 * @file KeyboardSettingsService.cpp
 * @brief RTC-backed transactional settings service for keyboard configuration
 */

#include "KeyboardSettingsService.h"

#include "../core/config/ConfigManager.h"
#include "../system/logging/Logging.h"

#include <services/RestartService.h>

#undef LOG_TAG
#define LOG_TAG "KeyboardSettings"

namespace KEYBOARD {

KeyboardSettingsService::KeyboardSettingsService(FS* fs)
    : RtcStatefulService(&RTC::ConfigStore::keyboard),
      _fs(fs),
      _lastEnabled(_state.enabled) {
    addUpdateHandler(
        [this](std::string_view originId) {
            (void)originId;
            return persistConfig();
        },
        false);
}

void KeyboardSettingsService::readState(RTC::KeyboardData& settings, JsonObject& root) {
    root[CONFIG::Keys::kEnabled] = settings.enabled;
}

StateUpdateResult KeyboardSettingsService::updateState(
    JsonObject& jsonObject,
    RTC::KeyboardData& settings,
    std::string_view originId) {
    (void)originId;

    RTC::KeyboardData nextState = settings;
    CONFIG::JSON::deserializeKeyboard(jsonObject, nextState);

    if (settings.enabled == nextState.enabled) {
        return StateUpdateResult::UNCHANGED;
    }

    settings = nextState;
    return StateUpdateResult::CHANGED;
}

StateHandlerResult KeyboardSettingsService::persistConfig() {
    if (!_fs || !CONFIG::save(*_fs)) {
        LOGE("Failed to persist keyboard settings");
        return StateHandlerResult::failure("config/save_failed");
    }

    const bool enabledChanged = (_state.enabled != _lastEnabled);
    _lastEnabled = _state.enabled;
    if (enabledChanged) {
        // Keyboard ownership is decided during USB boot setup, so saving the flag
        // without a restart would leave persisted config and runtime behavior divergent.
        LOGI("Keyboard enabled state changed - scheduling restart");
        RestartService::scheduleRestart();
    }

    return StateHandlerResult::success();
}

} // namespace KEYBOARD
