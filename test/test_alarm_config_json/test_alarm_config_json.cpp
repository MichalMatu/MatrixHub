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
    doc["ble_device_mac"] = "A4:C1:38:12:34:56";

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
    doc["ble_device_mac"] = "A4:C1:38:12:34:56";

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
    doc["ble_device_mac"] = "A4:C1:38:12:34:56";

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

void test_wifi_csi_motion_rule_is_canonicalized_from_inverse_operator() {
    JsonDocument doc;
    doc["id"] = "csi-motion";
    doc["name"] = "CSI motion";
    doc["source"] = "wifi_csi_motion";
    doc["operator"] = "below";
    doc["threshold"] = 0.9f;

    JsonObject obj = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;

    TEST_ASSERT_TRUE(CONFIG::JSON::deserializeAlarmRule(obj, rule));
    TEST_ASSERT_EQUAL(ALARMS::AlarmOperator::Above, rule.op);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, rule.threshold);
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
    doc["gpio_id"] = "gpio38";

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

void test_deserialize_rule_rejects_lossy_shelly_bindings() {
    JsonDocument doc;
    doc["id"] = "shelly-rule";
    doc["name"] = "Shelly rule";
    JsonArray devices = doc["shelly_device_ids"].to<JsonArray>();
    devices.add("relay-1");
    devices.add("relay-1");

    ALARMS::AlarmRule rule;
    JsonObject object = doc.as<JsonObject>();
    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRule(object, rule));

    devices.clear();
    for (uint8_t i = 0; i < ALARMS::kMaxShellyPerRule + 1; ++i) {
        char id[16];
        snprintf(id, sizeof(id), "relay-%u", i);
        devices.add(id);
    }
    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRule(object, rule));

    devices.clear();
    devices.add("");
    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRule(object, rule));

    devices.clear();
    devices.add("12345678901234567890123456789012");
    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRule(object, rule));
}

void test_deserialize_rule_rejects_wrong_shelly_binding_type() {
    JsonDocument doc;
    doc["id"] = "shelly-rule";
    doc["name"] = "Shelly rule";
    doc["shelly_device_ids"] = "relay-1";

    ALARMS::AlarmRule rule;
    JsonObject object = doc.as<JsonObject>();
    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRule(object, rule));
}

void test_load_alarms_preserves_full_retained_runtime_for_identity_mapping() {
    RTC::mockStore = RTC::ConfigStore{};
    ALARMS::RULES_CONFIG::mockRules = ALARMS::AlarmRulesSnapshot{};
    RTC::mockStore.alarms.ruleCount = 2;
    RTC::mockStore.alarms.enabledCount = 2;
    RTC::mockStore.alarms.ruleRuntimeIdentityHashes[0] = 0x1111ULL;
    RTC::mockStore.alarms.ruleRuntimeIdentityHashes[1] = 0x2222ULL;
    RTC::mockStore.alarms.runtimeStates[1].previouslyTriggered = true;
    RTC::mockStore.alarms.runtimeStates[1].initialized = true;
    RTC::mockStore.alarms.runtimeStates[1].lastTriggeredMs = 987;

    JsonDocument doc;
    JsonArray rules = doc["rules"].to<JsonArray>();
    JsonObject retainedRule = rules.add<JsonObject>();
    retainedRule["id"] = "retained-second";
    retainedRule["name"] = "Retained second";
    retainedRule["enabled"] = true;
    retainedRule["source"] = "wifi_csi_motion";

    JsonObject object = doc.as<JsonObject>();
    TEST_ASSERT_TRUE(CONFIG::JSON::loadAlarms(object));

    TEST_ASSERT_EQUAL_UINT8(1, ALARMS::RULES_CONFIG::mockRules.ruleCount);
    TEST_ASSERT_EQUAL_STRING(
        "retained-second", ALARMS::RULES_CONFIG::mockRules.rules[0].id);
    TEST_ASSERT_EQUAL_UINT8(2, RTC::mockStore.alarms.ruleCount);
    TEST_ASSERT_EQUAL_UINT8(2, RTC::mockStore.alarms.enabledCount);
    TEST_ASSERT_EQUAL_UINT64(
        0x1111ULL, RTC::mockStore.alarms.ruleRuntimeIdentityHashes[0]);
    TEST_ASSERT_EQUAL_UINT64(
        0x2222ULL, RTC::mockStore.alarms.ruleRuntimeIdentityHashes[1]);
    TEST_ASSERT_TRUE(
        RTC::mockStore.alarms.runtimeStates[1].previouslyTriggered);
    TEST_ASSERT_TRUE(RTC::mockStore.alarms.runtimeStates[1].initialized);
    TEST_ASSERT_EQUAL_UINT32(
        987, RTC::mockStore.alarms.runtimeStates[1].lastTriggeredMs);
}

