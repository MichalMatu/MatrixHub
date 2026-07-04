/**
 * @file test_shelly_protocol.cpp
 * @brief Unit tests for ShellyProtocol pure logic
 */

#include <unity.h>
#include <string.h>
#include <Arduino.h>
#undef max
#undef min
#include "../../src/shelly/protocol/ShellyProtocol.h"

// Provide mock for log implementation to satisfy linker
#include "../../src/system/logging/Logging.h"
namespace LOG {
    Settings Logging::_settings;
    void Logging::log(esp_log_level_t level, const char *tag, const char *fmt, ...) {}
    void Logging::logSection(const char* title) {}
}

#include "../../src/shelly/protocol/ShellyProtocol.cpp"

using namespace SHELLY;

void setUp(void) {}
void tearDown(void) {}

void test_build_relay_control_url() {
    char buffer[100];
    bool success;
    
    // Normal case - ON
    success = ShellyProtocol::buildGen1ControlUrl(buffer, sizeof(buffer), "192.168.1.50", 0, true);
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_STRING("http://192.168.1.50/relay/0?turn=on", buffer);
    
    // Normal case - OFF
    success = ShellyProtocol::buildGen1ControlUrl(buffer, sizeof(buffer), "10.0.0.1", 1, false);
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_STRING("http://10.0.0.1/relay/1?turn=off", buffer);
    
    // Truncation check
    char smallBuf[10];
    success = ShellyProtocol::buildGen1ControlUrl(smallBuf, sizeof(smallBuf), "192.168.1.1", 0, true);
    TEST_ASSERT_FALSE(success); // Should fail because "http://..." doesn't fit
}

void test_build_relay_status_url() {
    char buffer[100];
    bool success;
    
    success = ShellyProtocol::buildGen1StatusUrl(buffer, sizeof(buffer), "192.168.1.50");
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_STRING("http://192.168.1.50/status", buffer);
}

void test_parse_valid_status_json() {
    const char* jsonOn = "{\"relays\":[{\"ison\":true,\"has_timer\":false}]}";
    const char* jsonOff = "{\"relays\":[{\"ison\":false,\"has_timer\":false}]}";
    
    ShellyStatus status;
    char buf[100];
    
    // Test True
    strcpy(buf, jsonOn);
    TEST_ASSERT_TRUE(ShellyProtocol::parseGen1Status(buf, strlen(buf), 0, status));
    TEST_ASSERT_TRUE(status.isOn);
    
    // Test False
    strcpy(buf, jsonOff);
    TEST_ASSERT_TRUE(ShellyProtocol::parseGen1Status(buf, strlen(buf), 0, status));
    TEST_ASSERT_FALSE(status.isOn);
}

void test_parse_invalid_json() {
    const char* garbage = "not json";
    const char* partial = "{\"relays\":[{\"ison\":";
    const char* wrongField = "{\"state\":\"on\"}"; // Missing "relays"
    
    ShellyStatus status;
    char buf[100];
    
    strcpy(buf, garbage);
    TEST_ASSERT_FALSE(ShellyProtocol::parseGen1Status(buf, strlen(buf), 0, status));
    
    strcpy(buf, partial);
    TEST_ASSERT_FALSE(ShellyProtocol::parseGen1Status(buf, strlen(buf), 0, status));
    
    strcpy(buf, wrongField);
    TEST_ASSERT_FALSE(ShellyProtocol::parseGen1Status(buf, strlen(buf), 0, status));
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_build_relay_control_url);
    RUN_TEST(test_build_relay_status_url);
    RUN_TEST(test_parse_valid_status_json);
    RUN_TEST(test_parse_invalid_json);
    return UNITY_END();
}
