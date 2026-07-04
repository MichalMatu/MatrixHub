#pragma once

#include <ArduinoJson.h>
#include "../../system/rtc/types/RtcWifiSensingTypes.h"

namespace CONFIG {
namespace JSON {

    void serializeWifiSensing(JsonObject& obj, const RTC::WifiSensingData& data);
    void deserializeWifiSensing(JsonObject& obj, RTC::WifiSensingData& data);
    void loadWifiSensing(JsonObject& obj);
    void saveWifiSensing(JsonObject& obj);

} // namespace JSON
} // namespace CONFIG
