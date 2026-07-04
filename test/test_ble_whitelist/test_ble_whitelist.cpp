/**
 * @file test_ble_whitelist.cpp
 * @brief Unit tests for BleDeviceWhitelist
 * 
 * Tests cover:
 * - Empty whitelist behavior
 * - Adding and checking devices
 * - Case-insensitive MAC matching
 * - Capacity limits (kMaxBleSensors)
 * - Update operations
 */

#include <unity.h>
#include <cstring>
#include <atomic>

#define NATIVE_BUILD 1

// Mock FreeRTOS functions for native build
#define pdMS_TO_TICKS(x) (x)
inline void vTaskDelay(uint32_t ticks) { (void)ticks; }

// Include stubs first
#include <Arduino.h>

// Include types from actual source
#include "../../src/ble/BleTypes.h"
#include "../../src/system/rtc/RtcConfig.h"

// Now include the whitelist header and implementation
#include "../../src/ble/scanner/filters/BleDeviceWhitelist.h"

// Provide mock for log implementation to satisfy linker
#include "../../src/system/logging/Logging.h"
namespace LOG {
    Settings Logging::_settings;
    void Logging::log(esp_log_level_t level, const char *tag, const char *fmt, ...) {}
    void Logging::logSection(const char* title) {}
}

#include "../../src/ble/scanner/filters/BleDeviceWhitelist.cpp"

using namespace BLE;

// Test fixture
static BleDeviceWhitelist* whitelistPtr = nullptr;
#define whitelist (*whitelistPtr)

void setUp(void) {
    if (!whitelistPtr) {
        whitelistPtr = new BleDeviceWhitelist();
    }
    whitelistPtr->update(nullptr, 0);
}

void tearDown(void) {}

// Helper to create a sensor config
static BleSensorConfig createSensor(const char* mac, const char* alias = "") {
    BleSensorConfig cfg;
    strncpy(cfg.mac, mac, sizeof(cfg.mac) - 1);
    strncpy(cfg.alias, alias, sizeof(cfg.alias) - 1);
    return cfg;
}

// ============================================================================
// Test: Empty whitelist
// ============================================================================

void test_empty_whitelist_is_empty() {
    TEST_ASSERT_TRUE(whitelist.isEmpty());
    TEST_ASSERT_EQUAL(0, whitelist.count());
}

void test_empty_whitelist_rejects_all() {
    TEST_ASSERT_FALSE(whitelist.isWhitelisted("AA:BB:CC:DD:EE:FF"));
    TEST_ASSERT_FALSE(whitelist.isWhitelisted("00:00:00:00:00:00"));
}

// ============================================================================
// Test: Basic whitelist operations
// ============================================================================

void test_add_single_device() {
    BleSensorConfig sensors[1];
    sensors[0] = createSensor("AA:BB:CC:DD:EE:FF", "Sensor1");
    
    whitelist.update(sensors, 1);
    
    TEST_ASSERT_FALSE(whitelist.isEmpty());
    TEST_ASSERT_EQUAL(1, whitelist.count());
    TEST_ASSERT_TRUE(whitelist.isWhitelisted("AA:BB:CC:DD:EE:FF"));
}

void test_add_multiple_devices() {
    BleSensorConfig sensors[3];
    sensors[0] = createSensor("AA:BB:CC:DD:EE:01", "Sensor1");
    sensors[1] = createSensor("AA:BB:CC:DD:EE:02", "Sensor2");
    sensors[2] = createSensor("AA:BB:CC:DD:EE:03", "Sensor3");
    
    whitelist.update(sensors, 3);
    
    TEST_ASSERT_EQUAL(3, whitelist.count());
    TEST_ASSERT_TRUE(whitelist.isWhitelisted("AA:BB:CC:DD:EE:01"));
    TEST_ASSERT_TRUE(whitelist.isWhitelisted("AA:BB:CC:DD:EE:02"));
    TEST_ASSERT_TRUE(whitelist.isWhitelisted("AA:BB:CC:DD:EE:03"));
}

void test_non_whitelisted_rejected() {
    BleSensorConfig sensors[1];
    sensors[0] = createSensor("AA:BB:CC:DD:EE:FF");
    
    whitelist.update(sensors, 1);
    
    TEST_ASSERT_FALSE(whitelist.isWhitelisted("11:22:33:44:55:66"));
    TEST_ASSERT_FALSE(whitelist.isWhitelisted("AA:BB:CC:DD:EE:00"));
}

// ============================================================================
// Test: Case-insensitive MAC matching
// ============================================================================

void test_case_insensitive_lowercase() {
    BleSensorConfig sensors[1];
    sensors[0] = createSensor("AA:BB:CC:DD:EE:FF");
    
    whitelist.update(sensors, 1);
    
    TEST_ASSERT_TRUE(whitelist.isWhitelisted("aa:bb:cc:dd:ee:ff"));
}

void test_case_insensitive_mixed() {
    BleSensorConfig sensors[1];
    sensors[0] = createSensor("aa:bb:cc:dd:ee:ff");
    
    whitelist.update(sensors, 1);
    
    TEST_ASSERT_TRUE(whitelist.isWhitelisted("AA:BB:CC:DD:EE:FF"));
    TEST_ASSERT_TRUE(whitelist.isWhitelisted("Aa:Bb:Cc:Dd:Ee:Ff"));
}

