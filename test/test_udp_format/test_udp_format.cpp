/**
 * @file test_udp_format.cpp
 * @brief Unit tests for UDP data format generation
 * 
 * Tests cover:
 * - InfluxDB line protocol format
 * - JSON format
 * - CSV format
 * - Buffer overflow protection
 * - Edge cases (zero values, special chars)
 */

#include <cmath>
#include <cstdio>
#include <cstring>
#include <unity.h>
#include "../../src/udp/UdpPacketFormatter.h"

// For native testing, we need to link the implementation
// AND provide dummy definitions for Arduino/ESP specifics if needed
#ifdef UNIT_TEST
#include "../../src/udp/UdpPacketFormatter.cpp"
#endif

using namespace UDPPUSH;

void setUp(void) {}
void tearDown(void) {}

// A non-zero timestamp marks the snapshot as a real sensor reading.
static SensorSnapshot makeValidSnapshot() {
    SensorSnapshot snap{};
    snap.timestamp_ms = 123456;
    return snap;
}

// ============================================================================
// Test: InfluxDB Line Protocol
// ============================================================================

void test_line_protocol_basic() {
    char buffer[256];
    SensorSnapshot snap = makeValidSnapshot();
    snap.co2 = 678;
    snap.temp = 24.5f;
    snap.humid = 45.2f;
    snap.seq = 100;
    
    size_t len = UdpPacketFormatter::formatLineProtocol(buffer, sizeof(buffer), snap);
    
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_TRUE(strstr(buffer, "sensors,device=matrixhub,status=ok") != nullptr);
    TEST_ASSERT_TRUE(strstr(buffer, "co2=678i") != nullptr);
    TEST_ASSERT_TRUE(strstr(buffer, "temp=24.50") != nullptr);
    TEST_ASSERT_TRUE(strstr(buffer, "humidity=45.20") != nullptr);
}

void test_line_protocol_integer_co2() {
    char buffer[256];
    SensorSnapshot snap = makeValidSnapshot();
    snap.co2 = 1234;
    snap.temp = 20.0f;
    snap.humid = 50.0f;
    
    UdpPacketFormatter::formatLineProtocol(buffer, sizeof(buffer), snap);
    
    // CO2 should have 'i' suffix for InfluxDB integer
    TEST_ASSERT_TRUE(strstr(buffer, "co2=1234i") != nullptr);
}

void test_line_protocol_negative_temp() {
    char buffer[256];
    SensorSnapshot snap = makeValidSnapshot();
    snap.co2 = 500;
    snap.temp = -5.5f;
    snap.humid = 80.0f;
    
    size_t len = UdpPacketFormatter::formatLineProtocol(buffer, sizeof(buffer), snap);
    
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_TRUE(strstr(buffer, "temp=-5.50") != nullptr);
}

void test_line_protocol_zero_timestamp_returns_missing_record() {
    char buffer[256];
    SensorSnapshot snap = {};
    
    size_t len = UdpPacketFormatter::formatLineProtocol(buffer, sizeof(buffer), snap);
    
    // Now returns line protocol with status=missing
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_TRUE(strstr(buffer, "status=missing") != nullptr);
}

// ============================================================================
// Test: JSON Format
// ============================================================================

void test_json_basic() {
    char buffer[256];
    SensorSnapshot snap = makeValidSnapshot();
    snap.co2 = 800;
    snap.temp = 22.5f;
    snap.humid = 55.0f;
    snap.seq = 42;
    
    size_t len = UdpPacketFormatter::formatJson(buffer, sizeof(buffer), snap);
    
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_TRUE(strstr(buffer, "\"status\":\"ok\"") != nullptr);
    TEST_ASSERT_TRUE(strstr(buffer, "\"co2\":800") != nullptr);
    TEST_ASSERT_TRUE(strstr(buffer, "\"temp\":22.50") != nullptr);
    TEST_ASSERT_TRUE(strstr(buffer, "\"humidity\":55.00") != nullptr);
    TEST_ASSERT_TRUE(strstr(buffer, "\"seq\":42") != nullptr);
}

void test_json_starts_with_brace() {
    char buffer[256];
    SensorSnapshot snap = makeValidSnapshot();
    snap.co2 = 100;
    snap.temp = 10.0f;
    snap.humid = 30.0f;
    
    UdpPacketFormatter::formatJson(buffer, sizeof(buffer), snap);
    
    TEST_ASSERT_EQUAL('{', buffer[0]);
}

void test_json_ends_with_brace() {
    char buffer[256];
    SensorSnapshot snap = makeValidSnapshot();
    snap.co2 = 100;
    snap.temp = 10.0f;
    snap.humid = 30.0f;
    
    size_t len = UdpPacketFormatter::formatJson(buffer, sizeof(buffer), snap);
    
    TEST_ASSERT_EQUAL('}', buffer[len - 1]);
}

void test_json_zero_timestamp_returns_missing_record() {
    char buffer[256];
    SensorSnapshot snap = {};
    
    size_t len = UdpPacketFormatter::formatJson(buffer, sizeof(buffer), snap);
    
    // Now returns json with status=missing
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_TRUE(strstr(buffer, "\"status\":\"missing\"") != nullptr);
}

void test_json_large_seq() {
    char buffer[256];
    SensorSnapshot snap = makeValidSnapshot();
    snap.co2 = 500;
    snap.temp = 20.0f;
    snap.humid = 40.0f;
    snap.seq = 4294967295U;
    
    size_t len = UdpPacketFormatter::formatJson(buffer, sizeof(buffer), snap);
    
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_TRUE(strstr(buffer, "4294967295") != nullptr);
}

// ============================================================================
// Test: CSV Format
// ============================================================================

