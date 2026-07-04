#pragma once
#include <ArduinoJson.h>
#include "../../system/rtc/types/RtcBleTypes.h"

namespace CONFIG {
namespace JSON {

    void deserializeBle(JsonObject& obj, RTC::BleData& data);
    void loadBle(JsonObject& obj);
    void saveBle(JsonObject& obj);

} // namespace JSON
} // namespace CONFIG
