#pragma once

#include <ArduinoJson.h>

#include "../../system/rtc/types/RtcImuTypes.h"

namespace CONFIG {
namespace JSON {

void loadImu(JsonObject& obj);
void saveImu(JsonObject& obj);
void deserializeImu(JsonObject& obj, RTC::ImuData& data);

}  // namespace JSON
}  // namespace CONFIG
