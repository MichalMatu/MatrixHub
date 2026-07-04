#pragma once

#include <ArduinoJson.h>
#include "../../system/rtc/types/RtcSystemTypes.h"

namespace CONFIG {
namespace JSON {

    void deserializePower(JsonObject& obj, RTC::PowerData& data);
    void loadPower(JsonObject& obj);
    void savePower(JsonObject& obj);

} // namespace JSON
} // namespace CONFIG
