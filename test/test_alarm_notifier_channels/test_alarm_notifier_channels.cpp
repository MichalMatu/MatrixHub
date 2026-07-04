/**
 * @file test_alarm_notifier_channels.cpp
 * @brief Unit tests for NotifyChannel bitmask operations and channel verification
 */

#include <unity.h>
#include <cstdint>
#include "../../src/alarms/types/AlarmEnums.h"

using namespace ALARMS;

// ============================================================================
// Tests
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

// ----------------------------------------------------------------------------
// hasChannel() function tests
// ----------------------------------------------------------------------------

void test_has_channel_none() {
    NotifyChannel mask = NotifyChannel::None;
    
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Telegram));
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Led));
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Webhook));
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Pushover));
}

void test_has_channel_single_telegram() {
    NotifyChannel mask = NotifyChannel::Telegram;
    
    TEST_ASSERT_TRUE(hasChannel(mask, NotifyChannel::Telegram));
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Led));
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Webhook));
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Pushover));
}

void test_has_channel_single_led() {
    NotifyChannel mask = NotifyChannel::Led;
    
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Telegram));
    TEST_ASSERT_TRUE(hasChannel(mask, NotifyChannel::Led));
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Webhook));
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Pushover));
}

void test_has_channel_single_webhook() {
    NotifyChannel mask = NotifyChannel::Webhook;
    
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Telegram));
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Led));
    TEST_ASSERT_TRUE(hasChannel(mask, NotifyChannel::Webhook));
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Pushover));
}

void test_has_channel_single_pushover() {
    NotifyChannel mask = NotifyChannel::Pushover;
    
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Telegram));
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Led));
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Webhook));
    TEST_ASSERT_TRUE(hasChannel(mask, NotifyChannel::Pushover));
}

void test_has_channel_combined_telegram_pushover() {
    // This is Ch=0x09 (Telegram=0x01 + Pushover=0x08)
    NotifyChannel mask = NotifyChannel::Telegram | NotifyChannel::Pushover;
    
    TEST_ASSERT_TRUE(hasChannel(mask, NotifyChannel::Telegram));
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Led));
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Webhook));
    TEST_ASSERT_TRUE(hasChannel(mask, NotifyChannel::Pushover));
}

void test_has_channel_combined_led_pushover() {
    // This is Ch=0x0A (Led=0x02 + Pushover=0x08)
    NotifyChannel mask = NotifyChannel::Led | NotifyChannel::Pushover;
    
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Telegram));
    TEST_ASSERT_TRUE(hasChannel(mask, NotifyChannel::Led));
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Webhook));
    TEST_ASSERT_TRUE(hasChannel(mask, NotifyChannel::Pushover));
}

void test_has_channel_all() {
    // All channels: 0x0F
    NotifyChannel mask = NotifyChannel::Telegram | NotifyChannel::Led | 
                         NotifyChannel::Webhook | NotifyChannel::Pushover;
    
    TEST_ASSERT_TRUE(hasChannel(mask, NotifyChannel::Telegram));
    TEST_ASSERT_TRUE(hasChannel(mask, NotifyChannel::Led));
    TEST_ASSERT_TRUE(hasChannel(mask, NotifyChannel::Webhook));
    TEST_ASSERT_TRUE(hasChannel(mask, NotifyChannel::Pushover));
    
    // Verify bitmask value
    TEST_ASSERT_EQUAL_HEX8(0x0F, static_cast<uint8_t>(mask));
}

// ----------------------------------------------------------------------------
// Bitmask value verification tests
// ----------------------------------------------------------------------------

void test_channel_bitmask_values() {
    // Verify each channel has correct bit position
    TEST_ASSERT_EQUAL_HEX8(0x00, static_cast<uint8_t>(NotifyChannel::None));
    TEST_ASSERT_EQUAL_HEX8(0x01, static_cast<uint8_t>(NotifyChannel::Telegram));
    TEST_ASSERT_EQUAL_HEX8(0x02, static_cast<uint8_t>(NotifyChannel::Led));
    TEST_ASSERT_EQUAL_HEX8(0x04, static_cast<uint8_t>(NotifyChannel::Webhook));
    TEST_ASSERT_EQUAL_HEX8(0x08, static_cast<uint8_t>(NotifyChannel::Pushover));
}

void test_channel_or_operator() {
    // Test OR operator combines bits correctly
    NotifyChannel a = NotifyChannel::Telegram;  // 0x01
    NotifyChannel b = NotifyChannel::Webhook;   // 0x04
    NotifyChannel c = a | b;
    
    TEST_ASSERT_EQUAL_HEX8(0x05, static_cast<uint8_t>(c));
}

void test_channel_and_operator() {
    // Test AND operator works correctly
    NotifyChannel mask = NotifyChannel::Telegram | NotifyChannel::Pushover;  // 0x09
    NotifyChannel result = mask & NotifyChannel::Pushover;  // 0x09 & 0x08 = 0x08
    
    TEST_ASSERT_EQUAL_HEX8(0x08, static_cast<uint8_t>(result));
}

void test_real_world_scenario_ch0x0a() {
    // This was the actual value from user's logs: Ch=0x0A
    // 0x0A = 0b00001010 = Led (0x02) + Pushover (0x08)
    NotifyChannel mask = static_cast<NotifyChannel>(0x0A);
    
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Telegram));  // 0x01 - NOT set
    TEST_ASSERT_TRUE(hasChannel(mask, NotifyChannel::Led));        // 0x02 - SET
    TEST_ASSERT_FALSE(hasChannel(mask, NotifyChannel::Webhook));   // 0x04 - NOT set
    TEST_ASSERT_TRUE(hasChannel(mask, NotifyChannel::Pushover));   // 0x08 - SET
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    
    // hasChannel() tests
    RUN_TEST(test_has_channel_none);
    RUN_TEST(test_has_channel_single_telegram);
    RUN_TEST(test_has_channel_single_led);
    RUN_TEST(test_has_channel_single_webhook);
    RUN_TEST(test_has_channel_single_pushover);
    RUN_TEST(test_has_channel_combined_telegram_pushover);
    RUN_TEST(test_has_channel_combined_led_pushover);
    RUN_TEST(test_has_channel_all);
    
    // Bitmask tests
    RUN_TEST(test_channel_bitmask_values);
    RUN_TEST(test_channel_or_operator);
    RUN_TEST(test_channel_and_operator);
    RUN_TEST(test_real_world_scenario_ch0x0a);
    
    return UNITY_END();
}
