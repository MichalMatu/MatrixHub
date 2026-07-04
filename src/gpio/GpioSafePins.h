#pragma once

#include "GpioTypes.h"

namespace GPIO {

const GpioPinDefinition* allowedPins();
uint8_t allowedPinCount();
const GpioPinDefinition* findAllowedPin(uint8_t pin);
const GpioPinDefinition* findAllowedPinById(const char* id);
bool isPinAllowed(uint8_t pin);
void applyDefaultConfig(GpioData& data);
bool normalizeConfig(GpioData& data);
bool validateChannelConfig(const GpioChannelConfig& channel);

}  // namespace GPIO

