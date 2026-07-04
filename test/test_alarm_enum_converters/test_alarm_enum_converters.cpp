/**
 * @file test_alarm_enum_converters.cpp
 * @brief Unit tests for AlarmEnumConverters (string <-> enum conversions)
 * 
 * Tests cover:
 * - AlarmSource conversions (sourceToString, stringToSource)
 * - AlarmOperator conversions (operatorToString, stringToOperator)
 * - AlarmSeverity conversions (severityToString, stringToSeverity)
 * - NotifyChannel bitmask parsing (parseNotifyChannelsFromStrings)
 */

#include <Arduino.h>
#include <unity.h>
#include "../../src/alarms/serialization/AlarmEnumConverters.h"

using namespace ALARMS;

// Test fixtures
void setUp(void) {
    // Called before each test
}

void tearDown(void) {
    // Called after each test
}

// ============================================================================
// Test: sourceToString()
// ============================================================================

void test_sourceToString_co2() {
    TEST_ASSERT_EQUAL_STRING("co2", sourceToString(AlarmSource::CO2));
}

void test_sourceToString_temperature() {
    TEST_ASSERT_EQUAL_STRING("temperature", sourceToString(AlarmSource::Temperature));
}

void test_sourceToString_humidity() {
    TEST_ASSERT_EQUAL_STRING("humidity", sourceToString(AlarmSource::Humidity));
}

void test_sourceToString_wifi_motion() {
    TEST_ASSERT_EQUAL_STRING("wifi_motion", sourceToString(AlarmSource::WifiMotion));
}

void test_sourceToString_ble_temperature() {
    TEST_ASSERT_EQUAL_STRING("ble_temperature", sourceToString(AlarmSource::BleTemperature));
}

void test_sourceToString_ble_humidity() {
    TEST_ASSERT_EQUAL_STRING("ble_humidity", sourceToString(AlarmSource::BleHumidity));
}

void test_sourceToString_ble_battery() {
    TEST_ASSERT_EQUAL_STRING("ble_battery", sourceToString(AlarmSource::BleBattery));
}

void test_sourceToString_ble_rssi() {
    TEST_ASSERT_EQUAL_STRING("ble_rssi", sourceToString(AlarmSource::BleRssi));
}

void test_sourceToString_wifi_csi_motion() {
    TEST_ASSERT_EQUAL_STRING("wifi_csi_motion", sourceToString(AlarmSource::WifiCsiMotion));
}

void test_sourceToString_imu_tamper() {
    TEST_ASSERT_EQUAL_STRING("imu_tamper", sourceToString(AlarmSource::ImuTamper));
}

void test_sourceToString_gpio_digital() {
    TEST_ASSERT_EQUAL_STRING("gpio_digital", sourceToString(AlarmSource::GpioDigital));
}

void test_sourceToString_unknown() {
    // Cast invalid value to test unknown case
    AlarmSource invalid = static_cast<AlarmSource>(255);
    TEST_ASSERT_EQUAL_STRING("unknown", sourceToString(invalid));
}

// ============================================================================
// Test: stringToSource()
// ============================================================================

void test_stringToSource_co2() {
    TEST_ASSERT_EQUAL(AlarmSource::CO2, stringToSource("co2"));
}

void test_stringToSource_temperature() {
    TEST_ASSERT_EQUAL(AlarmSource::Temperature, stringToSource("temperature"));
}

void test_stringToSource_humidity() {
    TEST_ASSERT_EQUAL(AlarmSource::Humidity, stringToSource("humidity"));
}

void test_stringToSource_wifi_motion() {
    TEST_ASSERT_EQUAL(AlarmSource::WifiMotion, stringToSource("wifi_motion"));
}

void test_stringToSource_ble_temperature() {
    TEST_ASSERT_EQUAL(AlarmSource::BleTemperature, stringToSource("ble_temperature"));
}

void test_stringToSource_ble_humidity() {
    TEST_ASSERT_EQUAL(AlarmSource::BleHumidity, stringToSource("ble_humidity"));
}

void test_stringToSource_ble_battery() {
    TEST_ASSERT_EQUAL(AlarmSource::BleBattery, stringToSource("ble_battery"));
}

void test_stringToSource_ble_rssi() {
    TEST_ASSERT_EQUAL(AlarmSource::BleRssi, stringToSource("ble_rssi"));
}

