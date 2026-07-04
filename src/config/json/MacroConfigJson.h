#pragma once

#include <ArduinoJson.h>
#include "../../system/rtc/RtcConfig.h"

namespace CONFIG {
namespace JSON {

void deserializeMacros(JsonObjectConst obj, RTC::MacroData& cfg);
void loadMacros(JsonObjectConst obj);
void saveMacros(JsonObject obj);

} // namespace JSON
} // namespace CONFIG
