#include <unity.h>
#include <cstdint>

#include "../../src/ble/scanner/parsers/TpParser.h"

using namespace BLE;

void setUp(void) {}
void tearDown(void) {}

void test_parse_valid_tp357() {
    uint8_t serviceData[] = {
        0x10,
        0xE8, 0x00,
        0x32,
        0x64,
        0x00
    };

    TpData result = TpParser::parse(serviceData, sizeof(serviceData));

    TEST_ASSERT_TRUE_MESSAGE(result.valid, "Failed to parse valid TP357 payload");
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 23.2f, result.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, result.humidity);
    TEST_ASSERT_EQUAL_UINT8(100, result.battery);
}

void test_parse_negative_temperature() {
    uint8_t serviceData[] = {
        0x10,
        0xC9, 0xFF,
        0x40,
        0x32,
        0x00
    };

    TpData result = TpParser::parse(serviceData, sizeof(serviceData));

    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -5.5f, result.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 64.0f, result.humidity);
    TEST_ASSERT_EQUAL_UINT8(50, result.battery);
}

void test_parse_short_payload() {
    uint8_t serviceData[] = {0x10, 0xE8};

    TpData result = TpParser::parse(serviceData, sizeof(serviceData));

    TEST_ASSERT_FALSE_MESSAGE(result.valid, "Should reject short payload");
}

void test_parse_extreme_temperatures() {
    uint8_t maxTemp[] = { 0x10, 0xE8, 0x03, 50, 100, 0x00 };
    TpData resMax = TpParser::parse(maxTemp, sizeof(maxTemp));
    TEST_ASSERT_TRUE(resMax.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 100.0f, resMax.temperature);

    uint8_t minTemp[] = { 0x10, 0x0C, 0xFE, 50, 100, 0x00 };
    TpData resMin = TpParser::parse(minTemp, sizeof(minTemp));
    TEST_ASSERT_TRUE(resMin.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -50.0f, resMin.temperature);
}

void test_parse_invalid_values_rejection() {
    uint8_t highTemp[] = { 0x10, 0xF2, 0x03, 50, 100, 0x00 };
    TpData resHigh = TpParser::parse(highTemp, sizeof(highTemp));
    TEST_ASSERT_FALSE_MESSAGE(resHigh.valid, "Should reject > 100C");

    uint8_t highHum[] = { 0x10, 0xE8, 0x00, 101, 100, 0x00 };
    TpData resHum = TpParser::parse(highHum, sizeof(highHum));
    TEST_ASSERT_FALSE_MESSAGE(resHum.valid, "Should reject humidity > 100%");
}

void test_parse_battery_limits() {
    uint8_t batZero[] = { 0x10, 0xE8, 0x00, 50, 0, 0x00 };
    TpData resZero = TpParser::parse(batZero, sizeof(batZero));
    TEST_ASSERT_TRUE(resZero.valid);
    TEST_ASSERT_EQUAL_UINT8(0, resZero.battery);

    uint8_t batFull[] = { 0x10, 0xE8, 0x00, 50, 100, 0x00 };
    TpData resFull = TpParser::parse(batFull, sizeof(batFull));
    TEST_ASSERT_TRUE(resFull.valid);
    TEST_ASSERT_EQUAL_UINT8(100, resFull.battery);
}

void test_isThermoPro_check() {
    TEST_ASSERT_TRUE(TpParser::isThermoPro("TP357"));
    TEST_ASSERT_TRUE(TpParser::isThermoPro("TP357S"));
    TEST_ASSERT_FALSE(TpParser::isThermoPro("TP358"));
    TEST_ASSERT_FALSE(TpParser::isThermoPro(nullptr));
    TEST_ASSERT_FALSE(TpParser::isThermoPro("Other"));
}


int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    UNITY_BEGIN();
    
    RUN_TEST(test_parse_valid_tp357);
    RUN_TEST(test_parse_negative_temperature);
    RUN_TEST(test_parse_short_payload);
    RUN_TEST(test_parse_extreme_temperatures);
    RUN_TEST(test_parse_invalid_values_rejection);
    RUN_TEST(test_parse_battery_limits);
    RUN_TEST(test_isThermoPro_check);
    
    return UNITY_END();
}
