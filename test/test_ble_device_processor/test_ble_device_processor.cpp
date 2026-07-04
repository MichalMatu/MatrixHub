/**
 * @file test_ble_device_processor.cpp
 * @brief Unit tests for BleDeviceProcessor
 */

#include <unity.h>
#include <vector>
#include <string>

// Include mocks before anything else
#include "NimBLEDevice.h"
#include "NimBLEScan.h"

// Provide Arduino/FreeRTOS stubs
#define NATIVE_BUILD 1
#define pdMS_TO_TICKS(x) (x)
inline void vTaskDelay(uint32_t ticks) { (void)ticks; }
#include <Arduino.h>

// Provide mock for log implementation
#include "../../src/system/logging/Logging.h"
namespace LOG {
    Settings Logging::_settings;
    void Logging::log(esp_log_level_t level, const char *tag, const char *fmt, ...) {}
    void Logging::logSection(const char* title) {}
}

// Include the implementation files directly
#include "../../src/ble/scanner/parsers/BlePayloadParser.cpp"
#include "../../src/ble/scanner/parsers/BleDataParser.cpp"
#include "../../src/ble/scanner/filters/BleDeviceTypeDetector.cpp"
#include "../../src/ble/scanner/filters/BleDeviceWhitelist.cpp"
#include "../../src/ble/scanner/BleDeviceProcessor.cpp"

using namespace BLE;

// Minimal implementation of BleScanner to satisfy linker
BleScanner::BleScanner() {}
BleScanner::~BleScanner() {}
bool BleScanner::shouldReport(const char* mac, float temperature, float humidity, uint8_t battery, int8_t rssi, uint32_t nowMs) { return true; }
bool BleScanner::updateDiscoveryCache(const char* mac, float temp, float humid, uint8_t batt, int8_t rssi, uint32_t nowMs) { return true; }
void BleScanner::onScanComplete(const NimBLEScanResults& results) {}
void BleScanner::onResult(const NimBLEAdvertisedDevice* device) {}
void BleScanner::onScanEnd(const NimBLEScanResults& results, int reason) {}

// ============================================================================
// Setup
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Tests
// ============================================================================

void test_processor_increments_total_adv() {
    RTC::RtcBleStats telemetry;
    memset(&telemetry, 0, sizeof(telemetry));
    
    BleScanner scanner;
    BleDeviceWhitelist whitelist;
    NimBLEAdvertisedDevice device;
    
    // 0x06, 0x09, 'T', 'P', '3', '5', '7' (Matching name)
    device.setPayload({0x06, 0x09, 'T', 'P', '3', '5', '7'});
    
    BleDeviceProcessor::process(
        &device,
        scanner,
        whitelist,
        nullptr, // callback
        true,    // discoveryMode
        false,   // targetedMode
        telemetry
    );
    
    TEST_ASSERT_EQUAL(1, telemetry.advTotal);
    TEST_ASSERT_EQUAL(1, telemetry.advMatchedNamePrefix);
    TEST_ASSERT_EQUAL(0, telemetry.advParsedValid);
    TEST_ASSERT_EQUAL(1, telemetry.parserErrors);
}

void test_processor_filters_unknown_devices_in_targeted_mode() {
    RTC::RtcBleStats telemetry;
    memset(&telemetry, 0, sizeof(telemetry));
    
    BleScanner scanner;
    BleDeviceWhitelist whitelist;
    NimBLEAdvertisedDevice device;
    
    // Unknown device (doesn't match TP357 or BTHome)
    device.setPayload({0x06, 0x09, 'O', 't', 'h', 'e', 'r'});
    
    BleDeviceProcessor::process(
        &device,
        scanner,
        whitelist,
        nullptr,
        false,   // discoveryMode = false
        true,    // targetedMode = true (Whitelist only)
        telemetry
    );
    
    TEST_ASSERT_EQUAL(1, telemetry.advTotal);
    TEST_ASSERT_EQUAL(0, telemetry.advMatchedNamePrefix); // unknown
}

void test_processor_accepts_whitelisted_device_in_targeted_mode() {
    RTC::RtcBleStats telemetry;
    memset(&telemetry, 0, sizeof(telemetry));
    
    BleScanner scanner;
    BleDeviceWhitelist whitelist;
    NimBLEAdvertisedDevice device;
    
    // Setup whitelist for this MAC
    // Note: My mock NimBLEAddress returns AA:BB:CC:DD:EE:FF in reversed order for getVal()
    // So the snprintf in process() will produce "AA:BB:CC:DD:EE:FF"
    BleSensorConfig sensor;
    memset(&sensor, 0, sizeof(sensor));
    strlcpy(sensor.mac, "AA:BB:CC:DD:EE:FF", sizeof(sensor.mac));
    whitelist.update(&sensor, 1);
    
    // Targeted payload
    device.setPayload({0x06, 0x09, 'T', 'P', '3', '5', '7'});
    device.setAddress("AA:BB:CC:DD:EE:FF");
    
    BleDeviceProcessor::process(
        &device,
        scanner,
        whitelist,
        nullptr,
        false,   // discoveryMode = false
        true,    // targetedMode = true
        telemetry
    );
    
    TEST_ASSERT_EQUAL(1, telemetry.advTotal);
    TEST_ASSERT_EQUAL(1, telemetry.advWhitelisted);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_processor_increments_total_adv);
    RUN_TEST(test_processor_filters_unknown_devices_in_targeted_mode);
    RUN_TEST(test_processor_accepts_whitelisted_device_in_targeted_mode);
    return UNITY_END();
}
