#pragma once

#include <functional>

#include "../../rtc/types/RtcSystemTypes.h"

namespace SYSTEM {
namespace HEARTBEAT_CONFIG {

RTC::HeartbeatData copy();
void withConfig(const std::function<void(const RTC::HeartbeatData&)>& reader);
bool update(const std::function<void(RTC::HeartbeatData&)>& updater);

}  // namespace HEARTBEAT_CONFIG
}  // namespace SYSTEM
