/**
 * @file test_ip_validator.cpp
 * @brief Unit tests for IpValidator (SSRF protection)
 * 
 * Tests cover:
 * - Valid private IP ranges (10.x, 172.16-31.x, 192.168.x)
 * - Invalid/rejected IPs (loopback, broadcast, public)
 * - Format validation
 * - Edge cases and malformed input
 */

#include <unity.h>
#include <cstring>
#include <cstdio>

#define NATIVE_BUILD 1

// Mock logging
#define LOGW(...)

// Include implementation directly
namespace SHELLY {

class IpValidator {
public:
    static bool isValidPrivateIp(const char* ip);
    static bool isValidFormat(const char* ip);
    static bool isPrivateRange(const char* ip);
    static bool isLoopback(const char* ip);

private:
    static bool parseOctets(const char* ip, int& o1, int& o2, int& o3, int& o4);
};

bool IpValidator::parseOctets(const char* ip, int& o1, int& o2, int& o3, int& o4) {
    if (!ip || strlen(ip) == 0 || strlen(ip) > 15) {
        return false;
    }
    
    char extra;
    int parsed = sscanf(ip, "%d.%d.%d.%d%c", &o1, &o2, &o3, &o4, &extra);
    return (parsed == 4);
}

bool IpValidator::isValidFormat(const char* ip) {
    int o1, o2, o3, o4;
    
    if (!parseOctets(ip, o1, o2, o3, o4)) {
        return false;
    }
    
    if (o1 < 0 || o1 > 255 || 
        o2 < 0 || o2 > 255 || 
        o3 < 0 || o3 > 255 || 
        o4 < 0 || o4 > 255) {
        return false;
    }
    
    return true;
}

bool IpValidator::isLoopback(const char* ip) {
    int o1, o2, o3, o4;
    
    if (!parseOctets(ip, o1, o2, o3, o4)) {
        return false;
    }
    
    return (o1 == 127);
}

bool IpValidator::isPrivateRange(const char* ip) {
    int o1, o2, o3, o4;
    
    if (!parseOctets(ip, o1, o2, o3, o4)) {
        return false;
    }
    
    return (o1 == 10) ||
           (o1 == 172 && o2 >= 16 && o2 <= 31) ||
           (o1 == 192 && o2 == 168);
}

bool IpValidator::isValidPrivateIp(const char* ip) {
    int o1, o2, o3, o4;
    
    if (!parseOctets(ip, o1, o2, o3, o4)) {
        return false;
    }
    
    if (o1 < 0 || o1 > 255 || 
        o2 < 0 || o2 > 255 || 
        o3 < 0 || o3 > 255 || 
        o4 < 0 || o4 > 255) {
        return false;
    }
    
    // Block loopback
    if (o1 == 127) {
        return false;
    }
    
    // Block broadcast/any
    if ((o1 == 0 && o2 == 0 && o3 == 0 && o4 == 0) ||
        (o1 == 255 && o2 == 255 && o3 == 255 && o4 == 255)) {
        return false;
    }
    
    // Only private ranges
    bool isPrivate = 
        (o1 == 10) ||
        (o1 == 172 && o2 >= 16 && o2 <= 31) ||
        (o1 == 192 && o2 == 168);
    
    return isPrivate;
}

}  // namespace SHELLY

using namespace SHELLY;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Test: Valid format
// ============================================================================

void test_valid_format_simple() {
    TEST_ASSERT_TRUE(IpValidator::isValidFormat("192.168.1.1"));
    TEST_ASSERT_TRUE(IpValidator::isValidFormat("10.0.0.1"));
    TEST_ASSERT_TRUE(IpValidator::isValidFormat("0.0.0.0"));
    TEST_ASSERT_TRUE(IpValidator::isValidFormat("255.255.255.255"));
}

void test_invalid_format_null() {
    TEST_ASSERT_FALSE(IpValidator::isValidFormat(nullptr));
}

