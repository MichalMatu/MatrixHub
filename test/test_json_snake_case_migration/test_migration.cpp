#define NOMINMAX
#include <unity.h>
#include <ArduinoJson.h>

// Undefine just in case (e.g. if ArduinoJson defines them?)
#undef min
#undef max

// Include implementations directly to link them in native environment
#include "../../src/config/json/BleConfigJson.cpp"
#include "../../src/config/json/ShellyConfigJson.cpp"
// SystemConfigJson.cpp pulls in too many dependencies, so we might need to be careful.
// Let's try including it.
#include "../../src/config/json/SystemConfigJson.cpp"
#include "../../src/system/health/heartbeat/HeartbeatConfigSanitizer.cpp"

namespace LOG {
    Settings Logging::_settings;
    void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
        (void)level;
        (void)tag;
        (void)fmt;
    }
    void Logging::logSection(const char* title) { (void)title; }
    void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {
        (void)taskName;
        (void)stackSize;
    }
    const char* Logging::levelToString(esp_log_level_t level) { return "INFO"; }
    esp_log_level_t Logging::stringToLevel(std::string_view str, esp_log_level_t def) { return def; }
}

#include "system/rtc/types/RtcBleTypes.h"
#include "system/rtc/types/RtcShellyTypes.h"
#include "system/rtc/types/RtcSystemTypes.h"
#include "system/rtc/RtcConfig.h"

// Mock RTC Config Store
RTC::ConfigStore mockStore;

namespace RTC {
    ConfigStore& getMutableConfig() {
        return mockStore;
    }
    const ConfigStore& getConfig() {
        return mockStore;
    }
    void withConfig(const std::function<void(const ConfigStore&)>& reader) { reader(mockStore); }
    bool updateConfig(const std::function<void(ConfigStore&)>& updater) {
        updater(mockStore);
        return true;
    }
}

namespace SHELLY {
namespace CONFIG_STORE {
    static RTC::ShellyData mockShelly;

    RTC::ShellyData copy() { return mockShelly; }
    void withConfig(const std::function<void(const RTC::ShellyData&)>& reader) { reader(mockShelly); }
    bool update(const std::function<void(RTC::ShellyData&)>& updater) {
        updater(mockShelly);
        return true;
    }
}
}

namespace SYSTEM {
namespace HEARTBEAT_CONFIG {
    static RTC::HeartbeatData mockHeartbeat;

    RTC::HeartbeatData copy() { return mockHeartbeat; }
    void withConfig(const std::function<void(const RTC::HeartbeatData&)>& reader) { reader(mockHeartbeat); }
    bool update(const std::function<void(RTC::HeartbeatData&)>& updater) {
        updater(mockHeartbeat);
        return true;
    }
}
}

// Helper for resetting data
void resetBleData(RTC::BleData& data) {
    memset(&data, 0, sizeof(data));
    data.enabled = false;
}

void resetShellyData(RTC::ShellyData& data) {
    data.deviceCount = 0;
    // Reset first device
    if (RTC::kMaxShellyDevices > 0) {
        data.devices[0] = SHELLY::ShellyDevice();
        data.devices[0].relayIndex = 255; // Sentinel
        data.devices[0].enabled = false;
    }
}

void resetUdpData(RTC::UdpPusherData& data) {
    data.enabled = false;
    data.port = 0;
    data.intervalMs = 0;
}

// ============================================================================
// BLE Tests
// ============================================================================
void test_ble_migration_scanner_only_contract() {
    RTC::BleData data;
    resetBleData(data);

    JsonDocument doc;
    doc["enabled"] = true;

    JsonArray sensors = doc["sensors"].to<JsonArray>();
    JsonObject sensor = sensors.add<JsonObject>();
    sensor["mac"] = "AA:BB:CC:DD:EE:FF";
    sensor["alias"] = "Living Room";

    JsonObject obj = doc.as<JsonObject>();
    CONFIG::JSON::deserializeBle(obj, data);

    TEST_ASSERT_TRUE_MESSAGE(data.enabled, "enabled should be true");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, data.sensorCount, "sensors array should populate whitelist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("AA:BB:CC:DD:EE:FF", data.sensors[0].mac, "sensor MAC should be persisted");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("Living Room", data.sensors[0].alias, "sensor alias should be persisted");
}

