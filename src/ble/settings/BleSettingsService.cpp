/**
 * @file BleSettingsService.cpp
 * @brief BLE settings service - backed by RTC memory
 */

#include "BleSettingsService.h"
#include "../../system/logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "BLE"

namespace BLE {

BleSettingsService::BleSettingsService(PsychicHttpServer* server, FS* fs, SecurityManager* securityManager)
    : RtcStatefulService(&RTC::ConfigStore::ble),
      _httpEndpoint(
          [this](RTC::BleData& s, JsonObject& r) { this->readJson(s, r); },
          [this](JsonObject& r, RTC::BleData& s, std::string_view oid) { return this->updateJson(r, s, oid); },
          this, server, BLE_SETTINGS_SERVICE_PATH, securityManager, AuthenticationPredicates::IS_ADMIN),
      _fs(fs),
      _prevEnabled(_state.enabled) {
}

void BleSettingsService::begin() {
    _httpEndpoint.begin();

    _prevEnabled = _state.enabled;

    addUpdateHandler([this](std::string_view originId) {
        (void)originId;
        return onConfigUpdated();
    }, false);

    LOGI("BLE settings start: enabled=%s sensors=%u",
         _state.enabled ? "true" : "false",
         _state.sensorCount);
}

void BleSettingsService::readJson(RTC::BleData& settings, JsonObject& root) {
    using namespace CONFIG::Keys;
    root[kEnabled] = settings.enabled;

    JsonArray arr = root[kSensors].to<JsonArray>();
    for (uint8_t i = 0; i < settings.sensorCount && i < RTC::kMaxBleSensors; i++) {
        const auto& s = settings.sensors[i];
        if (!s.isEmpty()) {
            JsonObject obj = arr.add<JsonObject>();
            obj[kMac] = s.mac;
            obj[kAlias] = s.alias;
        }
    }
}

StateUpdateResult BleSettingsService::updateJson(JsonObject& root, RTC::BleData& settings, std::string_view originId) {
    (void)originId;
    using namespace CONFIG::Keys;
    bool changed = false;

    if (!root[kEnabled].isNull()) {
        bool newEnabled = root[kEnabled];
        if (newEnabled != settings.enabled) {
            settings.enabled = newEnabled;
            changed = true;
        }
    }

    if (root[kSensors].is<JsonArray>()) {
        JsonArray arr = root[kSensors];
        settings.sensorCount = 0;

        for (JsonObject obj : arr) {
            if (settings.sensorCount >= RTC::kMaxBleSensors) break;

            BLE::BleSensorConfig& cfg = settings.sensors[settings.sensorCount];
            strlcpy(cfg.mac, obj[kMac] | "", sizeof(cfg.mac));
            strlcpy(cfg.alias, obj[kAlias] | "", sizeof(cfg.alias));

            if (!cfg.isEmpty()) {
                settings.sensorCount++;
            }
        }
        changed = true;
    }

    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
}

bool BleSettingsService::isEnabled() const {
    return _state.enabled;
}

void BleSettingsService::setOnSettingsChanged(SettingsChangeCallback callback) {
    _onSettingsChanged = callback;
}

StateHandlerResult BleSettingsService::onConfigUpdated() {
    const bool previousEnabled = _prevEnabled;
    const bool enabledChanged = (_state.enabled != previousEnabled);
    if (enabledChanged) {
        LOGI("BLE enabled state changed: %d -> %d", previousEnabled, _state.enabled);
    }

    // Apply runtime ownership before persisting so API success still means the
    // live scanner actually reached the requested enabled/disabled state.
    if (enabledChanged && _onSettingsChanged && !_onSettingsChanged(_state.enabled)) {
        LOGE("Failed to apply BLE runtime state");
        return StateHandlerResult::failure("config/apply_failed");
    }

    if (!_fs || !CONFIG::save(*_fs)) {
        LOGE("Failed to persist BLE settings");
        if (enabledChanged && _onSettingsChanged && !_onSettingsChanged(previousEnabled)) {
            LOGE("Failed to roll back BLE runtime after save failure");
        }
        return StateHandlerResult::failure("config/save_failed");
    }

    _prevEnabled = _state.enabled;

    LOGI("BLE settings saved (scanner-only mode)");
    return StateHandlerResult::success();
}

} // namespace BLE
