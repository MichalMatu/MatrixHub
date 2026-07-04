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

// Include .cpp directly to avoid linker issues in native test environment without complex build flags
#include "../../src/config/json/AlarmConfigJson.cpp"
#include "../../src/alarms/types/AlarmRule.h"
#include "../../src/alarms/types/AlarmEnums.h"

// Stubs for RTC functions used by AlarmConfigJson but not by the functions under test
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

namespace ALARMS {
namespace RULES_CONFIG {
    static RTC::AlarmRulesData mockRules;

    bool copyTo(RTC::AlarmRulesData& out) {
        out = mockRules;
        return true;
    }
    void withRules(const std::function<void(const RTC::AlarmRulesData&)>& reader) { reader(mockRules); }
    bool update(const std::function<void(RTC::AlarmRulesData&)>& updater) {
        updater(mockRules);
        return true;
    }
}
}

void test_deserialize_source_string_wifi_motion() {
    JsonDocument doc;
    doc["id"] = "test";
    doc["name"] = "test";
    doc["source"] = "wifi_motion";
    
    JsonObject obj = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;
    
    CONFIG::JSON::deserializeAlarmRule(obj, rule);
    
    TEST_ASSERT_EQUAL(ALARMS::AlarmSource::WifiMotion, rule.source);
}

void test_deserialize_source_int_wifi_motion() {
    JsonDocument doc;
    doc["id"] = "test";
    doc["name"] = "test";
    doc["source"] = 3; // WifiMotion enum value
    
    JsonObject obj = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;
    
    CONFIG::JSON::deserializeAlarmRule(obj, rule);
    
    TEST_ASSERT_EQUAL(ALARMS::AlarmSource::WifiMotion, rule.source);
}

void test_deserialize_source_string_ble_battery() {
    JsonDocument doc;
    doc["id"] = "test";
    doc["name"] = "test";
    doc["source"] = "ble_battery";

    JsonObject obj = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;

    TEST_ASSERT_TRUE(CONFIG::JSON::deserializeAlarmRule(obj, rule));
    TEST_ASSERT_EQUAL(ALARMS::AlarmSource::BleBattery, rule.source);
}

void test_deserialize_source_string_ble_rssi() {
    JsonDocument doc;
    doc["id"] = "test";
    doc["name"] = "test";
    doc["source"] = "ble_rssi";

    JsonObject obj = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;

    TEST_ASSERT_TRUE(CONFIG::JSON::deserializeAlarmRule(obj, rule));
    TEST_ASSERT_EQUAL(ALARMS::AlarmSource::BleRssi, rule.source);
}

void test_deserialize_source_int_ble_rssi() {
    JsonDocument doc;
    doc["id"] = "test";
    doc["name"] = "test";
    doc["source"] = static_cast<int>(ALARMS::AlarmSource::BleRssi);

    JsonObject obj = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;

    TEST_ASSERT_TRUE(CONFIG::JSON::deserializeAlarmRule(obj, rule));
    TEST_ASSERT_EQUAL(ALARMS::AlarmSource::BleRssi, rule.source);
}

void test_deserialize_source_string_wifi_csi_motion() {
    JsonDocument doc;
    doc["id"] = "test";
    doc["name"] = "test";
    doc["source"] = "wifi_csi_motion";

    JsonObject obj = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;

    TEST_ASSERT_TRUE(CONFIG::JSON::deserializeAlarmRule(obj, rule));
    TEST_ASSERT_EQUAL(ALARMS::AlarmSource::WifiCsiMotion, rule.source);
}

void test_deserialize_source_int_wifi_csi_motion() {
    JsonDocument doc;
    doc["id"] = "test";
    doc["name"] = "test";
    doc["source"] = static_cast<int>(ALARMS::AlarmSource::WifiCsiMotion);

    JsonObject obj = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;

    TEST_ASSERT_TRUE(CONFIG::JSON::deserializeAlarmRule(obj, rule));
    TEST_ASSERT_EQUAL(ALARMS::AlarmSource::WifiCsiMotion, rule.source);
}

void test_deserialize_source_int_after_wifi_csi_motion_maps_to_imu_tamper() {
    JsonDocument doc;
    doc["id"] = "test";
    doc["name"] = "test";
    doc["source"] = static_cast<int>(ALARMS::AlarmSource::WifiCsiMotion) + 1;

    JsonObject obj = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;

    TEST_ASSERT_TRUE(CONFIG::JSON::deserializeAlarmRule(obj, rule));
    TEST_ASSERT_EQUAL(ALARMS::AlarmSource::ImuTamper, rule.source);
}

void test_deserialize_source_string_imu_tamper() {
    JsonDocument doc;
    doc["id"] = "test";
    doc["name"] = "test";
    doc["source"] = "imu_tamper";

    JsonObject obj = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;

    TEST_ASSERT_TRUE(CONFIG::JSON::deserializeAlarmRule(obj, rule));
    TEST_ASSERT_EQUAL(ALARMS::AlarmSource::ImuTamper, rule.source);
}

void test_deserialize_source_string_gpio_digital() {
    JsonDocument doc;
    doc["id"] = "test";
    doc["name"] = "test";
    doc["source"] = "gpio_digital";
    doc["gpio_id"] = "gpio38";

    JsonObject obj = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;

    TEST_ASSERT_TRUE(CONFIG::JSON::deserializeAlarmRule(obj, rule));
    TEST_ASSERT_EQUAL(ALARMS::AlarmSource::GpioDigital, rule.source);
    TEST_ASSERT_EQUAL_STRING("gpio38", rule.gpioId);
}

