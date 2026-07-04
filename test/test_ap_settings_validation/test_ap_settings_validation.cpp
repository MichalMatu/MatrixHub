#include <unity.h>
#include <ArduinoJson.h>

#include "../../lib/framework/utils/SettingValue.cpp"
#include "../../lib/framework/wifi/APSettingsService.h"

void setUp(void) {}
void tearDown(void) {}

void test_validate_rejects_invalid_http_payload() {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["ssid"] = "ap";
    root["password"] = "1234";
    root["channel"] = 99;
    root["max_clients"] = 0;
    root["local_ip"] = "0.0.0.0";
    root["gateway_ip"] = "192.168.4.1";
    root["subnet_mask"] = "255.255.255.0";

    const StateHandlerResult result = APSettings::validate(nullptr, root);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL(400, result.httpStatus);
    TEST_ASSERT_EQUAL_STRING("input/ap_ssid_invalid", result.errorCode);
}

void test_update_sanitizes_invalid_persisted_fields_to_factory_defaults() {
    APSettings settings;

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["ssid"] = "ap";
    root["password"] = "short";
    root["channel"] = 0;
    root["max_clients"] = 99;
    root["local_ip"] = "0.0.0.0";
    root["gateway_ip"] = "0.0.0.0";
    root["subnet_mask"] = "0.0.0.0";

    const StateUpdateResult result =
        APSettings::update(root, settings, "/config/apSettings.json");

    TEST_ASSERT_EQUAL(static_cast<int>(StateUpdateResult::CHANGED), static_cast<int>(result));
    TEST_ASSERT_EQUAL_STRING("matrixhub.local", settings.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING(FACTORY_AP_PASSWORD, settings.password.c_str());
    TEST_ASSERT_EQUAL(FACTORY_AP_CHANNEL, settings.channel);
    TEST_ASSERT_EQUAL(FACTORY_AP_MAX_CLIENTS, settings.maxClients);
    TEST_ASSERT_TRUE(settings.localIP == IPAddress(192, 168, 4, 1));
    TEST_ASSERT_TRUE(settings.gatewayIP == IPAddress(192, 168, 4, 1));
    TEST_ASSERT_TRUE(settings.subnetMask == IPAddress(255, 255, 255, 0));
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_validate_rejects_invalid_http_payload);
    RUN_TEST(test_update_sanitizes_invalid_persisted_fields_to_factory_defaults);
    return UNITY_END();
}
