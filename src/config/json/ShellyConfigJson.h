#pragma once
#include <ArduinoJson.h>
#include "../../system/rtc/types/RtcShellyTypes.h"

namespace CONFIG {
namespace JSON {

    void deserializeShelly(JsonObject& obj, RTC::ShellyData& data);
    void loadShelly(JsonObject& obj);
    void saveShelly(JsonObject& obj);

} // namespace JSON
} // namespace CONFIG
