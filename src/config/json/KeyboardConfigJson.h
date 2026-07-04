#pragma once

#include <ArduinoJson.h>

#include "../../system/rtc/types/RtcKeyboardTypes.h"

namespace CONFIG {
namespace JSON {

void loadKeyboard(JsonObject& obj);
void saveKeyboard(JsonObject& obj);
void deserializeKeyboard(JsonObject& obj, RTC::KeyboardData& data);

} // namespace JSON
} // namespace CONFIG
