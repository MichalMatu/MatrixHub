/**
 * @file KeyboardSettingsService.h
 * @brief RTC-backed transactional settings service for keyboard configuration
 */

#pragma once

#include <FS.h>

#include "../config/App.h"
#include "../config/json/KeyboardConfigJson.h"
#include "../system/rtc/RtcConfig.h"
#include "../system/rtc/RtcStatefulService.h"

namespace KEYBOARD {

class KeyboardSettingsService : public RtcStatefulService<RTC::KeyboardData> {
public:
    explicit KeyboardSettingsService(FS* fs);

    static void readState(RTC::KeyboardData& settings, JsonObject& root);
    static StateUpdateResult updateState(
        JsonObject& jsonObject,
        RTC::KeyboardData& settings,
        std::string_view originId);

private:
    StateHandlerResult persistConfig();

    FS* _fs;
    // Tracks the last persisted enabled flag so restart scheduling only happens
    // for real ownership changes, not every successful config save.
    bool _lastEnabled;
};

} // namespace KEYBOARD
