#pragma once

#include <ArduinoJson.h>
#include "../../system/rtc/types/RtcNotificationTypes.h"

namespace CONFIG {
namespace JSON {

    void loadNotification(JsonObject& obj);
    void saveNotification(JsonObject& obj);

} // namespace JSON
} // namespace CONFIG
