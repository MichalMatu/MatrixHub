#pragma once

#include <ArduinoJson.h>
#include "../../system/rtc/types/RtcCompensationTypes.h"

namespace CONFIG {
namespace JSON {

    void deserializeCompensation(JsonObject& obj, RTC::CompensationData& data);
    void loadCompensation(JsonObject& obj);
    void saveCompensation(JsonObject& obj);

} // namespace JSON
} // namespace CONFIG
