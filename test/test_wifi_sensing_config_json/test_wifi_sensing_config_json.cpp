#define NOMINMAX
#include <algorithm>
#include <unity.h>
#include <ArduinoJson.h>

// Force undef min/max macros to prevent conflicts with std::min/max
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// Include .cpp directly to avoid linker issues in native test environment
#include "../../src/config/json/WifiSensingConfigJson.cpp"
#include "../../src/system/rtc/types/RtcWifiSensingTypes.h"

// Stubs for RTC functions
namespace RTC {
    static ConfigStore mockStore;
    const ConfigStore& getConfig() { return mockStore; }
    ConfigStore& getMutableConfig() { return mockStore; }
    void withConfig(const std::function<void(const ConfigStore&)>& reader) { reader(mockStore); }
    bool updateConfig(const std::function<void(ConfigStore&)>& updater) {
        updater(mockStore);
        return true;
    }
}

void setUp(void) {
    // Reset mock store before each test
    RTC::mockStore.wifiSensing = RTC::WifiSensingData();
}

void tearDown(void) {}

void test_load_wifi_sensing_defaults() {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    // Empty object should trigger defaults
    
    CONFIG::JSON::loadWifiSensing(obj);
    
    const auto& w = RTC::mockStore.wifiSensing;
    TEST_ASSERT_FALSE(w.enabled);
    TEST_ASSERT_EQUAL_UINT32(200, w.sampleIntervalMs); // FACTORY_DEFAULT
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.0f, w.varianceThreshold); // FACTORY_DEFAULT
    TEST_ASSERT_FALSE(w.csiAlarmEnabled);
    TEST_ASSERT_EQUAL_UINT8(0, w.csiAlarmBandCount);
    TEST_ASSERT_EQUAL_UINT16(150, w.csiBaselineFrames);
    TEST_ASSERT_EQUAL_UINT8(8, w.csiTopK);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 6.0f, w.csiEnterThreshold);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.0f, w.csiClearThreshold);
}

void test_load_wifi_sensing_custom_values() {
    JsonDocument doc;
    doc["enabled"] = true;
    doc["sample_interval_ms"] = 500;
    doc["variance_threshold"] = 2.5f;
    JsonObject obj = doc.as<JsonObject>();
    
    CONFIG::JSON::loadWifiSensing(obj);
    
    const auto& w = RTC::mockStore.wifiSensing;
    TEST_ASSERT_TRUE(w.enabled);
    TEST_ASSERT_EQUAL_UINT32(500, w.sampleIntervalMs);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.5f, w.varianceThreshold);
}

void test_load_wifi_sensing_clamps_values() {
    JsonDocument doc;
    doc["sample_interval_ms"] = 10; // Too low (min 50)
    doc["variance_threshold"] = 0.05f; // Too low (min 0.1)
    JsonObject obj = doc.as<JsonObject>();
    
    CONFIG::JSON::loadWifiSensing(obj);
    
    const auto& w = RTC::mockStore.wifiSensing;
    // Check clamping logic (constants from RtcWifiSensingTypes.h / Hardware.h)
    // Assuming LIMITS::WIFI_SENSING::MIN_INTERVAL_MS = 50 (based on previous SystemConfigJson code)
    // We should probably include Hardware.h or defining constants to be sure, but let's test the behavior
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(50, w.sampleIntervalMs); 
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.1f, w.varianceThreshold);
}

