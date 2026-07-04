/**
 * @file AirMouseSettingsService.cpp
 * @brief RTC-backed transactional settings service for AirMouse config
 */

#include "AirMouseSettingsService.h"

#include "../core/config/ConfigManager.h"
#include "../system/logging/Logging.h"

#include <cstring>
#include <services/RestartService.h>

#undef LOG_TAG
#define LOG_TAG "AirMouseSettings"

namespace AIRMOUSE {

AirMouseSettingsService::AirMouseSettingsService(FS* fs, std::function<void()> onConfigApplied)
    : RtcStatefulService(&RTC::ConfigStore::airMouse),
      _fs(fs),
      _onConfigApplied(std::move(onConfigApplied)),
      // Seed the ownership cache from persisted RTC state so the first save only
      // restarts when a user actually changed one of the boot-owned toggles.
      _lastMovementEnabled(_state.movementEnabled),
      _lastClickEnabled(_state.clickEnabled),
      _lastJigglerEnabled(_state.jiggler.mode != RTC::MouseJigglerMode::JIGGLER_OFF) {
    addUpdateHandler(
        [this](std::string_view originId) {
            (void)originId;
            return persistAndApply();
        },
        false);
}

void AirMouseSettingsService::readState(RTC::AirMouseData& settings, JsonObject& root) {
    root[CONFIG::Keys::kMovementEnabled] = settings.movementEnabled;
    root[CONFIG::Keys::kClickEnabled] = settings.clickEnabled;
    // Do not surface the deprecated transport field. AirMouse ownership is now
    // derived from the USB-backed movement/click/jiggler toggles only.
    root[CONFIG::Keys::kSensitivityX] = settings.sensitivityX;
    root[CONFIG::Keys::kSensitivityY] = settings.sensitivityY;
    root[CONFIG::Keys::kDeadzone] = settings.deadzone;
    root[CONFIG::Keys::kAccelerationEnabled] = settings.accelerationEnabled;
    root[CONFIG::Keys::kAccelerationFactor] = settings.accelerationFactor;
    root[CONFIG::Keys::kTapThresholdG] = settings.tapThresholdG;
    root[CONFIG::Keys::kClickDebounceMs] = settings.clickDebounceMs;
    root[CONFIG::Keys::kDoubleClickWindowMs] = settings.doubleClickWindowMs;
    root[CONFIG::Keys::kClickSource] = static_cast<uint8_t>(settings.clickSource);
    root[CONFIG::Keys::kSingleClickAction] = static_cast<uint8_t>(settings.singleClickAction);
    root[CONFIG::Keys::kDoubleClickAction] = static_cast<uint8_t>(settings.doubleClickAction);
    root[CONFIG::Keys::kTripleClickAction] = static_cast<uint8_t>(settings.tripleClickAction);
    root[CONFIG::Keys::kSingleClickScript].set(String(settings.singleClickScript));
    root[CONFIG::Keys::kDoubleClickScript].set(String(settings.doubleClickScript));
    root[CONFIG::Keys::kTripleClickScript].set(String(settings.tripleClickScript));
    root[CONFIG::Keys::kEuroMinCutoff] = settings.euroMinCutoff;
    root[CONFIG::Keys::kEuroBeta] = settings.euroBeta;
    root[CONFIG::Keys::kEuroDCutoff] = settings.euroDCutoff;

    JsonObject jiggler = root[CONFIG::Keys::kJiggler].to<JsonObject>();
    jiggler[CONFIG::Keys::kMode] = static_cast<uint8_t>(settings.jiggler.mode);
    jiggler[CONFIG::Keys::kInterval] = settings.jiggler.interval;
    jiggler[CONFIG::Keys::kMoveDistance] = settings.jiggler.moveDistance;
    jiggler[CONFIG::Keys::kRandomInterval] = settings.jiggler.randomInterval;
}

StateUpdateResult AirMouseSettingsService::updateState(
    JsonObject& jsonObject,
    RTC::AirMouseData& settings,
    std::string_view originId) {
    (void)originId;

    RTC::AirMouseData nextState = settings;
    CONFIG::JSON::deserializeAirMouse(jsonObject, nextState);

    if (memcmp(&settings, &nextState, sizeof(RTC::AirMouseData)) == 0) {
        return StateUpdateResult::UNCHANGED;
    }

    settings = nextState;
    return StateUpdateResult::CHANGED;
}

StateHandlerResult AirMouseSettingsService::persistAndApply() {
    if (!_fs || !CONFIG::save(*_fs)) {
        LOGE("Failed to persist AirMouse settings");
        return StateHandlerResult::failure("config/save_failed");
    }

    const bool jigglerEnabled =
        _state.jiggler.mode != RTC::MouseJigglerMode::JIGGLER_OFF;
    const bool criticalToggleChanged =
        _state.movementEnabled != _lastMovementEnabled ||
        _state.clickEnabled != _lastClickEnabled ||
        jigglerEnabled != _lastJigglerEnabled;

    // These toggles decide whether AirMouse owns motion/click/jiggler runtime paths.
    // Keep normal config updates live, but restart when ownership itself changed.
    _lastMovementEnabled = _state.movementEnabled;
    _lastClickEnabled = _state.clickEnabled;
    _lastJigglerEnabled = jigglerEnabled;

    if (_onConfigApplied) {
        _onConfigApplied();
    }

    if (criticalToggleChanged) {
        LOGI("AirMouse runtime ownership changed - scheduling restart");
        RestartService::scheduleRestart();
    }

    return StateHandlerResult::success();
}

}  // namespace AIRMOUSE
