#include "ShellyProtocol.h"
#include <stdio.h>
#include <ArduinoJson.h>
#include "../../system/memory/PsramAllocator.h"
#include "../../system/logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "ShellyProto"

namespace SHELLY {

bool ShellyProtocol::buildGen1ControlUrl(char* buffer, size_t size, const char* ip, uint8_t relayIndex, bool turnOn) {
    if (!buffer || size == 0 || !ip) return false;
    
    // snprintf returns number of characters that WOULD have been written
    int len = snprintf(buffer, size, "http://%s/relay/%u?turn=%s", 
             ip, relayIndex, turnOn ? "on" : "off");
             
    // Check for success and truncation
    return len > 0 && (size_t)len < size;
}

bool ShellyProtocol::buildGen1StatusUrl(char* buffer, size_t size, const char* ip) {
    if (!buffer || size == 0 || !ip) return false;
    
    // Gen 1: /status gives global info (relays, meters, etc)
    int len = snprintf(buffer, size, "http://%s/status", ip);
    
    return len > 0 && (size_t)len < size;
}

bool ShellyProtocol::parseGen1Status(char* jsonResponse, size_t length, uint8_t relayIndex, ShellyStatus& outStatus) {
    if (!jsonResponse || length == 0) return false;

    // Allocate slightly larger document for full status
    // Zero-Copy mode: ArduinoJson uses the input buffer (jsonResponse) for strings.
    // This significantly reduces heap usage.
    SYSTEM::SpiRamJsonDocument doc(4096); 
    
    DeserializationError error = deserializeJson(doc, jsonResponse, length);
    
    if (error) {
        return false;
    }
    
    // Check for relays array
    JsonArray relays = doc["relays"];
    if (relays.isNull() || relays.size() <= relayIndex) {
        return false;
    }
    
    outStatus.isOn = relays[relayIndex]["ison"] | false;
    
    // Meters
    outStatus.hasPower = false;
    outStatus.power = 0.0f;
    outStatus.energy = 0.0f;
    outStatus.voltage = 0.0f;
    outStatus.current = 0.0f;

    JsonArray meters = doc["meters"];
    // Some devices match meter index to relay index (Shelly 2.5)
    // Some have single meter.
    if (!meters.isNull() && meters.size() > relayIndex) {
        JsonObject meter = meters[relayIndex];
        outStatus.hasPower = true;
        outStatus.power = meter["power"] | 0.0f;
        outStatus.energy = meter["total"] | 0.0f;
        
        // Voltage/Current might not be present on all Gen 1 devices
        // 'voltage' and 'current' fields are often unavailable.
    }

    // Temperature (Gen 1: "tmp": {"tC": ...})
    JsonObject tmp = doc["tmp"];
    if (!tmp.isNull()) {
        outStatus.temperature = tmp["tC"] | 0.0f;
    }

    // RSSI (Gen 1: "wifi_sta": {"rssi": ...})
    JsonObject wifi_sta = doc["wifi_sta"];
    if (!wifi_sta.isNull()) {
        outStatus.rssi = wifi_sta["rssi"] | 0;
    }
    
    return true;
}


bool ShellyProtocol::buildGen2StatusUrl(char* buffer, size_t size, const char* ip) {
    if (!buffer || size == 0 || !ip) return false;
    
    // Gen 2/3: RPC method for status
    int len = snprintf(buffer, size, "http://%s/rpc/Shelly.GetStatus", ip);
    
    return len > 0 && (size_t)len < size;
}

bool ShellyProtocol::buildGen2ControlUrl(char* buffer, size_t size, const char* ip, uint8_t relayIndex, bool turnOn) {
    if (!buffer || size == 0 || !ip) return false;
    
    // Gen 2/3: RPC method for control
    // http://<ip>/rpc/Switch.Set?id=<index>&on=<true|false>
    int len = snprintf(buffer, size, "http://%s/rpc/Switch.Set?id=%u&on=%s", 
             ip, relayIndex, turnOn ? "true" : "false");
    
    return len > 0 && (size_t)len < size;
}

bool ShellyProtocol::parseGen2Status(char* jsonResponse, size_t length, uint8_t relayIndex, ShellyStatus& outStatus) {
    if (!jsonResponse || length == 0) return false;

    // Zero-Copy mode: ArduinoJson uses the input buffer (jsonResponse) for strings.
    // This significantly reduces heap usage.
    SYSTEM::SpiRamJsonDocument doc(8192); 
    
    DeserializationError error = deserializeJson(doc, jsonResponse, length);
    
    if (error) {
        return false;
    }
    
    // Construct dynamic key "switch:N"
    char switchKey[16];
    snprintf(switchKey, sizeof(switchKey), "switch:%u", relayIndex);
    
    JsonObject sw = doc[switchKey];
    if (sw.isNull()) {
        // Fallback: maybe just "switch" if it's an array? No, Shelly Gen 2 uses key names.
        // Or "switches" array?
        // Let's stick to standard Gen 2 "switch:0" pattern for Plus/Pro/Gen3 devices.
        return false;
    }
    
    // Output state
    outStatus.isOn = sw["output"] | false;
    
    // Meters
    // Some Shelly firmware intermittently omits metering keys.
    // Treat missing "apower" as "no metering in this response" to preserve cached values.
    JsonVariant apower = sw["apower"];
    outStatus.hasPower = !apower.isNull();

    if (outStatus.hasPower) {
        // Use explicit type casting to avoid ambiguity
        outStatus.power = apower.as<float>();
        outStatus.voltage = sw["voltage"].as<float>();
        outStatus.current = sw["current"].as<float>();
    }

    // Warn if active relay reports 0 power (changed to DEBUG due to known Shelly quirk)
    if (outStatus.isOn && outStatus.power == 0.0f) {
        LOGD("Shelly reports 0.0W while ON! (V=%.1f)", outStatus.voltage);
    }
    
    // Energy is nested: "aenergy": {"total": 123.45}
    if (outStatus.hasPower && sw["aenergy"].is<JsonObject>()) {
        outStatus.energy = sw["aenergy"]["total"].as<float>();
    } else if (outStatus.hasPower) {
        outStatus.energy = 0.0f;
    }

    // Temperature (Gen 2: switch:0 -> temperature -> tC)
    if (sw["temperature"].is<JsonObject>()) {
        outStatus.temperature = sw["temperature"]["tC"].as<float>();
    }

    // RSSI (Gen 2: wifi -> rssi)
    JsonObject wifi = doc["wifi"];
    if (!wifi.isNull()) {
        outStatus.rssi = wifi["rssi"] | 0;
    }
    
    return true;
}

} // namespace SHELLY