void test_save_wifi_sensing() {
    RTC::mockStore.wifiSensing.enabled = true;
    RTC::mockStore.wifiSensing.sampleIntervalMs = 750;
    RTC::mockStore.wifiSensing.varianceThreshold = 3.3f;
    RTC::mockStore.wifiSensing.csiAlarmEnabled = true;
    RTC::mockStore.wifiSensing.csiAlarmBandCount = 1;
    RTC::mockStore.wifiSensing.csiAlarmBandStart[0] = 58;
    RTC::mockStore.wifiSensing.csiAlarmBandEnd[0] = 70;
    
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    
    CONFIG::JSON::saveWifiSensing(obj);
    
    TEST_ASSERT_TRUE(obj["enabled"].as<bool>());
    TEST_ASSERT_EQUAL_UINT32(750, obj["sample_interval_ms"].as<uint32_t>());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.3f, obj["variance_threshold"].as<float>());
    TEST_ASSERT_TRUE(obj["csi_alarm"]["enabled"].as<bool>());
    TEST_ASSERT_EQUAL_UINT32(1, obj["csi_alarm"]["bands"].as<JsonArray>().size());
    TEST_ASSERT_EQUAL_UINT32(58, obj["csi_alarm"]["bands"][0]["start"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(70, obj["csi_alarm"]["bands"][0]["end"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(150, obj["csi_alarm"]["baseline_frames"].as<uint32_t>());
}

void test_missing_csi_alarm_keeps_existing_values() {
    RTC::mockStore.wifiSensing.csiAlarmEnabled = true;
    RTC::mockStore.wifiSensing.csiAlarmBandCount = 1;
    RTC::mockStore.wifiSensing.csiAlarmBandStart[0] = 12;
    RTC::mockStore.wifiSensing.csiAlarmBandEnd[0] = 20;

    JsonDocument doc;
    doc["enabled"] = true;
    JsonObject obj = doc.as<JsonObject>();

    CONFIG::JSON::loadWifiSensing(obj);

    const auto& w = RTC::mockStore.wifiSensing;
    TEST_ASSERT_TRUE(w.enabled);
    TEST_ASSERT_TRUE(w.csiAlarmEnabled);
    TEST_ASSERT_EQUAL_UINT8(1, w.csiAlarmBandCount);
    TEST_ASSERT_EQUAL_UINT16(12, w.csiAlarmBandStart[0]);
    TEST_ASSERT_EQUAL_UINT16(20, w.csiAlarmBandEnd[0]);
}

void test_load_csi_alarm_max_four_bands_and_normalizes_ranges() {
    JsonDocument doc;
    JsonObject alarm = doc["csi_alarm"].to<JsonObject>();
    alarm["enabled"] = true;
    JsonArray bands = alarm["bands"].to<JsonArray>();
    for (uint8_t i = 0; i < 6; ++i) {
        JsonObject band = bands.add<JsonObject>();
        band["start"] = 20 + i;
        band["end"] = 10 + i;
    }
    JsonObject obj = doc.as<JsonObject>();

    CONFIG::JSON::loadWifiSensing(obj);

    const auto& w = RTC::mockStore.wifiSensing;
    TEST_ASSERT_TRUE(w.csiAlarmEnabled);
    TEST_ASSERT_EQUAL_UINT8(4, w.csiAlarmBandCount);
    TEST_ASSERT_EQUAL_UINT16(10, w.csiAlarmBandStart[0]);
    TEST_ASSERT_EQUAL_UINT16(20, w.csiAlarmBandEnd[0]);
    TEST_ASSERT_EQUAL_UINT16(13, w.csiAlarmBandStart[3]);
    TEST_ASSERT_EQUAL_UINT16(23, w.csiAlarmBandEnd[3]);
}

void test_load_csi_alarm_clamps_values() {
    JsonDocument doc;
    JsonObject alarm = doc["csi_alarm"].to<JsonObject>();
    alarm["enabled"] = true;
    JsonArray bands = alarm["bands"].to<JsonArray>();
    JsonObject band = bands.add<JsonObject>();
    band["start"] = 999;
    band["end"] = 260;
    alarm["baseline_frames"] = 5;
    alarm["top_k"] = 64;
    alarm["enter_threshold"] = 0.1f;
    alarm["clear_threshold"] = 200.0f;
    alarm["hold_ms"] = 10;
    alarm["clear_hold_ms"] = 99999;
    alarm["min_noise"] = 0.01f;
    alarm["min_energy"] = 20000.0f;
    alarm["noisy_threshold"] = 0.5f;
    alarm["sensitivity"] = 9;
    JsonObject obj = doc.as<JsonObject>();

    CONFIG::JSON::loadWifiSensing(obj);

    const auto& w = RTC::mockStore.wifiSensing;
    TEST_ASSERT_EQUAL_UINT8(1, w.csiAlarmBandCount);
    TEST_ASSERT_EQUAL_UINT16(255, w.csiAlarmBandStart[0]);
    TEST_ASSERT_EQUAL_UINT16(255, w.csiAlarmBandEnd[0]);
    TEST_ASSERT_EQUAL_UINT16(30, w.csiBaselineFrames);
    TEST_ASSERT_EQUAL_UINT8(32, w.csiTopK);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.5f, w.csiEnterThreshold);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.2f, w.csiClearThreshold);
    TEST_ASSERT_EQUAL_UINT16(100, w.csiHoldMs);
    TEST_ASSERT_EQUAL_UINT16(30000, w.csiClearHoldMs);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.1f, w.csiMinNoise);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10000.0f, w.csiMinEnergy);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.5f, w.csiNoisyThreshold);
    TEST_ASSERT_EQUAL_UINT8(2, w.csiSensitivity);
}

void test_csi_alarm_sensitivity_is_backend_authoritative() {
    JsonDocument doc;
    JsonObject alarm = doc["csi_alarm"].to<JsonObject>();
    alarm["sensitivity"] = 0;
    alarm["enter_threshold"] = 1.0f;
    alarm["clear_threshold"] = 0.5f;
    alarm["noisy_threshold"] = 2.0f;
    JsonObject obj = doc.as<JsonObject>();

    CONFIG::JSON::loadWifiSensing(obj);

    const auto& w = RTC::mockStore.wifiSensing;
    TEST_ASSERT_EQUAL_UINT8(0, w.csiSensitivity);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 8.0f, w.csiEnterThreshold);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.0f, w.csiClearThreshold);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 8.0f, w.csiNoisyThreshold);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_load_wifi_sensing_defaults);
    RUN_TEST(test_load_wifi_sensing_custom_values);
    RUN_TEST(test_load_wifi_sensing_clamps_values);
    RUN_TEST(test_save_wifi_sensing);
    RUN_TEST(test_missing_csi_alarm_keeps_existing_values);
    RUN_TEST(test_load_csi_alarm_max_four_bands_and_normalizes_ranges);
    RUN_TEST(test_load_csi_alarm_clamps_values);
    RUN_TEST(test_csi_alarm_sensitivity_is_backend_authoritative);
    return UNITY_END();
}
