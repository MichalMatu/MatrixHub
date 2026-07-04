/**
 * @file test_alarm_rule.cpp
 * @brief Unit tests for AlarmRule structure and helper methods
 * 
 * Tests cover:
 * - Default constructor initialization
 * - isValid() validation
 * - isBleSource() detection
 * - matchesSource() matching
 * - Shelly device management (addShellyDevice, hasShellyDevices, clearShellyDevices)
 */

#include <Arduino.h>
#include <unity.h>
#include <cstdio>  // for snprintf
#include "../../src/alarms/types/AlarmRule.h"

using namespace ALARMS;

// Test fixtures
void setUp(void) {
    // Called before each test
}

void tearDown(void) {
    // Called after each test
}

// ============================================================================
// Test: Default constructor initialization
// ============================================================================

void test_constructor_id_empty() {
    AlarmRule rule;
    TEST_ASSERT_EQUAL('\0', rule.id[0]);
}

void test_constructor_name_empty() {
    AlarmRule rule;
    TEST_ASSERT_EQUAL('\0', rule.name[0]);
}

void test_constructor_enabled_false() {
    AlarmRule rule;
    TEST_ASSERT_FALSE(rule.enabled);
}

void test_constructor_source_temperature() {
    AlarmRule rule;
    TEST_ASSERT_EQUAL(AlarmSource::Temperature, rule.source);
}

void test_constructor_operator_above() {
    AlarmRule rule;
    TEST_ASSERT_EQUAL(AlarmOperator::Above, rule.op);
}

void test_constructor_threshold_zero() {
    AlarmRule rule;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, rule.threshold);
}

void test_constructor_severity_warning() {
    AlarmRule rule;
    TEST_ASSERT_EQUAL(AlarmSeverity::Warning, rule.severity);
}

void test_constructor_channels_led() {
    AlarmRule rule;
    TEST_ASSERT_TRUE(hasChannel(rule.notifyChannels, NotifyChannel::Led));
}

void test_constructor_cooldown_300() {
    AlarmRule rule;
    TEST_ASSERT_EQUAL(300, rule.cooldownSeconds);
}

void test_constructor_timestamps_zero() {
    AlarmRule rule;
    TEST_ASSERT_EQUAL(0, rule.createdAt);
    TEST_ASSERT_EQUAL(0, rule.updatedAt);
}

void test_constructor_ble_mac_empty() {
    AlarmRule rule;
    TEST_ASSERT_EQUAL('\0', rule.bleDeviceMac[0]);
}

void test_constructor_gpio_id_empty() {
    AlarmRule rule;
    TEST_ASSERT_EQUAL('\0', rule.gpioId[0]);
}

void test_constructor_shelly_count_zero() {
    AlarmRule rule;
    TEST_ASSERT_EQUAL(0, rule.shellyDeviceCount);
}

// ============================================================================
// Test: isValid()
// ============================================================================

void test_isValid_default_false() {
    AlarmRule rule;
    TEST_ASSERT_FALSE(rule.isValid());
}

void test_isValid_with_id_only_false() {
    AlarmRule rule;
    strcpy(rule.id, "test-001");
    TEST_ASSERT_FALSE(rule.isValid());
}

void test_isValid_with_name_only_false() {
    AlarmRule rule;
    strcpy(rule.name, "Test Alarm");
    TEST_ASSERT_FALSE(rule.isValid());
}

void test_isValid_with_both_true() {
    AlarmRule rule;
    strcpy(rule.id, "test-001");
    strcpy(rule.name, "Test Alarm");
    TEST_ASSERT_TRUE(rule.isValid());
}

// ============================================================================
// Test: isBleSource()
// ============================================================================

void test_isBleSource_co2_false() {
    AlarmRule rule;
    rule.source = AlarmSource::CO2;
    TEST_ASSERT_FALSE(rule.isBleSource());
}

void test_isBleSource_temperature_false() {
    AlarmRule rule;
    rule.source = AlarmSource::Temperature;
    TEST_ASSERT_FALSE(rule.isBleSource());
}

void test_isBleSource_humidity_false() {
    AlarmRule rule;
    rule.source = AlarmSource::Humidity;
    TEST_ASSERT_FALSE(rule.isBleSource());
}

