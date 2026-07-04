/**
 * @file MacroSettingsService.cpp
 * @brief RTC-backed transactional settings service for macro configuration
 */

#include "MacroSettingsService.h"

#include "../core/config/ConfigManager.h"
#include "../system/logging/Logging.h"
#include "MacroService.h"

#include <cstring>
#include <services/RestartService.h>

#undef LOG_TAG
#define LOG_TAG "MacroSettings"

namespace MACROS {

MacroSettingsService::MacroSettingsService(FS* fs, MacroService* service)
    : RtcStatefulService(&RTC::ConfigStore::macros),
      _fs(fs),
      _service(service),
      _lastEnabled(_state.enabled) {
    addUpdateHandler(
        [this](std::string_view originId) {
            (void)originId;
            return persistAndApply();
        },
        false);
}

void MacroSettingsService::readState(RTC::MacroData& settings, JsonObject& root) {
    root[CONFIG::Keys::kEnabled] = settings.enabled;
    root[CONFIG::Keys::kBootDelay] = settings.bootDelay;
    root[CONFIG::Keys::kBootScript].set(String(settings.bootScript));
}

StateUpdateResult MacroSettingsService::updateState(
    JsonObject& jsonObject,
    RTC::MacroData& settings,
    std::string_view originId) {
    (void)originId;

    RTC::MacroData nextState = settings;
    const JsonObjectConst input = jsonObject;
    CONFIG::JSON::deserializeMacros(input, nextState);

    if (memcmp(&settings, &nextState, sizeof(RTC::MacroData)) == 0) {
        return StateUpdateResult::UNCHANGED;
    }

    settings = nextState;
    return StateUpdateResult::CHANGED;
}

StateHandlerResult MacroSettingsService::persistAndApply() {
    if (!_fs || !CONFIG::save(*_fs)) {
        LOGE("Failed to persist macro settings");
        return StateHandlerResult::failure("config/save_failed");
    }

    const bool enabledChanged = (_state.enabled != _lastEnabled);
    _lastEnabled = _state.enabled;
    if (enabledChanged) {
        // Macro enable/disable affects boot-time ownership. Apply a quick stop when
        // turning it off, then restart so the persisted state becomes the runtime state.
        if (_service && !_state.enabled) {
            _service->applySettings();
        }
        LOGI("Macro enabled state changed - scheduling restart");
        RestartService::scheduleRestart();
        return StateHandlerResult::success();
    }

    if (_service) {
        _service->applySettings();
    }

    return StateHandlerResult::success();
}

}  // namespace MACROS
