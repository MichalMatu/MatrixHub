#include "KeyboardConfigJson.h"

#include "../../system/rtc/RtcConfig.h"
#include "../App.h"

namespace CONFIG {
namespace JSON {

void deserializeKeyboard(JsonObject& obj, RTC::KeyboardData& data) {
    if (obj[Keys::kEnabled].is<bool>()) {
        data.enabled = obj[Keys::kEnabled].as<bool>();
    }
}

void loadKeyboard(JsonObject& obj) {
    if (obj.isNull()) {
        return;
    }

    RTC::updateConfigSection(&RTC::ConfigStore::keyboard, [&](RTC::KeyboardData& keyboard) {
        deserializeKeyboard(obj, keyboard);
    });
}

void saveKeyboard(JsonObject& obj) {
    const RTC::KeyboardData keyboard = RTC::copyConfigSection(&RTC::ConfigStore::keyboard);
    obj[Keys::kEnabled] = keyboard.enabled;
}

} // namespace JSON
} // namespace CONFIG
