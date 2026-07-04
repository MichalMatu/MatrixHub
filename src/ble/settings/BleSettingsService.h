/**
 * @file BleSettingsService.h
 * @brief Configuration service for scanner-only BLE
 */

#pragma once

#include "../../system/rtc/RtcStatefulService.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../config/App.h"
#include "core/config/ConfigManager.h"
#include <core/HttpEndpoint.h>
#include "../BleTypes.h"
#include <string_view>

#define BLE_SETTINGS_SERVICE_PATH "/api/ble/settings"

namespace BLE {

class BleSettingsService : public RtcStatefulService<RTC::BleData> {
public:
    BleSettingsService(PsychicHttpServer* server, FS* fs, SecurityManager* securityManager);

    void begin();
    bool isEnabled() const;

    // The callback applies the live runtime state for the persisted enabled
    // flag. Returning false means "persisted RTC state changed, but runtime did
    // not reach the requested state", so the outer transaction must fail.
    using SettingsChangeCallback = std::function<bool(bool enabled)>;
    void setOnSettingsChanged(SettingsChangeCallback callback);

private:
    HttpEndpoint<RTC::BleData> _httpEndpoint;
    FS* _fs;
    void readJson(RTC::BleData& settings, JsonObject& root);
    StateUpdateResult updateJson(JsonObject& root, RTC::BleData& settings, std::string_view originId);
    SettingsChangeCallback _onSettingsChanged = nullptr;
    bool _prevEnabled{false};

    StateHandlerResult onConfigUpdated();
};

} // namespace BLE
