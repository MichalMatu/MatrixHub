/**
 * @file UsbTerminalSettingsService.h
 * @brief RTC-backed transactional settings service for USB terminal configuration
 */

#pragma once

#include <FS.h>

#include "../config/App.h"
#include "../config/json/UsbTerminalConfigJson.h"
#include "../system/rtc/RtcConfig.h"
#include "../system/rtc/RtcStatefulService.h"

namespace USB_TERMINAL {

class UsbTerminalSettingsService : public RtcStatefulService<RTC::UsbTerminalData> {
public:
    explicit UsbTerminalSettingsService(FS* fs);

    static void readState(RTC::UsbTerminalData& settings, JsonObject& root);
    static StateUpdateResult updateState(
        JsonObject& jsonObject,
        RTC::UsbTerminalData& settings,
        std::string_view originId);

private:
    StateHandlerResult persistConfig();

    FS* _fs;
    // Tracks the last persisted enabled flag so restart scheduling stays tied
    // to real USB terminal ownership changes.
    bool _lastEnabled;
};

}  // namespace USB_TERMINAL
