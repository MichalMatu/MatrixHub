/**
 * @file test_bthome_parser.cpp
 * @brief Unit tests for BtHomeParser
 */

#include <unity.h>
#include <vector>
#include <cstring>
#include <cstdint>

#include "../../src/ble/scanner/parsers/BtHomeParser.h"

using namespace BLE;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// BTHome v2 Parsing Tests
// ============================================================================

void test_parse_temperature_humidity_battery() {
    // BTHome v2 packet example (unencrypted)
    // Info: 0x40 (v2, no encryption, no trigger)
    // 0x02 (Temp): 25.40 -> 2540 -> 0xEC 0x09
    // 0x03 (Hum): 45.50 -> 4550 -> 0xC6 0x11
    // 0x01 (Bat): 85 -> 0x55
    uint8_t payload[] = {
        0x40, 
        0x02, 0xEC, 0x09,
        0x03, 0xC6, 0x11, 
        0x01, 0x55
    };
    
    BtHomeData data = BtHomeParser::parse(payload, sizeof(payload));
    
    TEST_ASSERT_TRUE(data.valid);
    TEST_ASSERT_FALSE(data.encrypted);
    TEST_ASSERT_TRUE(data.hasTemperature);
    TEST_ASSERT_TRUE(data.hasHumidity);
    TEST_ASSERT_TRUE(data.hasBattery);
    
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.40f, data.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 45.50f, data.humidity);
    TEST_ASSERT_EQUAL(85, data.battery);
}

void test_parse_encrypted_packet() {
    // Info: 0x41 (v2, ENCRYPTED, no trigger)
    uint8_t payload[] = { 0x41, 0x00, 0x11 };
    
    BtHomeData data = BtHomeParser::parse(payload, 3);
    
    // Parser should mark as encrypted and stop
    TEST_ASSERT_TRUE(data.encrypted);
    TEST_ASSERT_FALSE(data.valid); 
}

void test_parse_invalid_version() {
    // Info: 0x20 (v1 ? not v2 010) -> (001)
    uint8_t payload[] = { 0x20, 0x02, 0x00, 0x00 };
    
    BtHomeData data = BtHomeParser::parse(payload, 4);
    
    TEST_ASSERT_FALSE(data.valid); // Should reject non-v2
}

void test_parse_temperature_negative() {
    // -10.50 C -> -1050 -> 0xFE6 (Two's complement) -> 0xE6 0xFB
    uint8_t payload[] = {
        0x40, 
        0x02, 0xE6, 0xFB
    };
    
    BtHomeData data = BtHomeParser::parse(payload, sizeof(payload));
    
    TEST_ASSERT_TRUE(data.valid);
    TEST_ASSERT_TRUE(data.hasTemperature);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -10.50f, data.temperature);
}

void test_parse_voltage() {
    // 0x0C (Voltage): 3000 mV -> 0xB8 0x0B
    uint8_t payload[] = {
        0x40,
        0x0C, 0xB8, 0x0B
    };
    
    BtHomeData data = BtHomeParser::parse(payload, sizeof(payload));
    
    TEST_ASSERT_FALSE(data.valid); // Need T or H to be "valid" overall for our sensor use case?
                                   // Logic line 180: valid = hasTemp || hasHum
                                   
    TEST_ASSERT_TRUE(data.hasVoltage);
    TEST_ASSERT_EQUAL(3000, data.voltage_mv);
}

void test_parse_precise_temp_hum() {
    // 0x45 (Temp 0.1): 255 -> 25.5 C -> 0xFF 0x00
    // 0x46 (Hum 0.1): 500 -> 50.0 % -> 0xF4 0x01
    uint8_t payload[] = {
        0x40,
        0x45, 0xFF, 0x00,
        0x46, 0xF4, 0x01
    };
    
    BtHomeData data = BtHomeParser::parse(payload, sizeof(payload));
    
    TEST_ASSERT_TRUE(data.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 25.5f, data.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, data.humidity);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    UNITY_BEGIN();
    
    RUN_TEST(test_parse_temperature_humidity_battery);
    RUN_TEST(test_parse_encrypted_packet);
    RUN_TEST(test_parse_invalid_version);
    RUN_TEST(test_parse_temperature_negative);
    RUN_TEST(test_parse_voltage);
    RUN_TEST(test_parse_precise_temp_hum);
    
    return UNITY_END();
}