void test_invalid_format_empty() {
    TEST_ASSERT_FALSE(IpValidator::isValidFormat(""));
}

void test_invalid_format_incomplete() {
    TEST_ASSERT_FALSE(IpValidator::isValidFormat("192.168.1"));
    TEST_ASSERT_FALSE(IpValidator::isValidFormat("192.168"));
    TEST_ASSERT_FALSE(IpValidator::isValidFormat("192"));
}

void test_invalid_format_garbage() {
    TEST_ASSERT_FALSE(IpValidator::isValidFormat("abc.def.ghi.jkl"));
    TEST_ASSERT_FALSE(IpValidator::isValidFormat("hello world"));
}

void test_invalid_format_extra_chars() {
    TEST_ASSERT_FALSE(IpValidator::isValidFormat("192.168.1.1x"));
    TEST_ASSERT_FALSE(IpValidator::isValidFormat("192.168.1.1 "));
}

void test_invalid_format_octet_out_of_range() {
    TEST_ASSERT_FALSE(IpValidator::isValidFormat("256.1.1.1"));
    TEST_ASSERT_FALSE(IpValidator::isValidFormat("1.300.1.1"));
    TEST_ASSERT_FALSE(IpValidator::isValidFormat("1.1.1.999"));
}

void test_invalid_format_negative() {
    TEST_ASSERT_FALSE(IpValidator::isValidFormat("-1.1.1.1"));
}

// ============================================================================
// Test: Private ranges
// ============================================================================

void test_private_range_10() {
    TEST_ASSERT_TRUE(IpValidator::isPrivateRange("10.0.0.1"));
    TEST_ASSERT_TRUE(IpValidator::isPrivateRange("10.255.255.255"));
    TEST_ASSERT_TRUE(IpValidator::isPrivateRange("10.100.50.25"));
}

void test_private_range_172() {
    TEST_ASSERT_TRUE(IpValidator::isPrivateRange("172.16.0.1"));
    TEST_ASSERT_TRUE(IpValidator::isPrivateRange("172.31.255.255"));
    TEST_ASSERT_TRUE(IpValidator::isPrivateRange("172.20.10.5"));
    
    // Outside 172.16-31 range
    TEST_ASSERT_FALSE(IpValidator::isPrivateRange("172.15.0.1"));
    TEST_ASSERT_FALSE(IpValidator::isPrivateRange("172.32.0.1"));
}

void test_private_range_192_168() {
    TEST_ASSERT_TRUE(IpValidator::isPrivateRange("192.168.0.1"));
    TEST_ASSERT_TRUE(IpValidator::isPrivateRange("192.168.255.255"));
    TEST_ASSERT_TRUE(IpValidator::isPrivateRange("192.168.1.100"));
    
    // Not 192.168.x.x
    TEST_ASSERT_FALSE(IpValidator::isPrivateRange("192.169.1.1"));
    TEST_ASSERT_FALSE(IpValidator::isPrivateRange("192.167.1.1"));
}

void test_public_ip_not_private() {
    TEST_ASSERT_FALSE(IpValidator::isPrivateRange("8.8.8.8"));
    TEST_ASSERT_FALSE(IpValidator::isPrivateRange("1.1.1.1"));
    TEST_ASSERT_FALSE(IpValidator::isPrivateRange("142.250.185.78")); // google.com
}

// ============================================================================
// Test: Loopback detection
// ============================================================================

void test_loopback_detected() {
    TEST_ASSERT_TRUE(IpValidator::isLoopback("127.0.0.1"));
    TEST_ASSERT_TRUE(IpValidator::isLoopback("127.0.0.0"));
    TEST_ASSERT_TRUE(IpValidator::isLoopback("127.255.255.255"));
}

void test_non_loopback() {
    TEST_ASSERT_FALSE(IpValidator::isLoopback("192.168.1.1"));
    TEST_ASSERT_FALSE(IpValidator::isLoopback("128.0.0.1"));
}

