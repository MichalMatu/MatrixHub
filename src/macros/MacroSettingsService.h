/**
 * @file MacroSettingsService.h
 * @brief RTC-backed transactional settings service for macro configuration
 */

#pragma once

#include <FS.h>

#include "../config/App.h"
#include "../config/json/MacroConfigJson.h"
#include "../system/rtc/RtcConfig.h"
#include "../system/rtc/RtcStatefulService.h"

namespace MACROS {

class MacroService;

class MacroSettingsService : public RtcStatefulService<RTC::MacroData> {
public:
    MacroSettingsService(FS* fs, MacroService* service);

    static void readState(RTC::MacroData& settings, JsonObject& root);
    static StateUpdateResult updateState(
        JsonObject& jsonObject,
        RTC::MacroData& settings,
        std::string_view originId);

private:
    StateHandlerResult persistAndApply();

    FS* _fs;
    MacroService* _service;
    // Tracks whether macro ownership changed at the persisted config layer,
    // which is the part that needs a restart to fully reconcile runtime.
    bool _lastEnabled;
};

}  // namespace MACROS
