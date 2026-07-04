/**
 * @file UdpSettingsService.h
 * @brief RTC-backed transactional settings service for UDP pusher config
 */

#pragma once

#include <FS.h>

#include "../config/App.h"
#include "../config/json/SystemConfigJson.h"
#include "../system/rtc/RtcConfig.h"
#include "../system/rtc/RtcStatefulService.h"

namespace UDPPUSH {

class UdpSettingsService : public RtcStatefulService<RTC::UdpPusherData> {
public:
    explicit UdpSettingsService(FS* fs);

    static void readState(RTC::UdpPusherData& settings, JsonObject& root);
    static StateUpdateResult updateState(
        JsonObject& jsonObject,
        RTC::UdpPusherData& settings,
        std::string_view originId);

private:
    static const char* formatToString(RTC::UdpFormat format);
    StateHandlerResult persistConfig();

    FS* _fs;
};

}  // namespace UDPPUSH
