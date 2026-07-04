/**
 * @file test_alarm_rule_parser.cpp
 * @brief Unit tests for Alarm rule deserialization (JSON -> AlarmRule)
 */

#include <unity.h>
#include <ArduinoJson.h>

#include "../../src/config/App.h"

// Avoid Arduino-style min/max macros breaking std::numeric_limits::max()
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "../../src/config/json/AlarmConfigJson.h"
#include "../../src/config/json/AlarmConfigJson.cpp" // Include impl for native linking

// RTC stubs for native build
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

using namespace CONFIG::JSON;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Validation: required fields
// ============================================================================

void test_deserialize_missing_id_fails() {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    obj[CONFIG::Keys::kName] = "Test Alarm";

    ALARMS::AlarmRule rule;
    TEST_ASSERT_FALSE(deserializeAlarmRule(obj, rule));
}

void test_deserialize_missing_name_fails() {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    obj[CONFIG::Keys::kId] = "alarm-001";

    ALARMS::AlarmRule rule;
    TEST_ASSERT_FALSE(deserializeAlarmRule(obj, rule));
}

void test_deserialize_empty_id_fails() {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    obj[CONFIG::Keys::kId] = "";
    obj[CONFIG::Keys::kName] = "Test Alarm";

    ALARMS::AlarmRule rule;
    TEST_ASSERT_FALSE(deserializeAlarmRule(obj, rule));
}

void test_deserialize_empty_name_fails() {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    obj[CONFIG::Keys::kId] = "alarm-001";
    obj[CONFIG::Keys::kName] = "";

    ALARMS::AlarmRule rule;
    TEST_ASSERT_FALSE(deserializeAlarmRule(obj, rule));
}

// ============================================================================
// Validation: length limits
// ============================================================================

void test_deserialize_id_too_long_fails() {
    char longId[ALARMS::kMaxIdLen + 4];
    memset(longId, 'x', sizeof(longId));
    longId[sizeof(longId) - 1] = '\0';

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    obj[CONFIG::Keys::kId] = longId;
    obj[CONFIG::Keys::kName] = "Test Alarm";

    ALARMS::AlarmRule rule;
    TEST_ASSERT_FALSE(deserializeAlarmRule(obj, rule));
}

void test_deserialize_name_too_long_fails() {
    char longName[ALARMS::kMaxAlarmNameLen + 4];
    memset(longName, 'y', sizeof(longName));
    longName[sizeof(longName) - 1] = '\0';

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    obj[CONFIG::Keys::kId] = "alarm-001";
    obj[CONFIG::Keys::kName] = longName;

    ALARMS::AlarmRule rule;
    TEST_ASSERT_FALSE(deserializeAlarmRule(obj, rule));
}

void test_deserialize_valid_id_and_name_ok() {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    obj[CONFIG::Keys::kId] = "alarm-001";
    obj[CONFIG::Keys::kName] = "Test Alarm";
    obj[CONFIG::Keys::kSource] = "temperature";
    obj[CONFIG::Keys::kOperator] = "above";
    obj[CONFIG::Keys::kThreshold] = 25.0f;
    obj[CONFIG::Keys::kSeverity] = "warning";

    ALARMS::AlarmRule rule;
    TEST_ASSERT_TRUE(deserializeAlarmRule(obj, rule));
    TEST_ASSERT_EQUAL_STRING("alarm-001", rule.id);
    TEST_ASSERT_EQUAL_STRING("Test Alarm", rule.name);
}

void test_deserialize_invalid_source_int_fails() {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    obj[CONFIG::Keys::kId] = "alarm-001";
    obj[CONFIG::Keys::kName] = "Test Alarm";
    obj[CONFIG::Keys::kSource] = 99;

    ALARMS::AlarmRule rule;
    TEST_ASSERT_FALSE(deserializeAlarmRule(obj, rule));
}

void test_deserialize_invalid_severity_string_fails() {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    obj[CONFIG::Keys::kId] = "alarm-001";
    obj[CONFIG::Keys::kName] = "Test Alarm";
    obj[CONFIG::Keys::kSeverity] = "panic";

    ALARMS::AlarmRule rule;
    TEST_ASSERT_FALSE(deserializeAlarmRule(obj, rule));
}

void test_deserialize_duplicate_rule_id_fails() {
    JsonDocument doc;
    JsonArray rules = doc.to<JsonArray>();

    JsonObject first = rules.add<JsonObject>();
    first[CONFIG::Keys::kId] = "alarm-001";
    first[CONFIG::Keys::kName] = "Temp";

    JsonObject second = rules.add<JsonObject>();
    second[CONFIG::Keys::kId] = "alarm-001";
    second[CONFIG::Keys::kName] = "Humidity";

    RTC::AlarmRulesData parsed{};
    AlarmRulesParseError error = AlarmRulesParseError::None;

    TEST_ASSERT_FALSE(deserializeAlarmRules(rules, parsed, &error));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(AlarmRulesParseError::DuplicateRuleId), static_cast<int>(error));
}

// ============================================================================
// Validation: timestamp overflow
// ============================================================================

void test_deserialize_timestamp_overflow_fails() {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    obj[CONFIG::Keys::kId] = "alarm-001";
    obj[CONFIG::Keys::kName] = "Test Alarm";
    obj[CONFIG::Keys::kCreatedAt] = static_cast<uint64_t>(UINT32_MAX) + 1ULL;

    ALARMS::AlarmRule rule;
    TEST_ASSERT_FALSE(deserializeAlarmRule(obj, rule));
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_deserialize_missing_id_fails);
    RUN_TEST(test_deserialize_missing_name_fails);
    RUN_TEST(test_deserialize_empty_id_fails);
    RUN_TEST(test_deserialize_empty_name_fails);
    RUN_TEST(test_deserialize_id_too_long_fails);
    RUN_TEST(test_deserialize_name_too_long_fails);
    RUN_TEST(test_deserialize_valid_id_and_name_ok);
    RUN_TEST(test_deserialize_invalid_source_int_fails);
    RUN_TEST(test_deserialize_invalid_severity_string_fails);
    RUN_TEST(test_deserialize_duplicate_rule_id_fails);
    RUN_TEST(test_deserialize_timestamp_overflow_fails);
    return UNITY_END();
}