void test_csv_basic() {
    char buffer[256];
    SensorSnapshot snap = makeValidSnapshot();
    snap.co2 = 900;
    snap.temp = 25.0f;
    snap.humid = 60.0f;
    
    size_t len = UdpPacketFormatter::formatCsv(buffer, sizeof(buffer), snap);
    
    TEST_ASSERT_GREATER_THAN(0, len);
    // Expected: "ok,900,25.00,60.00,0" (status,co2,temp,humidity,seq)
    TEST_ASSERT_EQUAL_STRING("ok,900,25.00,60.00,0", buffer);
}

void test_csv_comma_separated() {
    char buffer[256];
    SensorSnapshot snap = makeValidSnapshot();
    snap.co2 = 1000;
    snap.temp = 30.0f;
    snap.humid = 70.0f;
    
    UdpPacketFormatter::formatCsv(buffer, sizeof(buffer), snap);
    
    // Count commas
    int commas = 0;
    for (size_t i = 0; i < strlen(buffer); i++) {
        if (buffer[i] == ',') commas++;
    }
    TEST_ASSERT_EQUAL(4, commas);  // status,co2,temp,humidity,seq -> 4 commas
}

void test_csv_negative_temp() {
    char buffer[256];
    SensorSnapshot snap = makeValidSnapshot();
    snap.co2 = 500;
    snap.temp = -10.5f;
    snap.humid = 90.0f;
    
    UdpPacketFormatter::formatCsv(buffer, sizeof(buffer), snap);
    
    TEST_ASSERT_TRUE(strstr(buffer, "-10.50") != nullptr);
}

void test_csv_zero_timestamp_returns_missing_record() {
    char buffer[256];
    SensorSnapshot snap = {};
    
    size_t len = UdpPacketFormatter::formatCsv(buffer, sizeof(buffer), snap);
    
    // Now returns "missing" status record instead of nothing
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_TRUE(strstr(buffer, "missing") != nullptr);
}

// ============================================================================
// Test: Buffer overflow protection
// ============================================================================

void test_line_protocol_small_buffer_returns_zero() {
    char buffer[10];  // Too small
    SensorSnapshot snap = makeValidSnapshot();
    snap.co2 = 500;
    snap.temp = 20.0f;
    snap.humid = 50.0f;
    
    size_t len = UdpPacketFormatter::formatLineProtocol(buffer, sizeof(buffer), snap);
    
    TEST_ASSERT_EQUAL(0, len);
}

void test_json_small_buffer_returns_zero() {
    char buffer[10];
    SensorSnapshot snap = makeValidSnapshot();
    snap.co2 = 500;
    snap.temp = 20.0f;
    snap.humid = 50.0f;
    
    size_t len = UdpPacketFormatter::formatJson(buffer, sizeof(buffer), snap);
    
    TEST_ASSERT_EQUAL(0, len);
}

void test_csv_exact_buffer_size() {
// "ok,100,20.00,50.00,0" = 20 chars + null = 21 bytes needed
// However snprintf on some platforms reads slightly past the exact bound when formatting float. Give it 32.
    char buffer[32];
    SensorSnapshot snap = makeValidSnapshot();
    snap.co2 = 100;
    snap.temp = 20.0f;
    snap.humid = 50.0f;
    
    size_t len = UdpPacketFormatter::formatCsv(buffer, 21, snap); // limit explicitly to 21
    
    // Should fit exactly: 20 chars
    TEST_ASSERT_EQUAL(20, len);
    TEST_ASSERT_EQUAL_STRING("ok,100,20.00,50.00,0", buffer);
}

// ============================================================================
// Test: Edge values
// ============================================================================

void test_max_co2_value() {
    char buffer[256];
    SensorSnapshot snap = makeValidSnapshot();
    snap.co2 = 65535;
    snap.temp = 25.0f;
    snap.humid = 50.0f;
    
    size_t len = UdpPacketFormatter::formatLineProtocol(buffer, sizeof(buffer), snap);
    
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_TRUE(strstr(buffer, "co2=65535i") != nullptr);
}

void test_decimal_precision() {
    char buffer[256];
    SensorSnapshot snap = makeValidSnapshot();
    snap.co2 = 500;
    snap.temp = 23.456f;
    snap.humid = 67.891f;
    
    UdpPacketFormatter::formatCsv(buffer, sizeof(buffer), snap);
    
    // Should round to 2 decimal places
    TEST_ASSERT_TRUE(strstr(buffer, "23.46") != nullptr);  // rounded
    TEST_ASSERT_TRUE(strstr(buffer, "67.89") != nullptr);
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    UNITY_BEGIN();
    
    // Line protocol
    RUN_TEST(test_line_protocol_basic);
    RUN_TEST(test_line_protocol_integer_co2);
    RUN_TEST(test_line_protocol_negative_temp);
    RUN_TEST(test_line_protocol_zero_timestamp_returns_missing_record);
    
    // JSON
    RUN_TEST(test_json_basic);
    RUN_TEST(test_json_starts_with_brace);
    RUN_TEST(test_json_ends_with_brace);
    RUN_TEST(test_json_zero_timestamp_returns_missing_record);
    RUN_TEST(test_json_large_seq);
    
    // CSV
    RUN_TEST(test_csv_basic);
    RUN_TEST(test_csv_comma_separated);
    RUN_TEST(test_csv_negative_temp);
    RUN_TEST(test_csv_zero_timestamp_returns_missing_record);
    
    // Buffer overflow
    RUN_TEST(test_line_protocol_small_buffer_returns_zero);
    RUN_TEST(test_json_small_buffer_returns_zero);
    RUN_TEST(test_csv_exact_buffer_size);
    
    // Edge values
    RUN_TEST(test_max_co2_value);
    RUN_TEST(test_decimal_precision);
    
    return UNITY_END();
}
