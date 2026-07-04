#pragma once

#include <ArduinoJson.h>
#include "../../system/rtc/types/RtcUsbTerminalTypes.h"

namespace CONFIG {
namespace JSON {

void loadUsbTerminal(JsonObject& obj);
void saveUsbTerminal(JsonObject& obj);
void deserializeUsbTerminal(JsonObject& obj, RTC::UsbTerminalData& data);

} // namespace JSON
} // namespace CONFIG
