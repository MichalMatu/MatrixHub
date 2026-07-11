#include <unity.h>

#include <string>

#include <Arduino.h>

std::string g_responseBuffer;

extern "C" {
#include "esp_http_server.h"

esp_err_t httpd_resp_set_type(httpd_req_t* r, const char* type) {
    (void)r;
    (void)type;
    return ESP_OK;
}

esp_err_t httpd_resp_set_hdr(httpd_req_t* r, const char* field, const char* value) {
    (void)r;
    (void)field;
    (void)value;
    return ESP_OK;
}

esp_err_t httpd_resp_send_chunk(httpd_req_t* r, const char* buf, ssize_t len) {
    (void)r;
    if (!buf || len == 0) {
        return ESP_OK;
    }
    g_responseBuffer.append(buf, static_cast<size_t>(len));
    return ESP_OK;
}
}

#include "../../src/system/utils/json/JsonResponseWriter.cpp"
#include "../../src/api/alarms/utils/AlarmRulesSerializer.cpp"

void setUp(void) {
    g_responseBuffer.clear();
    TEST_ASSERT_TRUE(Utils::JsonResponseWriter::begin());
}

void tearDown(void) {}

static ALARMS::AlarmRule makeRule(const char* id, const char* name) {
    ALARMS::AlarmRule rule;
    strlcpy(rule.id, id, sizeof(rule.id));
    strlcpy(rule.name, name, sizeof(rule.name));
    rule.enabled = true;
    rule.source = ALARMS::AlarmSource::Temperature;
    rule.op = ALARMS::AlarmOperator::Above;
    rule.threshold = 25.0f;
    rule.severity = ALARMS::AlarmSeverity::Warning;
    rule.cooldownSeconds = 60;
    rule.createdAt = 10;
    rule.updatedAt = 20;
    rule.notifyChannels = ALARMS::NotifyChannel::Led;
    return rule;
}

void test_serializer_uses_parallel_status_index() {
    ALARMS::AlarmRule rules[] = {
        makeRule("first", "First rule"),
        makeRule("second", "Second rule"),
    };
    API::RuleStatus statuses[2]{};
    statuses[0].valid = true;
    statuses[0].triggered = false;
    statuses[0].lastTriggered = 11;
    statuses[0].currentValue = 21.0f;
    statuses[0].transitionSeq = 5;
    statuses[0].deviceMillis = 101;
    statuses[0].bootId = 0x0000000000000017ULL;
    statuses[1].valid = true;
    statuses[1].triggered = true;
    statuses[1].lastTriggered = 22;
    statuses[1].currentValue = 32.5f;
    statuses[1].transitionSeq = 6;
    statuses[1].deviceMillis = 202;
    statuses[1].bootId = 0x0123456789ABCDEFULL;

    httpd_req_t req{};
    req.uri = "/api/alarms/rules";
    Utils::JsonResponseWriter writer(&req);
    TEST_ASSERT_TRUE(writer.beginResponse());
    TEST_ASSERT_TRUE(API::AlarmRulesSerializer::serialize(writer, rules, 2, statuses, true));
    TEST_ASSERT_TRUE(writer.finish());

    const size_t firstRule = g_responseBuffer.find("\"id\":\"first\"");
    const size_t secondRule = g_responseBuffer.find("\"id\":\"second\"");
    TEST_ASSERT_NOT_EQUAL(std::string::npos, firstRule);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, secondRule);
    TEST_ASSERT_TRUE(firstRule < secondRule);

    const size_t firstTriggered = g_responseBuffer.find("\"triggered\":false", firstRule);
    const size_t secondTriggered = g_responseBuffer.find("\"triggered\":true", secondRule);
    const size_t secondValue = g_responseBuffer.find("\"current_value\":32.50", secondRule);
    const size_t firstTransitionSeq =
        g_responseBuffer.find("\"transition_seq\":5", firstRule);
    const size_t secondDeviceMillis =
        g_responseBuffer.find("\"device_millis\":202", secondRule);
    const size_t secondBootId =
        g_responseBuffer.find("\"boot_id\":\"0123456789abcdef\"", secondRule);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, firstTriggered);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, secondTriggered);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, secondValue);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, firstTransitionSeq);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, secondDeviceMillis);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, secondBootId);
    TEST_ASSERT_TRUE(firstTriggered < secondRule);
    TEST_ASSERT_TRUE(secondTriggered > secondRule);
}

