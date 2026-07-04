/**
 * @file AirMouseSettingsService.h
 * @brief RTC-backed transactional settings service for AirMouse config
 */

#pragma once

#include <FS.h>

#include "../config/App.h"
#include "../config/json/AirMouseConfigJson.h"
#include "../system/rtc/RtcConfig.h"
#include "../system/rtc/RtcStatefulService.h"

namespace AIRMOUSE {

class AirMouseSettingsService : public RtcStatefulService<RTC::AirMouseData> {
public:
    explicit AirMouseSettingsService(FS* fs, std::function<void()> onConfigApplied = {});

    static void readState(RTC::AirMouseData& settings, JsonObject& root);
    static StateUpdateResult updateState(
        JsonObject& jsonObject,
        RTC::AirMouseData& settings,
        std::string_view originId);

private:
    StateHandlerResult persistAndApply();

    FS* _fs;
    std::function<void()> _onConfigApplied;
    // Cache the last ownership-related toggles so we can separate live tuning
    // changes from config changes that require a restart to take full effect.
    bool _lastMovementEnabled;
    bool _lastClickEnabled;
    bool _lastJigglerEnabled;
};

}  // namespace AIRMOUSE
