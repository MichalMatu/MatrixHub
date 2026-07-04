/**
 * @file test_ble_device_type_detector.cpp
 * @brief Unit tests for BleDeviceTypeDetector
 */

#include <unity.h>
#include <vector>
#include <cstring>
#include <cstdint>

// Include dependencies
#include "../../src/ble/scanner/parsers/BlePayloadParser.cpp"
#include "../../src/ble/scanner/parsers/BtHomeParser.h"
#include "../../src/ble/scanner/filters/BleDeviceTypeDetector.cpp"

using namespace BLE;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// TP357 Detection Tests
// ============================================================================

void test_detect_tp357_shortened_name() {
    // 0x02, 0x01, 0x06 (Flags), 0x06, 0x08, 'T', 'P', '3', '5', '7'
    std::vector<uint8_t> payload = {
        0x02, 0x01, 0x06,
        0x06, 0x08, 'T', 'P', '3', '5', '7'
    };
    
    BleDeviceTypeResult result = BleDeviceTypeDetector::detect(payload.data(), payload.size());
    
    TEST_ASSERT_EQUAL(BleDeviceType::TP357, result.type);
    TEST_ASSERT_NULL(result.btHomeData);
    TEST_ASSERT_EQUAL(0, result.btHomeDataLen);
}

void test_detect_tp357_complete_name() {
    // 0x06, 0x09, 'T', 'P', '3', '5', '7'
    std::vector<uint8_t> payload = {
        0x06, 0x09, 'T', 'P', '3', '5', '7'
    };
    
    BleDeviceTypeResult result = BleDeviceTypeDetector::detect(payload.data(), payload.size());
    
    TEST_ASSERT_EQUAL(BleDeviceType::TP357, result.type);
    TEST_ASSERT_NULL(result.btHomeData);
}

// ============================================================================
// BTHome Detection Tests (ATC prefix)
// ============================================================================

void test_detect_bthome_atc_with_service_data() {
    // 0x04, 0x09, 'A', 'T', 'C' (Name: ATC)
    // 0x05, 0x16, 0xD2, 0xFC, 0x40, 0x00 (Service Data: BTHome UUID 0xFCD2)
    std::vector<uint8_t> payload = {
        0x04, 0x09, 'A', 'T', 'C',
        0x05, 0x16, 0xD2, 0xFC, 0x40, 0x00
    };
    
    BleDeviceTypeResult result = BleDeviceTypeDetector::detect(payload.data(), payload.size());
    
    TEST_ASSERT_EQUAL(BleDeviceType::BTHome, result.type);
    TEST_ASSERT_NOT_NULL(result.btHomeData);
    TEST_ASSERT_EQUAL(2, result.btHomeDataLen); // 0x40, 0x00
    TEST_ASSERT_EQUAL(0x40, result.btHomeData[0]);
}

void test_detect_bthome_lywsd_with_service_data() {
    // 0x06, 0x09, 'L', 'Y', 'W', 'S', 'D' (Name: LYWSD)
    // 0x09, 0x16, 0xD2, 0xFC, 0x40, 0x02, 0xEC, 0x09, 0x03, 0xC6 (Service Data)
    std::vector<uint8_t> payload = {
        0x06, 0x09, 'L', 'Y', 'W', 'S', 'D',
        0x09, 0x16, 0xD2, 0xFC, 0x40, 0x02, 0xEC, 0x09, 0x03, 0xC6
    };
    
    BleDeviceTypeResult result = BleDeviceTypeDetector::detect(payload.data(), payload.size());
    
    TEST_ASSERT_EQUAL(BleDeviceType::BTHome, result.type);
    TEST_ASSERT_NOT_NULL(result.btHomeData);
    TEST_ASSERT_EQUAL(6, result.btHomeDataLen); // 0x40 ... 0xC6
}

// ============================================================================
// BTHome Detection Tests (no name prefix, service UUID only)
// ============================================================================