void test_serializer_defaults_missing_parallel_status() {
    ALARMS::AlarmRule rules[] = {
        makeRule("first", "First rule"),
    };
    API::RuleStatus statuses[1]{};

    httpd_req_t req{};
    req.uri = "/api/alarms/rules";
    Utils::JsonResponseWriter writer(&req);
    TEST_ASSERT_TRUE(writer.beginResponse());
    TEST_ASSERT_TRUE(API::AlarmRulesSerializer::serialize(writer, rules, 1, statuses, true));
    TEST_ASSERT_TRUE(writer.finish());

    TEST_ASSERT_NOT_EQUAL(
        std::string::npos,
        g_responseBuffer.find("\"triggered\":false"));
    TEST_ASSERT_NOT_EQUAL(
        std::string::npos,
        g_responseBuffer.find("\"last_triggered\":0"));
    TEST_ASSERT_NOT_EQUAL(
        std::string::npos,
        g_responseBuffer.find("\"transition_seq\":0"));
    TEST_ASSERT_NOT_EQUAL(
        std::string::npos,
        g_responseBuffer.find("\"device_millis\":0"));
    TEST_ASSERT_NOT_EQUAL(
        std::string::npos,
        g_responseBuffer.find("\"boot_id\":\"0000000000000000\""));
    TEST_ASSERT_EQUAL(std::string::npos, g_responseBuffer.find("\"current_value\""));
}

void test_serializer_includes_ble_mac_for_battery_source() {
    ALARMS::AlarmRule rules[] = {
        makeRule("ble-batt", "BLE battery"),
    };
    rules[0].source = ALARMS::AlarmSource::BleBattery;
    strlcpy(rules[0].bleDeviceMac, "aa:bb:cc:dd:ee:ff", sizeof(rules[0].bleDeviceMac));

    httpd_req_t req{};
    req.uri = "/api/alarms/rules";
    Utils::JsonResponseWriter writer(&req);
    TEST_ASSERT_TRUE(writer.beginResponse());
    TEST_ASSERT_TRUE(API::AlarmRulesSerializer::serialize(writer, rules, 1, nullptr, false));
    TEST_ASSERT_TRUE(writer.finish());

    TEST_ASSERT_NOT_EQUAL(std::string::npos, g_responseBuffer.find("\"source\":\"ble_battery\""));
    TEST_ASSERT_NOT_EQUAL(
        std::string::npos,
        g_responseBuffer.find("\"ble_device_mac\":\"aa:bb:cc:dd:ee:ff\""));
    TEST_ASSERT_EQUAL(
        std::string::npos,
        g_responseBuffer.find("\"transition_seq\""));
    TEST_ASSERT_EQUAL(
        std::string::npos,
        g_responseBuffer.find("\"device_millis\""));
    TEST_ASSERT_EQUAL(
        std::string::npos,
        g_responseBuffer.find("\"boot_id\""));
}

void test_serializer_includes_gpio_id_for_gpio_source() {
    ALARMS::AlarmRule rules[] = {
        makeRule("gpio-rule", "GPIO rule"),
    };
    rules[0].source = ALARMS::AlarmSource::GpioDigital;
    strlcpy(rules[0].gpioId, "gpio38", sizeof(rules[0].gpioId));

    httpd_req_t req{};
    req.uri = "/api/alarms/rules";
    Utils::JsonResponseWriter writer(&req);
    TEST_ASSERT_TRUE(writer.beginResponse());
    TEST_ASSERT_TRUE(API::AlarmRulesSerializer::serialize(writer, rules, 1, nullptr, false));
    TEST_ASSERT_TRUE(writer.finish());

    TEST_ASSERT_NOT_EQUAL(std::string::npos, g_responseBuffer.find("\"source\":\"gpio_digital\""));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, g_responseBuffer.find("\"gpio_id\":\"gpio38\""));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_serializer_uses_parallel_status_index);
    RUN_TEST(test_serializer_defaults_missing_parallel_status);
    RUN_TEST(test_serializer_includes_ble_mac_for_battery_source);
    RUN_TEST(test_serializer_includes_gpio_id_for_gpio_source);
    return UNITY_END();
}