void test_deserialize_rule_rejects_unknown_channel_bits_and_wrong_types() {
    JsonDocument doc;
    doc["id"] = "strict-rule";
    doc["name"] = "Strict rule";
    doc["notify_channels"] = 256;
    JsonObject object = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;
    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRule(object, rule));

    doc["notify_channels"] = 257;
    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRule(object, rule));

    doc["notify_channels"] = 1;
    doc["enabled"] = "true";
    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRule(object, rule));

    doc.remove("enabled");
    doc["cooldown_seconds"] = "60";
    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRule(object, rule));

    doc.remove("cooldown_seconds");
    doc["created_at"] = -1;
    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRule(object, rule));
}

void test_deserialize_rule_validates_cooldown_without_integer_wrap() {
    JsonDocument doc;
    doc["id"] = "cooldown-rule";
    doc["name"] = "Cooldown rule";
    JsonObject object = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;

    doc["cooldown_seconds"] = 9;
    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRule(object, rule));
    doc["cooldown_seconds"] = 10;
    TEST_ASSERT_TRUE(CONFIG::JSON::deserializeAlarmRule(object, rule));
    TEST_ASSERT_EQUAL_UINT32(10, rule.cooldownSeconds);
    doc["cooldown_seconds"] = 65535;
    TEST_ASSERT_TRUE(CONFIG::JSON::deserializeAlarmRule(object, rule));
    TEST_ASSERT_EQUAL_UINT32(65535, rule.cooldownSeconds);
    doc["cooldown_seconds"] = 65536;
    TEST_ASSERT_TRUE(CONFIG::JSON::deserializeAlarmRule(object, rule));
    TEST_ASSERT_EQUAL_UINT32(65536, rule.cooldownSeconds);
    doc["cooldown_seconds"] = 86400;
    TEST_ASSERT_TRUE(CONFIG::JSON::deserializeAlarmRule(object, rule));
    TEST_ASSERT_EQUAL_UINT32(86400, rule.cooldownSeconds);
    doc["cooldown_seconds"] = 86401;
    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRule(object, rule));
    doc["cooldown_seconds"] = -1;
    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRule(object, rule));
}

void test_deserialize_rule_requires_valid_source_selector() {
    JsonDocument doc;
    doc["id"] = "selector-rule";
    doc["name"] = "Selector rule";
    doc["source"] = "ble_temperature";
    JsonObject object = doc.as<JsonObject>();
    ALARMS::AlarmRule rule;

    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRule(object, rule));
    doc["ble_device_mac"] = "not-a-mac";
    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRule(object, rule));
    doc["ble_device_mac"] = "A4:C1:38:12:34:56";
    TEST_ASSERT_TRUE(CONFIG::JSON::deserializeAlarmRule(object, rule));

    doc["source"] = "gpio_digital";
    doc.remove("ble_device_mac");
    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeAlarmRule(object, rule));
    doc["gpio_id"] = "gpio38";
    TEST_ASSERT_TRUE(CONFIG::JSON::deserializeAlarmRule(object, rule));
}

