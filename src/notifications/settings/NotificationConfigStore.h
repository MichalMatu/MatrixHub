#pragma once

#include <functional>

#include "../../system/rtc/types/RtcNotificationTypes.h"

namespace NOTIFICATIONS {
namespace CONFIG_STORE {

RTC::NotificationData copy();
void withConfig(const std::function<void(const RTC::NotificationData&)>& reader);
bool update(const std::function<void(RTC::NotificationData&)>& updater);

}  // namespace CONFIG_STORE
}  // namespace NOTIFICATIONS