void test_stringToSource_wifi_csi_motion() {
    TEST_ASSERT_EQUAL(AlarmSource::WifiCsiMotion, stringToSource("wifi_csi_motion"));
}

void test_stringToSource_imu_tamper() {
    TEST_ASSERT_EQUAL(AlarmSource::ImuTamper, stringToSource("imu_tamper"));
}

void test_stringToSource_gpio_digital() {
    TEST_ASSERT_EQUAL(AlarmSource::GpioDigital, stringToSource("gpio_digital"));
}

void test_stringToSource_null() {
    // NULL defaults to Temperature
    TEST_ASSERT_EQUAL(AlarmSource::Temperature, stringToSource(nullptr));
}

void test_stringToSource_empty() {
    // Empty string defaults to Temperature
    TEST_ASSERT_EQUAL(AlarmSource::Temperature, stringToSource(""));
}

void test_stringToSource_invalid() {
    // Unknown string defaults to Temperature
    TEST_ASSERT_EQUAL(AlarmSource::Temperature, stringToSource("invalid_source"));
}

void test_stringToSource_case_sensitive() {
    // Should be case-sensitive (uppercase should not match)
    TEST_ASSERT_EQUAL(AlarmSource::Temperature, stringToSource("CO2"));
    TEST_ASSERT_EQUAL(AlarmSource::Temperature, stringToSource("TEMPERATURE"));
}

// ============================================================================
// Test: operatorToString()
// ============================================================================

void test_operatorToString_above() {
    TEST_ASSERT_EQUAL_STRING("above", operatorToString(AlarmOperator::Above));
}

void test_operatorToString_below() {
    TEST_ASSERT_EQUAL_STRING("below", operatorToString(AlarmOperator::Below));
}

void test_operatorToString_unknown() {
    AlarmOperator invalid = static_cast<AlarmOperator>(255);
    TEST_ASSERT_EQUAL_STRING("above", operatorToString(invalid));
}

// ============================================================================
// Test: stringToOperator()
// ============================================================================

void test_stringToOperator_above() {
    TEST_ASSERT_EQUAL(AlarmOperator::Above, stringToOperator("above"));
}

void test_stringToOperator_below() {
    TEST_ASSERT_EQUAL(AlarmOperator::Below, stringToOperator("below"));
}

void test_stringToOperator_null() {
    TEST_ASSERT_EQUAL(AlarmOperator::Above, stringToOperator(nullptr));
}

void test_stringToOperator_invalid() {
    TEST_ASSERT_EQUAL(AlarmOperator::Above, stringToOperator("invalid"));
}

// ============================================================================
// Test: severityToString()
// ============================================================================

void test_severityToString_info() {
    TEST_ASSERT_EQUAL_STRING("info", severityToString(AlarmSeverity::Info));
}

void test_severityToString_warning() {
    TEST_ASSERT_EQUAL_STRING("warning", severityToString(AlarmSeverity::Warning));
}

void test_severityToString_critical() {
    TEST_ASSERT_EQUAL_STRING("critical", severityToString(AlarmSeverity::Critical));
}

void test_severityToString_unknown() {
    AlarmSeverity invalid = static_cast<AlarmSeverity>(255);
    TEST_ASSERT_EQUAL_STRING("warning", severityToString(invalid));
}

// ============================================================================
// Test: stringToSeverity()
// ============================================================================

void test_stringToSeverity_info() {
    TEST_ASSERT_EQUAL(AlarmSeverity::Info, stringToSeverity("info"));
}

void test_stringToSeverity_warning() {
    TEST_ASSERT_EQUAL(AlarmSeverity::Warning, stringToSeverity("warning"));
}

void test_stringToSeverity_critical() {
    TEST_ASSERT_EQUAL(AlarmSeverity::Critical, stringToSeverity("critical"));
}

void test_stringToSeverity_null() {
    TEST_ASSERT_EQUAL(AlarmSeverity::Warning, stringToSeverity(nullptr));
}

void test_stringToSeverity_invalid() {
    TEST_ASSERT_EQUAL(AlarmSeverity::Warning, stringToSeverity("high"));
}

// ============================================================================
// Test: parseNotifyChannelsFromStrings()
// ============================================================================

void test_parseNotifyChannels_empty() {
    NotifyChannel result = parseNotifyChannelsFromStrings(nullptr, 0);
    TEST_ASSERT_EQUAL(NotifyChannel::None, result);
}

