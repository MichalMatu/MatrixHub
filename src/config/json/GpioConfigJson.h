#pragma once

#include <ArduinoJson.h>

#include "../../gpio/GpioTypes.h"

namespace CONFIG {
namespace JSON {

bool deserializeGpio(JsonObject& obj, GPIO::GpioData& data);
bool loadGpio(JsonObject& obj);
void saveGpio(JsonObject& obj);

}  // namespace JSON
}  // namespace CONFIG