void test_load_alarms_rejects_entire_corrupt_snapshot() {
    ALARMS::RULES_CONFIG::mockRules = ALARMS::AlarmRulesSnapshot{};
    std::strncpy(ALARMS::RULES_CONFIG::mockRules.rules[0].id,
                 "existing",
                 sizeof(ALARMS::RULES_CONFIG::mockRules.rules[0].id) - 1);
    std::strncpy(ALARMS::RULES_CONFIG::mockRules.rules[0].name,
                 "Existing",
                 sizeof(ALARMS::RULES_CONFIG::mockRules.rules[0].name) - 1);
    ALARMS::RULES_CONFIG::mockRules.ruleCount = 1;

    JsonDocument doc;
    JsonArray rules = doc["rules"].to<JsonArray>();
    addRule(rules, "valid", "Valid");
    JsonObject invalid = addRule(rules, "invalid", "Invalid");
    invalid["source"] = "gpio_digital";
    JsonObject object = doc.as<JsonObject>();

    TEST_ASSERT_FALSE(CONFIG::JSON::loadAlarms(object));
    TEST_ASSERT_EQUAL_UINT8(1, ALARMS::RULES_CONFIG::mockRules.ruleCount);
    TEST_ASSERT_EQUAL_STRING(
        "existing", ALARMS::RULES_CONFIG::mockRules.rules[0].id);
}

void test_load_alarms_rejects_wrong_rules_container_type() {
    ALARMS::RULES_CONFIG::mockRules = ALARMS::AlarmRulesSnapshot{};
    std::strncpy(ALARMS::RULES_CONFIG::mockRules.rules[0].id,
                 "existing",
                 sizeof(ALARMS::RULES_CONFIG::mockRules.rules[0].id) - 1);
    std::strncpy(ALARMS::RULES_CONFIG::mockRules.rules[0].name,
                 "Existing",
                 sizeof(ALARMS::RULES_CONFIG::mockRules.rules[0].name) - 1);
    ALARMS::RULES_CONFIG::mockRules.ruleCount = 1;

    JsonDocument doc;
    doc["rules"] = "not-an-array";
    JsonObject object = doc.as<JsonObject>();

    TEST_ASSERT_FALSE(CONFIG::JSON::loadAlarms(object));
    TEST_ASSERT_EQUAL_UINT8(1, ALARMS::RULES_CONFIG::mockRules.ruleCount);
    TEST_ASSERT_EQUAL_STRING(
        "existing", ALARMS::RULES_CONFIG::mockRules.rules[0].id);
}

void test_load_alarms_rejects_incomplete_section_without_rules_snapshot() {
    ALARMS::RULES_CONFIG::mockRules = ALARMS::AlarmRulesSnapshot{};
    std::strncpy(ALARMS::RULES_CONFIG::mockRules.rules[0].id,
                 "existing",
                 sizeof(ALARMS::RULES_CONFIG::mockRules.rules[0].id) - 1);
    std::strncpy(ALARMS::RULES_CONFIG::mockRules.rules[0].name,
                 "Existing",
                 sizeof(ALARMS::RULES_CONFIG::mockRules.rules[0].name) - 1);
    ALARMS::RULES_CONFIG::mockRules.ruleCount = 1;

    JsonDocument doc;
    JsonObject object = doc.to<JsonObject>();

    TEST_ASSERT_FALSE(CONFIG::JSON::loadAlarms(object));
    TEST_ASSERT_EQUAL_UINT8(1, ALARMS::RULES_CONFIG::mockRules.ruleCount);
    TEST_ASSERT_EQUAL_STRING(
        "existing", ALARMS::RULES_CONFIG::mockRules.rules[0].id);
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
    RUN_TEST(test_wifi_csi_motion_rule_is_canonicalized_from_inverse_operator);
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
    RUN_TEST(test_deserialize_rule_rejects_lossy_shelly_bindings);
    RUN_TEST(test_deserialize_rule_rejects_wrong_shelly_binding_type);
    RUN_TEST(test_load_alarms_preserves_full_retained_runtime_for_identity_mapping);
    RUN_TEST(test_deserialize_rule_rejects_unknown_channel_bits_and_wrong_types);
    RUN_TEST(test_deserialize_rule_validates_cooldown_without_integer_wrap);
    RUN_TEST(test_deserialize_rule_requires_valid_source_selector);
    RUN_TEST(test_load_alarms_rejects_entire_corrupt_snapshot);
    RUN_TEST(test_load_alarms_rejects_wrong_rules_container_type);
    RUN_TEST(test_load_alarms_rejects_incomplete_section_without_rules_snapshot);
    return UNITY_END();
}