void test_deserialize_source_int_after_imu_tamper_maps_to_gpio_digital() {
    JsonDocument doc;
    doc["id"] = "test";
    doc["name"] = "test";
    doc["source"] = static_cast<int>(ALARMS::AlarmSource::ImuTamper) + 1;

    JsonObject obj = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;

    TEST_ASSERT_TRUE(CONFIG::JSON::deserializeAlarmRule(obj, rule));
    TEST_ASSERT_EQUAL(ALARMS::AlarmSource::GpioDigital, rule.source);
}

void test_deserialize_source_int_after_gpio_digital_fails() {
    JsonDocument doc;
    doc["id"] = "test";
    doc["name"] = "test";
    doc["source"] = static_cast<int>(ALARMS::AlarmSource::GpioDigital) + 1;

    JsonObject obj = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;

    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRule(obj, rule));
}

void test_deserialize_operator_string_below() {
    JsonDocument doc;
    doc["id"] = "test";
    doc["name"] = "test";
    doc["operator"] = "below";
    
    JsonObject obj = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;
    
    CONFIG::JSON::deserializeAlarmRule(obj, rule);
    
    TEST_ASSERT_EQUAL(ALARMS::AlarmOperator::Below, rule.op);
}

void test_deserialize_severity_string_critical() {
    JsonDocument doc;
    doc["id"] = "test";
    doc["name"] = "test";
    doc["severity"] = "critical";
    
    JsonObject obj = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;
    
    CONFIG::JSON::deserializeAlarmRule(obj, rule);
    
    TEST_ASSERT_EQUAL(ALARMS::AlarmSeverity::Critical, rule.severity);
}

void test_deserialize_notify_channels_unknown_string_fails() {
    JsonDocument doc;
    doc["id"] = "test";
    doc["name"] = "test";
    JsonArray channels = doc["notify_channels"].to<JsonArray>();
    channels.add("telegram");
    channels.add("unknown");

    JsonObject obj = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;

    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRule(obj, rule));
}

void test_deserialize_blank_name_fails() {
    JsonDocument doc;
    doc["id"] = "blank-name";
    doc["name"] = "   ";

    JsonObject obj = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;

    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRule(obj, rule));
}

static JsonObject addRule(JsonArray& rules, const char* id, const char* name) {
    JsonObject rule = rules.add<JsonObject>();
    rule["id"] = id;
    rule["name"] = name;
    return rule;
}

void test_deserialize_rules_rejects_duplicate_names() {
    JsonDocument doc;
    JsonArray rules = doc["rules"].to<JsonArray>();
    addRule(rules, "one", "Same");
    addRule(rules, "two", "Same");

    ALARMS::AlarmRulesSnapshot parsed{};
    CONFIG::JSON::AlarmRulesParseError error = CONFIG::JSON::AlarmRulesParseError::None;

    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRules(rules, parsed, &error));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CONFIG::JSON::AlarmRulesParseError::DuplicateRuleName),
        static_cast<uint8_t>(error));
}

void test_deserialize_rules_rejects_duplicate_names_trimmed_case_insensitive() {
    JsonDocument doc;
    JsonArray rules = doc["rules"].to<JsonArray>();
    addRule(rules, "one", " High Temp ");
    addRule(rules, "two", "high temp");

    ALARMS::AlarmRulesSnapshot parsed{};
    CONFIG::JSON::AlarmRulesParseError error = CONFIG::JSON::AlarmRulesParseError::None;

    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRules(rules, parsed, &error));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CONFIG::JSON::AlarmRulesParseError::DuplicateRuleName),
        static_cast<uint8_t>(error));
}

void test_deserialize_rules_rejects_more_than_max_rules() {
    JsonDocument doc;
    JsonArray rules = doc["rules"].to<JsonArray>();
    for (uint8_t i = 0; i < RTC::kMaxAlarmRules + 1; i++) {
        char id[16];
        char name[24];
        snprintf(id, sizeof(id), "rule-%u", i);
        snprintf(name, sizeof(name), "Rule %u", i);
        addRule(rules, id, name);
    }

    ALARMS::AlarmRulesSnapshot parsed{};
    CONFIG::JSON::AlarmRulesParseError error = CONFIG::JSON::AlarmRulesParseError::None;

    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRules(rules, parsed, &error));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CONFIG::JSON::AlarmRulesParseError::TooManyRules),
        static_cast<uint8_t>(error));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_deserialize_source_string_wifi_motion);
    RUN_TEST(test_deserialize_source_int_wifi_motion);
    RUN_TEST(test_deserialize_source_string_ble_battery);
    RUN_TEST(test_deserialize_source_string_ble_rssi);
    RUN_TEST(test_deserialize_source_int_ble_rssi);
    RUN_TEST(test_deserialize_source_string_wifi_csi_motion);
    RUN_TEST(test_deserialize_source_int_wifi_csi_motion);
    RUN_TEST(test_deserialize_source_int_after_wifi_csi_motion_maps_to_imu_tamper);
    RUN_TEST(test_deserialize_source_string_imu_tamper);
    RUN_TEST(test_deserialize_source_string_gpio_digital);
    RUN_TEST(test_deserialize_source_int_after_imu_tamper_maps_to_gpio_digital);
    RUN_TEST(test_deserialize_source_int_after_gpio_digital_fails);
    RUN_TEST(test_deserialize_operator_string_below);
    RUN_TEST(test_deserialize_severity_string_critical);
    RUN_TEST(test_deserialize_notify_channels_unknown_string_fails);
    RUN_TEST(test_deserialize_blank_name_fails);
    RUN_TEST(test_deserialize_rules_rejects_duplicate_names);
    RUN_TEST(test_deserialize_rules_rejects_duplicate_names_trimmed_case_insensitive);
    RUN_TEST(test_deserialize_rules_rejects_more_than_max_rules);
    return UNITY_END();
}