void test_isBleSource_wifi_motion_false() {
    AlarmRule rule;
    rule.source = AlarmSource::WifiMotion;
    TEST_ASSERT_FALSE(rule.isBleSource());
}

void test_isBleSource_ble_temperature_true() {
    AlarmRule rule;
    rule.source = AlarmSource::BleTemperature;
    TEST_ASSERT_TRUE(rule.isBleSource());
}

void test_isBleSource_ble_humidity_true() {
    AlarmRule rule;
    rule.source = AlarmSource::BleHumidity;
    TEST_ASSERT_TRUE(rule.isBleSource());
}

void test_isBleSource_ble_battery_true() {
    AlarmRule rule;
    rule.source = AlarmSource::BleBattery;
    TEST_ASSERT_TRUE(rule.isBleSource());
}

void test_isBleSource_ble_rssi_true() {
    AlarmRule rule;
    rule.source = AlarmSource::BleRssi;
    TEST_ASSERT_TRUE(rule.isBleSource());
}

void test_isGpioSource_gpio_digital_true() {
    AlarmRule rule;
    rule.source = AlarmSource::GpioDigital;
    TEST_ASSERT_TRUE(rule.isGpioSource());
    TEST_ASSERT_FALSE(rule.isBleSource());
}

void test_isGpioSource_temperature_false() {
    AlarmRule rule;
    rule.source = AlarmSource::Temperature;
    TEST_ASSERT_FALSE(rule.isGpioSource());
}

// ============================================================================
// Test: matchesSource()
// ============================================================================

void test_matchesSource_same_true() {
    AlarmRule rule;
    rule.source = AlarmSource::CO2;
    TEST_ASSERT_TRUE(rule.matchesSource(AlarmSource::CO2));
}

void test_matchesSource_different_false() {
    AlarmRule rule;
    rule.source = AlarmSource::CO2;
    TEST_ASSERT_FALSE(rule.matchesSource(AlarmSource::Temperature));
}

// ============================================================================
// Test: Shelly device management
// ============================================================================

void test_hasShellyDevices_default_false() {
    AlarmRule rule;
    TEST_ASSERT_FALSE(rule.hasShellyDevices());
}

void test_addShellyDevice_first() {
    AlarmRule rule;
    bool result = rule.addShellyDevice("shelly-plug-001");
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, rule.shellyDeviceCount);
    TEST_ASSERT_TRUE(rule.hasShellyDevices());
    TEST_ASSERT_EQUAL_STRING("shelly-plug-001", rule.shellyDeviceIds[0]);
}

void test_addShellyDevice_multiple() {
    AlarmRule rule;
    rule.addShellyDevice("shelly-001");
    rule.addShellyDevice("shelly-002");
    rule.addShellyDevice("shelly-003");
    
    TEST_ASSERT_EQUAL(3, rule.shellyDeviceCount);
    TEST_ASSERT_EQUAL_STRING("shelly-001", rule.shellyDeviceIds[0]);
    TEST_ASSERT_EQUAL_STRING("shelly-002", rule.shellyDeviceIds[1]);
    TEST_ASSERT_EQUAL_STRING("shelly-003", rule.shellyDeviceIds[2]);
}

void test_addShellyDevice_max_limit() {
    AlarmRule rule;
    
    // Add up to maximum
    for (uint8_t i = 0; i < kMaxShellyPerRule; i++) {
        char id[32];
        snprintf(id, sizeof(id), "shelly-%03d", i);
        TEST_ASSERT_TRUE(rule.addShellyDevice(id));
    }
    
    TEST_ASSERT_EQUAL(kMaxShellyPerRule, rule.shellyDeviceCount);
    
    // One more should fail
    TEST_ASSERT_FALSE(rule.addShellyDevice("shelly-overflow"));
    TEST_ASSERT_EQUAL(kMaxShellyPerRule, rule.shellyDeviceCount);
}

void test_addShellyDevice_null_fails() {
    AlarmRule rule;
    TEST_ASSERT_FALSE(rule.addShellyDevice(nullptr));
    TEST_ASSERT_EQUAL(0, rule.shellyDeviceCount);
}

void test_addShellyDevice_empty_fails() {
    AlarmRule rule;
    TEST_ASSERT_FALSE(rule.addShellyDevice(""));
    TEST_ASSERT_EQUAL(0, rule.shellyDeviceCount);
}

