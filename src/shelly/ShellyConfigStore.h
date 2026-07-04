#pragma once

#include <functional>

#include "../system/rtc/types/RtcShellyTypes.h"

namespace SHELLY {
namespace CONFIG_STORE {

RTC::ShellyData copy();
void withConfig(const std::function<void(const RTC::ShellyData&)>& reader);
bool update(const std::function<void(RTC::ShellyData&)>& updater);

}  // namespace CONFIG_STORE
}  // namespace SHELLY