// ============================================================================
// Test: Capacity limits
// ============================================================================

void test_max_capacity() {
    BleSensorConfig sensors[RTC::kMaxBleSensors];
    for (int i = 0; i < RTC::kMaxBleSensors; i++) {
        char mac[18];
        snprintf(mac, sizeof(mac), "AA:BB:CC:DD:EE:%02X", i);
        sensors[i] = createSensor(mac);
    }
    
    whitelist.update(sensors, RTC::kMaxBleSensors);
    
    TEST_ASSERT_EQUAL(RTC::kMaxBleSensors, whitelist.count());
    
    for (int i = 0; i < RTC::kMaxBleSensors; i++) {
        char mac[18];
        snprintf(mac, sizeof(mac), "AA:BB:CC:DD:EE:%02X", i);
        TEST_ASSERT_TRUE(whitelist.isWhitelisted(mac));
    }
}

void test_exceeds_capacity_truncates() {
    BleSensorConfig sensors[RTC::kMaxBleSensors + 5];
    for (size_t i = 0; i < RTC::kMaxBleSensors + 5; i++) {
        char mac[18];
        snprintf(mac, sizeof(mac), "AA:BB:CC:DD:EE:%02X", (int)i);
        sensors[i] = createSensor(mac);
    }
    
    whitelist.update(sensors, RTC::kMaxBleSensors + 5);
    
    TEST_ASSERT_EQUAL(RTC::kMaxBleSensors, whitelist.count());
    
    TEST_ASSERT_TRUE(whitelist.isWhitelisted("AA:BB:CC:DD:EE:00"));
    TEST_ASSERT_TRUE(whitelist.isWhitelisted("AA:BB:CC:DD:EE:07"));
    
    TEST_ASSERT_FALSE(whitelist.isWhitelisted("AA:BB:CC:DD:EE:08"));
    TEST_ASSERT_FALSE(whitelist.isWhitelisted("AA:BB:CC:DD:EE:0C"));
}

// ============================================================================
// Test: Update replaces previous list
// ============================================================================

void test_update_replaces_list() {
    BleSensorConfig sensors1[2];
    sensors1[0] = createSensor("11:11:11:11:11:11");
    sensors1[1] = createSensor("22:22:22:22:22:22");
    whitelist.update(sensors1, 2);
    
    TEST_ASSERT_TRUE(whitelist.isWhitelisted("11:11:11:11:11:11"));
    TEST_ASSERT_TRUE(whitelist.isWhitelisted("22:22:22:22:22:22"));
    
    BleSensorConfig sensors2[1];
    sensors2[0] = createSensor("33:33:33:33:33:33");
    whitelist.update(sensors2, 1);
    
    TEST_ASSERT_EQUAL(1, whitelist.count());
    TEST_ASSERT_TRUE(whitelist.isWhitelisted("33:33:33:33:33:33"));
    TEST_ASSERT_FALSE(whitelist.isWhitelisted("11:11:11:11:11:11"));
    TEST_ASSERT_FALSE(whitelist.isWhitelisted("22:22:22:22:22:22"));
}

void test_update_to_empty_clears() {
    BleSensorConfig sensors[1];
    sensors[0] = createSensor("AA:BB:CC:DD:EE:FF");
    whitelist.update(sensors, 1);
    
    TEST_ASSERT_FALSE(whitelist.isEmpty());
    
    whitelist.update(nullptr, 0);
    
    TEST_ASSERT_TRUE(whitelist.isEmpty());
    TEST_ASSERT_FALSE(whitelist.isWhitelisted("AA:BB:CC:DD:EE:FF"));
}

// ============================================================================
// Test: Edge cases
// ============================================================================

void test_null_mac_not_whitelisted() {
    BleSensorConfig sensors[1];
    sensors[0] = createSensor("AA:BB:CC:DD:EE:FF");
    whitelist.update(sensors, 1);
    
    TEST_ASSERT_FALSE(whitelist.isWhitelisted(nullptr));
}

void test_empty_mac_not_whitelisted() {
    BleSensorConfig sensors[1];
    sensors[0] = createSensor("AA:BB:CC:DD:EE:FF");
    whitelist.update(sensors, 1);
    
    TEST_ASSERT_FALSE(whitelist.isWhitelisted(""));
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    UNITY_BEGIN();
    
    RUN_TEST(test_empty_whitelist_is_empty);
    RUN_TEST(test_empty_whitelist_rejects_all);
    
    RUN_TEST(test_add_single_device);
    RUN_TEST(test_add_multiple_devices);
    RUN_TEST(test_non_whitelisted_rejected);
    
    RUN_TEST(test_case_insensitive_lowercase);
    RUN_TEST(test_case_insensitive_mixed);
    
    RUN_TEST(test_max_capacity);
    RUN_TEST(test_exceeds_capacity_truncates);
    
    RUN_TEST(test_update_replaces_list);
    RUN_TEST(test_update_to_empty_clears);
    
    RUN_TEST(test_null_mac_not_whitelisted);
    RUN_TEST(test_empty_mac_not_whitelisted);
    
    return UNITY_END();
}
