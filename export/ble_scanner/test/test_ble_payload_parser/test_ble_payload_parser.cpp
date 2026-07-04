/**
 * @file test_ble_payload_parser.cpp
 * @brief Unit tests for BlePayloadParser
 */

#include <unity.h>
#include <vector>
#include <cstring>
#include <cstdint>

// Include source directly for native test compilation context
#include "../../src/ble/scanner/parsers/BlePayloadParser.cpp"

using namespace BLE;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// payloadHasNamePrefix tests
// ============================================================================

void test_name_prefix_shortened_matches() {
    // 0x02, 0x01, 0x06 (Flags), 0x06, 0x08, 'T', 'P', '3', '5', '7'
    std::vector<uint8_t> payload = {
        0x02, 0x01, 0x06,
        0x06, 0x08, 'T', 'P', '3', '5', '7'
    };
    
    bool hasName = false;
    bool match = BlePayloadParser::payloadHasNamePrefix(payload.data(), payload.size(), "TP357", 5, &hasName);
    
    TEST_ASSERT_TRUE(match);
    TEST_ASSERT_TRUE(hasName);
}

void test_name_prefix_complete_matches() {
    // 0x06, 0x09, 'T', 'P', '3', '5', '7'
    std::vector<uint8_t> payload = {
        0x06, 0x09, 'T', 'P', '3', '5', '7'
    };
    
    bool hasName = false;
    bool match = BlePayloadParser::payloadHasNamePrefix(payload.data(), payload.size(), "TP357", 5, &hasName);
    
    TEST_ASSERT_TRUE(match);
    TEST_ASSERT_TRUE(hasName);
}

void test_name_prefix_mismatch() {
    // 0x06, 0x09, 'O', 't', 'h', 'e', 'r'
    std::vector<uint8_t> payload = {
        0x06, 0x09, 'O', 't', 'h', 'e', 'r'
    };
    
    bool hasName = false;
    bool match = BlePayloadParser::payloadHasNamePrefix(payload.data(), payload.size(), "TP357", 5, &hasName);
    
    TEST_ASSERT_FALSE(match);
    TEST_ASSERT_TRUE(hasName); // Has name field, but doesn't match
}

void test_no_name_field() {
    // 0x02, 0x01, 0x06 (Just flags)
    std::vector<uint8_t> payload = {
        0x02, 0x01, 0x06
    };
    
    bool hasName = false;
    bool match = BlePayloadParser::payloadHasNamePrefix(payload.data(), payload.size(), "TP357", 5, &hasName);
    
    TEST_ASSERT_FALSE(match);
    TEST_ASSERT_FALSE(hasName);
}

void test_malformed_payload() {
    // Length says 10 bytes, but vector ends
    std::vector<uint8_t> payload = {
        0x0A, 0x09, 'A'
    };
    
    bool hasName = false;
    bool match = BlePayloadParser::payloadHasNamePrefix(payload.data(), payload.size(), "A", 1, &hasName);
    
    TEST_ASSERT_FALSE(match);
    TEST_ASSERT_FALSE(hasName);
}

// ============================================================================
// findServiceData tests
// ============================================================================

void test_find_service_data_found() {
    // 0x05, 0x16, 0xD2, 0xFC, 0x11, 0x22 (UUID 0xFCD2, Data 0x11, 0x22)
    std::vector<uint8_t> payload = {
        0x05, 0x16, 0xD2, 0xFC, 0x11, 0x22
    };
    
    size_t len = 0;
    const uint8_t* data = BlePayloadParser::findServiceData(payload.data(), payload.size(), 0xFCD2, &len);
    
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL(0x11, data[0]);
    TEST_ASSERT_EQUAL(0x22, data[1]);
}

void test_find_service_data_not_found_wrong_uuid() {
    // 0x05, 0x16, 0xAA, 0xBB, 0x11, 0x22
    std::vector<uint8_t> payload = {
        0x05, 0x16, 0xAA, 0xBB, 0x11, 0x22
    };
    
    size_t len = 0;
    const uint8_t* data = BlePayloadParser::findServiceData(payload.data(), payload.size(), 0xFCD2, &len);
    
    TEST_ASSERT_NULL(data);
}

void test_find_service_data_malformed() {
    // 0x05, 0x16, 0xD2 (missing byte for uuid and data)
    std::vector<uint8_t> payload = {
        0x05, 0x16, 0xD2
    };
    
    size_t len = 0;
    const uint8_t* data = BlePayloadParser::findServiceData(payload.data(), payload.size(), 0xFCD2, &len);
    
    TEST_ASSERT_NULL(data);
}

void test_find_service_data_out_of_bounds_read() {
    // Malformed packet where AD length (0xFF) extends far beyond the actual received payload length
    std::vector<uint8_t> payload = {
        0xFF, 0x16, 0xD2, 0xFC, 0x11, 0x22
    };
    
    size_t len = 0;
    // Length argument correctly dictates the buffer size, protecting against the malicious 0xFF
    const uint8_t* data = BlePayloadParser::findServiceData(payload.data(), payload.size(), 0xFCD2, &len);
    
    // Should safely abort the search and return null without accessing out of bounds
    TEST_ASSERT_NULL(data);
}

void test_payload_has_name_out_of_bounds_read() {
    // Malformed packet where AD length (0x10) extends beyond the physical 4 bytes
    std::vector<uint8_t> payload = {
        0x10, 0x09, 'T', 'P'
    };
    
    bool hasName = false;
    bool match = BlePayloadParser::payloadHasNamePrefix(payload.data(), payload.size(), "TP357", 5, &hasName);
    
    // Should safely abort
    TEST_ASSERT_FALSE(match);
    TEST_ASSERT_FALSE(hasName); // aborted before it could declare a valid name field exists
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    UNITY_BEGIN();
    
    RUN_TEST(test_name_prefix_shortened_matches);
    RUN_TEST(test_name_prefix_complete_matches);
    RUN_TEST(test_name_prefix_mismatch);
    RUN_TEST(test_no_name_field);
    RUN_TEST(test_malformed_payload);
    
    RUN_TEST(test_find_service_data_found);
    RUN_TEST(test_find_service_data_not_found_wrong_uuid);
    RUN_TEST(test_find_service_data_malformed);
    RUN_TEST(test_find_service_data_out_of_bounds_read);
    RUN_TEST(test_payload_has_name_out_of_bounds_read);
    
    return UNITY_END();
}