// ============================================================================
// Test: Full private IP validation (SSRF protection)
// ============================================================================

void test_valid_private_ip_accepted() {
    TEST_ASSERT_TRUE(IpValidator::isValidPrivateIp("192.168.1.1"));
    TEST_ASSERT_TRUE(IpValidator::isValidPrivateIp("10.0.0.1"));
    TEST_ASSERT_TRUE(IpValidator::isValidPrivateIp("172.16.0.1"));
}

void test_loopback_rejected() {
    TEST_ASSERT_FALSE(IpValidator::isValidPrivateIp("127.0.0.1"));
}

void test_broadcast_rejected() {
    TEST_ASSERT_FALSE(IpValidator::isValidPrivateIp("255.255.255.255"));
}

void test_any_address_rejected() {
    TEST_ASSERT_FALSE(IpValidator::isValidPrivateIp("0.0.0.0"));
}

void test_public_ip_rejected() {
    TEST_ASSERT_FALSE(IpValidator::isValidPrivateIp("8.8.8.8"));
    TEST_ASSERT_FALSE(IpValidator::isValidPrivateIp("1.1.1.1"));
    TEST_ASSERT_FALSE(IpValidator::isValidPrivateIp("142.250.185.78"));
}

void test_invalid_format_rejected() {
    TEST_ASSERT_FALSE(IpValidator::isValidPrivateIp(nullptr));
    TEST_ASSERT_FALSE(IpValidator::isValidPrivateIp(""));
    TEST_ASSERT_FALSE(IpValidator::isValidPrivateIp("not.an.ip.address"));
}

// ============================================================================
// Test: Edge cases
// ============================================================================

void test_boundary_172_range() {
    // Just inside
    TEST_ASSERT_TRUE(IpValidator::isValidPrivateIp("172.16.0.0"));
    TEST_ASSERT_TRUE(IpValidator::isValidPrivateIp("172.31.255.255"));
    
    // Just outside
    TEST_ASSERT_FALSE(IpValidator::isValidPrivateIp("172.15.255.255"));
    TEST_ASSERT_FALSE(IpValidator::isValidPrivateIp("172.32.0.0"));
}

void test_max_length_ip() {
    // Max valid: 255.255.255.255 = 15 chars
    TEST_ASSERT_TRUE(IpValidator::isValidFormat("255.255.255.255"));
    
    // Too long (>15 chars)
    TEST_ASSERT_FALSE(IpValidator::isValidFormat("1234.1234.1234.1234"));
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    UNITY_BEGIN();
    
    // Format validation
    RUN_TEST(test_valid_format_simple);
    RUN_TEST(test_invalid_format_null);
    RUN_TEST(test_invalid_format_empty);
    RUN_TEST(test_invalid_format_incomplete);
    RUN_TEST(test_invalid_format_garbage);
    RUN_TEST(test_invalid_format_extra_chars);
    RUN_TEST(test_invalid_format_octet_out_of_range);
    RUN_TEST(test_invalid_format_negative);
    
    // Private ranges
    RUN_TEST(test_private_range_10);
    RUN_TEST(test_private_range_172);
    RUN_TEST(test_private_range_192_168);
    RUN_TEST(test_public_ip_not_private);
    
    // Loopback
    RUN_TEST(test_loopback_detected);
    RUN_TEST(test_non_loopback);
    
    // Full validation (SSRF)
    RUN_TEST(test_valid_private_ip_accepted);
    RUN_TEST(test_loopback_rejected);
    RUN_TEST(test_broadcast_rejected);
    RUN_TEST(test_any_address_rejected);
    RUN_TEST(test_public_ip_rejected);
    RUN_TEST(test_invalid_format_rejected);
    
    // Edge cases
    RUN_TEST(test_boundary_172_range);
    RUN_TEST(test_max_length_ip);
    
    return UNITY_END();
}