void test_parseNotifyChannels_telegram_only() {
    const char* channels[] = {"telegram"};
    NotifyChannel result = parseNotifyChannelsFromStrings(channels, 1);
    TEST_ASSERT_TRUE(hasChannel(result, NotifyChannel::Telegram));
    TEST_ASSERT_FALSE(hasChannel(result, NotifyChannel::Led));
    TEST_ASSERT_FALSE(hasChannel(result, NotifyChannel::Webhook));
}

void test_parseNotifyChannels_led_only() {
    const char* channels[] = {"led"};
    NotifyChannel result = parseNotifyChannelsFromStrings(channels, 1);
    TEST_ASSERT_FALSE(hasChannel(result, NotifyChannel::Telegram));
    TEST_ASSERT_TRUE(hasChannel(result, NotifyChannel::Led));
    TEST_ASSERT_FALSE(hasChannel(result, NotifyChannel::Webhook));
}

void test_parseNotifyChannels_webhook_only() {
    const char* channels[] = {"webhook"};
    NotifyChannel result = parseNotifyChannelsFromStrings(channels, 1);
    TEST_ASSERT_FALSE(hasChannel(result, NotifyChannel::Telegram));
    TEST_ASSERT_FALSE(hasChannel(result, NotifyChannel::Led));
    TEST_ASSERT_TRUE(hasChannel(result, NotifyChannel::Webhook));
}

void test_parseNotifyChannels_multiple() {
    const char* channels[] = {"telegram", "led", "webhook"};
    NotifyChannel result = parseNotifyChannelsFromStrings(channels, 3);
    TEST_ASSERT_TRUE(hasChannel(result, NotifyChannel::Telegram));
    TEST_ASSERT_TRUE(hasChannel(result, NotifyChannel::Led));
    TEST_ASSERT_TRUE(hasChannel(result, NotifyChannel::Webhook));
}

void test_parseNotifyChannels_with_invalid() {
    const char* channels[] = {"telegram", "invalid", "led"};
    NotifyChannel result = parseNotifyChannelsFromStrings(channels, 3);
    TEST_ASSERT_TRUE(hasChannel(result, NotifyChannel::Telegram));
    TEST_ASSERT_TRUE(hasChannel(result, NotifyChannel::Led));
    TEST_ASSERT_FALSE(hasChannel(result, NotifyChannel::Webhook));
}

void test_parseNotifyChannels_with_null_entry() {
    const char* channels[] = {"telegram", nullptr, "led"};
    NotifyChannel result = parseNotifyChannelsFromStrings(channels, 3);
    TEST_ASSERT_TRUE(hasChannel(result, NotifyChannel::Telegram));
    TEST_ASSERT_TRUE(hasChannel(result, NotifyChannel::Led));
}

void test_parseNotifyChannels_duplicates() {
    // Duplicates should be idempotent (result same as single)
    const char* channels[] = {"telegram", "telegram", "telegram"};
    NotifyChannel result = parseNotifyChannelsFromStrings(channels, 3);
    TEST_ASSERT_TRUE(hasChannel(result, NotifyChannel::Telegram));
    TEST_ASSERT_EQUAL(static_cast<uint8_t>(NotifyChannel::Telegram), static_cast<uint8_t>(result));
}

// ============================================================================
// Test: hasChannel() helper
// ============================================================================

void test_hasChannel_none() {
    TEST_ASSERT_FALSE(hasChannel(NotifyChannel::None, NotifyChannel::Telegram));
    TEST_ASSERT_FALSE(hasChannel(NotifyChannel::None, NotifyChannel::Led));
    TEST_ASSERT_FALSE(hasChannel(NotifyChannel::None, NotifyChannel::Webhook));
}

void test_hasChannel_combined_mask() {
    NotifyChannel mask = NotifyChannel::Telegram | NotifyChannel::Led;
    TEST_ASSERT_TRUE(hasChannel(mask, NotifyChannel::Telegram));
    TEST_ASSERT_TRUE(hasChannel(mask, NotifyChannel::Led));
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Webhook));
}

// ============================================================================
// Test: Bitmask operators
// ============================================================================

void test_bitmask_or_operator() {
    NotifyChannel a = NotifyChannel::Telegram;
    NotifyChannel b = NotifyChannel::Led;
    NotifyChannel combined = a | b;
    
    TEST_ASSERT_EQUAL(3, static_cast<uint8_t>(combined));  // 1 | 2 = 3
}