void test_detect_bthome_no_name_service_data_only() {
    // No name field, just flags and BTHome service data
    // 0x02, 0x01, 0x06 (Flags)
    // 0x05, 0x16, 0xD2, 0xFC, 0x40, 0x55 (Service Data: BTHome UUID 0xFCD2)
    std::vector<uint8_t> payload = {
        0x02, 0x01, 0x06,
        0x05, 0x16, 0xD2, 0xFC, 0x40, 0x55
    };
    
    BleDeviceTypeResult result = BleDeviceTypeDetector::detect(payload.data(), payload.size());
    
    TEST_ASSERT_EQUAL(BleDeviceType::BTHome, result.type);
    TEST_ASSERT_NOT_NULL(result.btHomeData);
    TEST_ASSERT_EQUAL(2, result.btHomeDataLen);
}

// ============================================================================
// Unknown Device Detection Tests
// ============================================================================

void test_detect_unknown_no_matching_name() {
    // 0x06, 0x09, 'O', 't', 'h', 'e', 'r'
    std::vector<uint8_t> payload = {
        0x06, 0x09, 'O', 't', 'h', 'e', 'r'
    };
    
    BleDeviceTypeResult result = BleDeviceTypeDetector::detect(payload.data(), payload.size());
    
    TEST_ASSERT_EQUAL(BleDeviceType::Unknown, result.type);
    TEST_ASSERT_NULL(result.btHomeData);
}

void test_detect_unknown_empty_payload() {
    std::vector<uint8_t> payload = {};
    
    BleDeviceTypeResult result = BleDeviceTypeDetector::detect(payload.data(), payload.size());
    
    TEST_ASSERT_EQUAL(BleDeviceType::Unknown, result.type);
}

void test_detect_unknown_null_payload() {
    BleDeviceTypeResult result = BleDeviceTypeDetector::detect(nullptr, 0);
    
    TEST_ASSERT_EQUAL(BleDeviceType::Unknown, result.type);
}

// ============================================================================
// Edge Cases
// ============================================================================

void test_detect_atc_prefix_but_no_service_data() {
    // Name matches ATC but no BTHome service data present
    // 0x04, 0x09, 'A', 'T', 'C'
    // 0x02, 0x01, 0x06 (Just flags, no service data)
    std::vector<uint8_t> payload = {
        0x04, 0x09, 'A', 'T', 'C',
        0x02, 0x01, 0x06
    };
    
    BleDeviceTypeResult result = BleDeviceTypeDetector::detect(payload.data(), payload.size());
    
    // Current behavior: ATC prefix sets type to BTHome even without service data
    // This is acceptable - name prefix is strong indicator
    TEST_ASSERT_EQUAL(BleDeviceType::BTHome, result.type);
    TEST_ASSERT_NULL(result.btHomeData); // But no service data found
    TEST_ASSERT_EQUAL(0, result.btHomeDataLen);
}

void test_detect_tp357_takes_priority_over_bthome() {
    // If somehow both TP357 name and BTHome service present (unlikely in real world),
    // TP357 detection should return early
    std::vector<uint8_t> payload = {
        0x06, 0x09, 'T', 'P', '3', '5', '7',
        0x05, 0x16, 0xD2, 0xFC, 0x40, 0x00
    };
    
    BleDeviceTypeResult result = BleDeviceTypeDetector::detect(payload.data(), payload.size());
    
    TEST_ASSERT_EQUAL(BleDeviceType::TP357, result.type);
    TEST_ASSERT_NULL(result.btHomeData); // TP357 path returns early, no BTHome data
}

// ============================================================================
// Unity Setup
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // TP357 Detection
    RUN_TEST(test_detect_tp357_shortened_name);
    RUN_TEST(test_detect_tp357_complete_name);
    
    // BTHome Detection (with name prefix)
    RUN_TEST(test_detect_bthome_atc_with_service_data);
    RUN_TEST(test_detect_bthome_lywsd_with_service_data);
    
    // BTHome Detection (no name prefix)
    RUN_TEST(test_detect_bthome_no_name_service_data_only);
    
    // Unknown Device Detection
    RUN_TEST(test_detect_unknown_no_matching_name);
    RUN_TEST(test_detect_unknown_empty_payload);
    RUN_TEST(test_detect_unknown_null_payload);
    
    // Edge Cases
    RUN_TEST(test_detect_atc_prefix_but_no_service_data);
    RUN_TEST(test_detect_tp357_takes_priority_over_bthome);
    
    return UNITY_END();
}