void test_ble_migration_removed_fields_ignored() {
    RTC::BleData data;
    resetBleData(data);
    strlcpy(data.sensors[0].mac, "11:22:33:44:55:66", sizeof(data.sensors[0].mac));
    strlcpy(data.sensors[0].alias, "Desk", sizeof(data.sensors[0].alias));
    data.sensorCount = 1;

    JsonDocument doc;
    doc["advertising_enabled"] = true;
    doc["scanner_enabled"] = true;
    doc["use_static_passkey"] = true;
    doc["static_passkey"] = 999999;
    doc["advertisingEnabled"] = true; // Legacy
    doc["scannerEnabled"] = true;     // Legacy
    doc["useStaticPasskey"] = true;   // Legacy
    doc["staticPasskey"] = 123456;    // Legacy

    JsonObject obj = doc.as<JsonObject>();
    CONFIG::JSON::deserializeBle(obj, data);

    TEST_ASSERT_FALSE_MESSAGE(data.enabled, "removed fields must not flip enabled state");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, data.sensorCount, "removed fields must not rewrite whitelist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("11:22:33:44:55:66", data.sensors[0].mac, "existing whitelist entry should stay intact");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("Desk", data.sensors[0].alias, "existing alias should stay intact");
}

// ============================================================================
// Shelly Tests
// ============================================================================
void test_shelly_migration_device_relay() {
    RTC::ShellyData data;
    resetShellyData(data);

    // JSON for a single device with NEW relay_index
    JsonDocument doc;
    JsonArray devices = doc["devices"].to<JsonArray>();
    JsonObject dev1 = devices.add<JsonObject>();
    dev1["id"] = "shelly1";
    dev1["ip"] = "192.168.1.100";
    dev1["relay_index"] = 2; // NEW KEY
    dev1["generation"] = 1;
    dev1["enabled"] = true;

    JsonObject obj = doc.as<JsonObject>();
    CONFIG::JSON::deserializeShelly(obj, data);

    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, data.deviceCount, "Should parse 1 device");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(2, data.devices[0].relayIndex, "relay_index should be mapped to relayIndex");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, data.devices[0].generation, "generation should be persisted");
}

void test_shelly_migration_legacy_ignored() {
    RTC::ShellyData data;
    resetShellyData(data);

    // JSON with OLD key
    JsonDocument doc;
    JsonArray devices = doc["devices"].to<JsonArray>();
    JsonObject dev1 = devices.add<JsonObject>();
    dev1["id"] = "shelly1";
    dev1["ip"] = "192.168.1.100";
    dev1["relayIndex"] = 2; // OLD KEY (Should be ignored -> default to 0)
    dev1["enabled"] = true;

    JsonObject obj = doc.as<JsonObject>();
    CONFIG::JSON::deserializeShelly(obj, data);

    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, data.deviceCount, "Should parse 1 device");
    // Default is 0, so if we pass 2 and it ignores it, it should satisfy "!= 2" or "== 0".
    // Wait, the deserializer might default to 0. 
    // Let's verify what the default is in valid parsing.
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, data.devices[0].relayIndex, "Legacy relayIndex should be ignored (default 0)");
}


// ============================================================================
// UDP Pusher Tests
// ============================================================================
void test_udp_pusher_migration() {
    RTC::UdpPusherData data;
    resetUdpData(data);

    // JSON where udp_pusher object contains snake_case keys (though udp_pusher struct itself mostly uses standard keys)
    // The key "udp_pusher" itself is the main change in App.h (kUdpPusher), 
    // but the struct internal keys were mostly simple ("enabled", "port", "interval_ms", "host").
    // Let's check SystemConfigJson.cpp for what keys it uses inside deserializeUdpPusher.
    // keys: kEnabled, kPort, kIntervalMs, kFormat, kHost.
    // kIntervalMs -> "interval_ms" (changed in migration)
    
    JsonDocument doc;
    doc["enabled"] = true;
    doc["port"] = 9999;
    doc["interval_ms"] = 5000; // NEW KEY (was pollIntervalMs or similar?)
    doc["host"] = "192.168.1.5";

    JsonObject obj = doc.as<JsonObject>();
    CONFIG::JSON::deserializeUdpPusher(obj, data);

    TEST_ASSERT_TRUE(data.enabled);
    TEST_ASSERT_EQUAL_UINT16(9999, data.port);
    TEST_ASSERT_EQUAL_UINT32(5000, data.intervalMs);
    TEST_ASSERT_EQUAL_STRING("192.168.1.5", data.host);
}


// Unity hooks
void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_ble_migration_scanner_only_contract);
    RUN_TEST(test_ble_migration_removed_fields_ignored);
    
    RUN_TEST(test_shelly_migration_device_relay);
    RUN_TEST(test_shelly_migration_legacy_ignored);
    
    RUN_TEST(test_udp_pusher_migration);
    
    return UNITY_END();
}
