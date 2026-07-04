#include "ShellyConfigJson.h"
#include "../App.h"
#include "../System.h"
#include "../../shelly/ShellyConfigStore.h"
#include "../../system/rtc/RtcConfig.h"

namespace CONFIG {
namespace JSON {

namespace {

const SHELLY::ShellyDevice* findPreviousDeviceById(const RTC::ShellyData& data, const char* id) {
    if (!id || id[0] == '\0') {
        return nullptr;
    }

    for (uint8_t i = 0; i < data.deviceCount; i++) {
        if (strcmp(data.devices[i].id, id) == 0) {
            return &data.devices[i];
        }
    }

    return nullptr;
}

uint8_t resolveGeneration(JsonObject& dev, const RTC::ShellyData& previousData, const char* id) {
    JsonVariant generationValue = dev[Keys::kGeneration];
    if (!generationValue.isNull()) {
        uint8_t generation = generationValue | 2;
        return (generation == 1) ? 1 : 2;
    }

    if (const auto* previous = findPreviousDeviceById(previousData, id)) {
        if (previous->generation == 1 || previous->generation == 2) {
            return previous->generation;
        }
    }

    // Legacy configs predate explicit generation selection. Start with Gen1 and let
    // the runtime auto-heal to Gen2 if the RPC endpoint responds successfully.
    return 1;
}

} // namespace

// ---------------------------------------------------------------------------
// deserializeShelly — shared by loadShelly (file) AND potential API endpoint.
// Overwrites entire struct (full-replace, not partial-update).
// ---------------------------------------------------------------------------
void deserializeShelly(JsonObject& obj, RTC::ShellyData& s) {
    RTC::ShellyData previousData = s;

    // Clear existing devices
    for (uint8_t i = 0; i < RTC::kMaxShellyDevices; i++) {
        s.devices[i] = SHELLY::ShellyDevice();
        s.devices[i].enabled = false;
    }
    
    s.deviceCount = 0;
    if (obj[Keys::kDevices].is<JsonArray>()) {
        for (JsonObject dev : obj[Keys::kDevices].as<JsonArray>()) {
            if (s.deviceCount >= RTC::kMaxShellyDevices) break;
            
            auto& d = s.devices[s.deviceCount];
            d = SHELLY::ShellyDevice();
            
            const char* id = dev[Keys::kId] | "";
            strlcpy(d.id, id, sizeof(d.id));
            
            const char* ip = dev[Keys::kIp] | "";
            strlcpy(d.ip, ip, sizeof(d.ip));
            
            const char* name = dev[Keys::kName] | "";
            strlcpy(d.name, name, sizeof(d.name));
            
            d.relayIndex = dev[Keys::kRelayIndex] | 0;
            d.enabled = dev[Keys::kEnabled] | true;
            d.generation = resolveGeneration(dev, previousData, d.id);
            s.deviceCount++;
        }
    }
}

void loadShelly(JsonObject& obj) {
    RTC::ShellyData next = SHELLY::CONFIG_STORE::copy();
    deserializeShelly(obj, next);

    uint8_t enabledCount = 0;
    for (uint8_t i = 0; i < next.deviceCount; i++) {
        if (next.devices[i].enabled) {
            enabledCount++;
        }
    }

    if (!SHELLY::CONFIG_STORE::update([&](RTC::ShellyData& shelly) {
            shelly = next;
        })) {
        return;
    }

    RTC::updateConfigSection(&RTC::ConfigStore::shelly, [&](RTC::ShellySummaryData& shelly) {
        shelly.deviceCount = next.deviceCount;
        shelly.enabledCount = enabledCount;
    });
}

void saveShelly(JsonObject& obj) {
    RTC::ShellyData s = SHELLY::CONFIG_STORE::copy();
    
    JsonArray devices = obj[Keys::kDevices].to<JsonArray>();
    for (uint8_t i = 0; i < s.deviceCount; i++) {
        JsonObject d = devices.add<JsonObject>();
        d[Keys::kId].set(String(s.devices[i].id));
        d[Keys::kName].set(String(s.devices[i].name));
        d[Keys::kIp].set(String(s.devices[i].ip));
        d[Keys::kRelayIndex] = s.devices[i].relayIndex;
        d[Keys::kEnabled] = s.devices[i].enabled;
        d[Keys::kGeneration] = s.devices[i].generation;
    }
}

} // namespace JSON
} // namespace CONFIG