void test_clearShellyDevices() {
    AlarmRule rule;
    rule.addShellyDevice("shelly-001");
    rule.addShellyDevice("shelly-002");
    
    TEST_ASSERT_EQUAL(2, rule.shellyDeviceCount);
    
    rule.clearShellyDevices();
    
    TEST_ASSERT_EQUAL(0, rule.shellyDeviceCount);
    TEST_ASSERT_FALSE(rule.hasShellyDevices());
    TEST_ASSERT_EQUAL('\0', rule.shellyDeviceIds[0][0]);
}

// ============================================================================
// Test: Structure size (memory safety)
// ============================================================================

void test_structure_size_reasonable() {
    // AlarmRule should be a reasonable size (< 320 bytes per rule)
    // This catches accidental bloat
    TEST_ASSERT_LESS_THAN(320, sizeof(AlarmRule));
}

void test_structure_fixed_size() {
    // Structure should be fixed-size (no hidden allocations)
    // Multiple instances should be identical size
    AlarmRule rule1;
    AlarmRule rule2;
    
    // Fill rule1 with data
    strcpy(rule1.id, "test-001");
    strcpy(rule1.name, "Test Alarm");
    rule1.addShellyDevice("shelly-001");
    
    // Sizes should be identical
    TEST_ASSERT_EQUAL(sizeof(rule1), sizeof(rule2));
}

// ============================================================================
// Test Runner
// ============================================================================

void run_all_tests() {
    UNITY_BEGIN();
    
    // Default constructor tests
    RUN_TEST(test_constructor_id_empty);
    RUN_TEST(test_constructor_name_empty);
    RUN_TEST(test_constructor_enabled_false);
    RUN_TEST(test_constructor_source_temperature);
    RUN_TEST(test_constructor_operator_above);
    RUN_TEST(test_constructor_threshold_zero);
    RUN_TEST(test_constructor_severity_warning);
    RUN_TEST(test_constructor_channels_led);
    RUN_TEST(test_constructor_cooldown_300);
    RUN_TEST(test_constructor_timestamps_zero);
    RUN_TEST(test_constructor_ble_mac_empty);
    RUN_TEST(test_constructor_gpio_id_empty);
    RUN_TEST(test_constructor_shelly_count_zero);
    
    // isValid tests
    RUN_TEST(test_isValid_default_false);
    RUN_TEST(test_isValid_with_id_only_false);
    RUN_TEST(test_isValid_with_name_only_false);
    RUN_TEST(test_isValid_with_both_true);
    
    // isBleSource tests
    RUN_TEST(test_isBleSource_co2_false);
    RUN_TEST(test_isBleSource_temperature_false);
    RUN_TEST(test_isBleSource_humidity_false);
    RUN_TEST(test_isBleSource_wifi_motion_false);
    RUN_TEST(test_isBleSource_ble_temperature_true);
    RUN_TEST(test_isBleSource_ble_humidity_true);
    RUN_TEST(test_isBleSource_ble_battery_true);
    RUN_TEST(test_isBleSource_ble_rssi_true);
    RUN_TEST(test_isGpioSource_gpio_digital_true);
    RUN_TEST(test_isGpioSource_temperature_false);
    
    // matchesSource tests
    RUN_TEST(test_matchesSource_same_true);
    RUN_TEST(test_matchesSource_different_false);
    
    // Shelly device management tests
    RUN_TEST(test_hasShellyDevices_default_false);
    RUN_TEST(test_addShellyDevice_first);
    RUN_TEST(test_addShellyDevice_multiple);
    RUN_TEST(test_addShellyDevice_max_limit);
    RUN_TEST(test_addShellyDevice_null_fails);
    RUN_TEST(test_addShellyDevice_empty_fails);
    RUN_TEST(test_clearShellyDevices);
    
    // Structure size tests
    RUN_TEST(test_structure_size_reasonable);
    RUN_TEST(test_structure_fixed_size);
    
    UNITY_END();
}

#if defined(ARDUINO) || defined(ESP_PLATFORM)
// Arduino environment - use setup/loop
void setup() {
    delay(2000);  // Give time for serial monitor
    run_all_tests();
}

void loop() {
    // Empty - tests run once in setup()
}
#else
// Native environment - use main()
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    run_all_tests();
    return 0;
}
#endif