void test_bitmask_and_operator() {
    NotifyChannel combined = NotifyChannel::Telegram | NotifyChannel::Led;
    NotifyChannel masked = combined & NotifyChannel::Telegram;
    
    TEST_ASSERT_EQUAL(1, static_cast<uint8_t>(masked));  // 3 & 1 = 1
}

// ============================================================================
// Test Runner
// ============================================================================

void run_all_tests() {
    UNITY_BEGIN();
    
    // sourceToString tests
    RUN_TEST(test_sourceToString_co2);
    RUN_TEST(test_sourceToString_temperature);
    RUN_TEST(test_sourceToString_humidity);
    RUN_TEST(test_sourceToString_wifi_motion);
    RUN_TEST(test_sourceToString_ble_temperature);
    RUN_TEST(test_sourceToString_ble_humidity);
    RUN_TEST(test_sourceToString_ble_battery);
    RUN_TEST(test_sourceToString_ble_rssi);
    RUN_TEST(test_sourceToString_wifi_csi_motion);
    RUN_TEST(test_sourceToString_imu_tamper);
    RUN_TEST(test_sourceToString_gpio_digital);
    RUN_TEST(test_sourceToString_unknown);
    
    // stringToSource tests
    RUN_TEST(test_stringToSource_co2);
    RUN_TEST(test_stringToSource_temperature);
    RUN_TEST(test_stringToSource_humidity);
    RUN_TEST(test_stringToSource_wifi_motion);
    RUN_TEST(test_stringToSource_ble_temperature);
    RUN_TEST(test_stringToSource_ble_humidity);
    RUN_TEST(test_stringToSource_ble_battery);
    RUN_TEST(test_stringToSource_ble_rssi);
    RUN_TEST(test_stringToSource_wifi_csi_motion);
    RUN_TEST(test_stringToSource_imu_tamper);
    RUN_TEST(test_stringToSource_gpio_digital);
    RUN_TEST(test_stringToSource_null);
    RUN_TEST(test_stringToSource_empty);
    RUN_TEST(test_stringToSource_invalid);
    RUN_TEST(test_stringToSource_case_sensitive);
    
    // operatorToString tests
    RUN_TEST(test_operatorToString_above);
    RUN_TEST(test_operatorToString_below);
    RUN_TEST(test_operatorToString_unknown);
    
    // stringToOperator tests
    RUN_TEST(test_stringToOperator_above);
    RUN_TEST(test_stringToOperator_below);
    RUN_TEST(test_stringToOperator_null);
    RUN_TEST(test_stringToOperator_invalid);
    
    // severityToString tests
    RUN_TEST(test_severityToString_info);
    RUN_TEST(test_severityToString_warning);
    RUN_TEST(test_severityToString_critical);
    RUN_TEST(test_severityToString_unknown);
    
    // stringToSeverity tests
    RUN_TEST(test_stringToSeverity_info);
    RUN_TEST(test_stringToSeverity_warning);
    RUN_TEST(test_stringToSeverity_critical);
    RUN_TEST(test_stringToSeverity_null);
    RUN_TEST(test_stringToSeverity_invalid);
    
    // parseNotifyChannelsFromStrings tests
    RUN_TEST(test_parseNotifyChannels_empty);
    RUN_TEST(test_parseNotifyChannels_telegram_only);
    RUN_TEST(test_parseNotifyChannels_led_only);
    RUN_TEST(test_parseNotifyChannels_webhook_only);
    RUN_TEST(test_parseNotifyChannels_multiple);
    RUN_TEST(test_parseNotifyChannels_with_invalid);
    RUN_TEST(test_parseNotifyChannels_with_null_entry);
    RUN_TEST(test_parseNotifyChannels_duplicates);
    
    // hasChannel tests
    RUN_TEST(test_hasChannel_none);
    RUN_TEST(test_hasChannel_combined_mask);
    
    // Bitmask operator tests
    RUN_TEST(test_bitmask_or_operator);
    RUN_TEST(test_bitmask_and_operator);
    
    UNITY_END();
}

#ifdef NATIVE_BUILD
// Native environment - use main()
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    run_all_tests();
    return 0;
}
#else
// Arduino environment - use setup/loop
void setup() {
    delay(2000);  // Give time for serial monitor
    run_all_tests();
}

void loop() {
    // Empty - tests run once in setup()
}
#endif
