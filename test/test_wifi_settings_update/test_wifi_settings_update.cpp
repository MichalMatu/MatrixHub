#include <unity.h>
#include <ArduinoJson.h>
#include <esp_log.h>

#include "../../lib/framework/utils/SettingValue.cpp"
#include "../../lib/framework/wifi/WiFiSettingsService.h"

void setUp(void) {}
void tearDown(void) {}

void test_update_accepts_32_byte_ssid_and_preserves_static_ip_config() {
    WiFiSettings settings;

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["hostname"] = "matrixhub";
    root["mode"] = "sta";

    JsonObject wifi = root["wifi_networks"].to<JsonArray>().add<JsonObject>();
    wifi["ssid"] = "12345678901234567890123456789012";
    wifi["password"] = "secret123";
    wifi["static_ip_config"] = true;
    wifi["local_ip"] = "192.168.1.50";
    wifi["gateway_ip"] = "192.168.1.1";
    wifi["subnet_mask"] = "255.255.255.0";
    wifi["dns_ip_1"] = "8.8.8.8";

    const StateUpdateResult result = WiFiSettings::update(root, settings, "http");

    TEST_ASSERT_EQUAL(static_cast<int>(StateUpdateResult::CHANGED), static_cast<int>(result));
    TEST_ASSERT_EQUAL_STRING("matrixhub", settings.hostname.c_str());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WiFiOperatingMode::Station),
                            static_cast<uint8_t>(settings.mode));
    TEST_ASSERT_EQUAL(1, settings.wifiSettings.size());

    const wifi_settings_t& network = settings.wifiSettings.front();
    TEST_ASSERT_EQUAL_STRING("12345678901234567890123456789012", network.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("secret123", network.password.c_str());
    TEST_ASSERT_TRUE(network.staticIPConfig);
    TEST_ASSERT_TRUE(network.localIP == IPAddress(192, 168, 1, 50));
    TEST_ASSERT_TRUE(network.gatewayIP == IPAddress(192, 168, 1, 1));
    TEST_ASSERT_TRUE(network.subnetMask == IPAddress(255, 255, 255, 0));
    TEST_ASSERT_TRUE(network.dnsIP1 == IPAddress(8, 8, 8, 8));
    TEST_ASSERT_TRUE(network.dnsIP2 == IPAddress(INADDR_NONE));
}

void test_update_rejects_33_byte_ssid() {
    WiFiSettings settings;

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["hostname"] = "matrixhub";
    root["mode"] = "ap";

    JsonObject wifi = root["wifi_networks"].to<JsonArray>().add<JsonObject>();
    wifi["ssid"] = "123456789012345678901234567890123";
    wifi["password"] = "secret123";

    const StateUpdateResult result = WiFiSettings::update(root, settings, "http");

    TEST_ASSERT_EQUAL(static_cast<int>(StateUpdateResult::CHANGED), static_cast<int>(result));
    TEST_ASSERT_EQUAL_STRING("matrixhub", settings.hostname.c_str());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WiFiOperatingMode::AccessPoint),
                            static_cast<uint8_t>(settings.mode));
    TEST_ASSERT_EQUAL(0, settings.wifiSettings.size());
}

void test_update_rejects_sta_without_valid_networks() {
    WiFiSettings settings;

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["hostname"] = "matrixhub";
    root["mode"] = "sta";
    root["wifi_networks"].to<JsonArray>();

    const StateUpdateResult result = WiFiSettings::update(root, settings, HTTP_ENDPOINT_ORIGIN_ID);

    TEST_ASSERT_EQUAL(static_cast<int>(StateUpdateResult::ERROR), static_cast<int>(result));
}

void test_update_uses_matrixhub_when_hostname_is_missing() {
    WiFiSettings settings;

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["mode"] = "ap";
    root["wifi_networks"].to<JsonArray>();

    const StateUpdateResult result = WiFiSettings::update(root, settings, "fs");

    TEST_ASSERT_EQUAL(static_cast<int>(StateUpdateResult::CHANGED), static_cast<int>(result));
    TEST_ASSERT_EQUAL_STRING("matrixhub", settings.hostname.c_str());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WiFiOperatingMode::AccessPoint),
                            static_cast<uint8_t>(settings.mode));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_update_accepts_32_byte_ssid_and_preserves_static_ip_config);
    RUN_TEST(test_update_rejects_33_byte_ssid);
    RUN_TEST(test_update_rejects_sta_without_valid_networks);
    RUN_TEST(test_update_uses_matrixhub_when_hostname_is_missing);
    return UNITY_END();
}
