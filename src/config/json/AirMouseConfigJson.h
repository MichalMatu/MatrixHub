#pragma once
#include <ArduinoJson.h>

#include "../../system/rtc/types/RtcAirMouseTypes.h"

namespace CONFIG {
namespace JSON {

    void loadAirMouse(JsonObject& obj);
    void saveAirMouse(JsonObject& obj);
    
    // Helper for API and Load - updates only fields present in JSON
    void deserializeAirMouse(JsonObject& obj, RTC::AirMouseData& out);

} // namespace JSON
} // namespace CONFIG
