#pragma once

#include <functional>

#include "GpioTypes.h"

namespace GPIO {
namespace CONFIG_STORE {

GpioData copy();
void withConfig(const std::function<void(const GpioData&)>& reader);
bool update(const std::function<void(GpioData&)>& updater);
bool resetToDefaults();

}  // namespace CONFIG_STORE
}  // namespace GPIO

