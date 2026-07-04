#include "BleConfigJson.h"
#include "../App.h"
#include "../../system/rtc/RtcConfig.h"

namespace CONFIG {
namespace JSON {

void deserializeBle(JsonObject& obj, RTC::BleData& b) {
    if (obj[Keys::kEnabled].is<bool>()) {
        bool v = obj[Keys::kEnabled].as<bool>();
        b.enabled = v;
    }

    if (obj[Keys::kSensors].is<JsonArray>()) {
        b.sensorCount = 0;
        for (JsonObject sensor : obj[Keys::kSensors].as<JsonArray>()) {
            if (b.sensorCount >= RTC::kMaxBleSensors) break;

            const char* mac = sensor[Keys::kMac] | "";
            const char* alias = sensor[Keys::kAlias] | "";

            if (strlen(mac) > 0) {
                strlcpy(b.sensors[b.sensorCount].mac, mac, sizeof(b.sensors[0].mac));
                strlcpy(b.sensors[b.sensorCount].alias, alias, sizeof(b.sensors[0].alias));
                b.sensorCount++;
            }
        }
    }
}

void loadBle(JsonObject& obj) {
    RTC::updateConfigSection(&RTC::ConfigStore::ble, [&](RTC::BleData& ble) {
        deserializeBle(obj, ble);
    });
}

void saveBle(JsonObject& obj) {
    RTC::BleData b{};
    RTC::withConfig([&](const RTC::ConfigStore& cfg) {
        b = cfg.ble;
    });
    obj[Keys::kEnabled] = b.enabled;

    JsonArray sensors = obj[Keys::kSensors].to<JsonArray>();
    for (uint8_t i = 0; i < b.sensorCount; i++) {
        JsonObject s = sensors.add<JsonObject>();
        s[Keys::kMac].set(String(b.sensors[i].mac));
        s[Keys::kAlias].set(String(b.sensors[i].alias));
    }
}

} // namespace JSON
} // namespace CONFIG
