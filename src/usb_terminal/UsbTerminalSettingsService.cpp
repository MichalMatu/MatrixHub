/**
 * @file UsbTerminalSettingsService.cpp
 * @brief RTC-backed transactional settings service for USB terminal configuration
 */

#include "UsbTerminalSettingsService.h"

#include "../core/config/ConfigManager.h"
#include "../system/logging/Logging.h"

#include <cstring>
#include <services/RestartService.h>

#undef LOG_TAG
#define LOG_TAG "UsbTerminalSettings"

namespace USB_TERMINAL {

UsbTerminalSettingsService::UsbTerminalSettingsService(FS* fs)
    : RtcStatefulService(&RTC::ConfigStore::usbTerminal),
      _fs(fs),
      _lastEnabled(_state.enabled) {
    addUpdateHandler(
        [this](std::string_view originId) {
            (void)originId;
            return persistConfig();
        },
        false);
}

void UsbTerminalSettingsService::readState(RTC::UsbTerminalData& settings, JsonObject& root) {
    char targetPort[sizeof(settings.targetPort)] = {0};
    memcpy(targetPort, settings.targetPort, sizeof(targetPort));
    targetPort[sizeof(targetPort) - 1] = '\0';

    root[CONFIG::Keys::kEnabled] = settings.enabled;
    root[CONFIG::Keys::kIdleTimeoutMs] = settings.idleTimeoutMs;
    root[CONFIG::Keys::kTargetPort].set(String(targetPort));
}

StateUpdateResult UsbTerminalSettingsService::updateState(
    JsonObject& jsonObject,
    RTC::UsbTerminalData& settings,
    std::string_view originId) {
    (void)originId;

    RTC::UsbTerminalData nextState = settings;
    CONFIG::JSON::deserializeUsbTerminal(jsonObject, nextState);

    if (settings.enabled == nextState.enabled &&
        settings.idleTimeoutMs == nextState.idleTimeoutMs &&
        strcmp(settings.targetPort, nextState.targetPort) == 0) {
        return StateUpdateResult::UNCHANGED;
    }

    settings = nextState;
    return StateUpdateResult::CHANGED;
}

StateHandlerResult UsbTerminalSettingsService::persistConfig() {
    if (!_fs || !CONFIG::save(*_fs)) {
        LOGE("Failed to persist USB terminal settings");
        return StateHandlerResult::failure("config/save_failed");
    }

    const bool enabledChanged = (_state.enabled != _lastEnabled);
    _lastEnabled = _state.enabled;
    if (enabledChanged) {
        // USB terminal startup follows boot policy. Restart after toggling enabled so
        // the saved setting and the actually exposed USB runtime stay in sync.
        LOGI("USB terminal enabled state changed - scheduling restart");
        RestartService::scheduleRestart();
    }

    return StateHandlerResult::success();
}

}  // namespace USB_TERMINAL
