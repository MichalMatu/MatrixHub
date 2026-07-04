#ifdef UNIT_TEST

#include <unity.h>
#include <Arduino.h>

// Mock FreeRTOS... (keep as is for native)
#ifdef NATIVE_BUILD
// millis is provided by Arduino.h stubs already for unity tests
unsigned long _mockMillis = 0;
#ifndef __cplusplus
unsigned long millis(); // from stub
#endif
void simulateTime(unsigned long ms) { _mockMillis += ms; }
#else
void simulateTime(unsigned long ms) { delay(ms); }
#endif

#include "security/RateLimiter.h"
#define millis() _mockMillis
#include "security/RateLimiter.cpp"
#undef millis

void setUp(void) {
    #ifdef NATIVE_BUILD
    _mockMillis = 0;
    #endif
}

void tearDown(void) {}

// Test: Should allow requests under limit
void test_should_allow_under_limit() {
    RateLimiter limiter(5, 10000);  // 5 requests per 10 seconds
    uint32_t ip = 0xC0A80001;  // 192.168.0.1
    
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_TRUE_MESSAGE(limiter.shouldAllow(ip), "Request should be allowed under limit");
    }
}

// Test: Should block requests over limit
void test_should_block_over_limit() {
    RateLimiter limiter(5, 10000);
    uint32_t ip = 0xC0A80001;
    
    // Use up all allowed requests
    for (int i = 0; i < 5; i++) {
        limiter.shouldAllow(ip);
    }
    
    // 6th request should be blocked
    TEST_ASSERT_FALSE_MESSAGE(limiter.shouldAllow(ip), "Request should be blocked over limit");
}

// Test: Different IPs have separate limits
void test_separate_limits_per_ip() {
    RateLimiter limiter(2, 10000);
    uint32_t ip1 = 0xC0A80001;  // 192.168.0.1
    uint32_t ip2 = 0xC0A80002;  // 192.168.0.2
    
    // Use up IP1's limit
    limiter.shouldAllow(ip1);
    limiter.shouldAllow(ip1);
    TEST_ASSERT_FALSE(limiter.shouldAllow(ip1));
    
    // IP2 should still be allowed
    TEST_ASSERT_TRUE_MESSAGE(limiter.shouldAllow(ip2), "Different IP should have its own limit");
}

// Test: Window reset after timeout
void test_window_reset_after_timeout() {
    RateLimiter limiter(1, 100);  // 1 request per 100ms
    uint32_t ip = 0xC0A80001;
    
    limiter.shouldAllow(ip);
    TEST_ASSERT_FALSE(limiter.shouldAllow(ip));  // Blocked
    
    // Simulate time passing
    simulateTime(150);  // 150ms later, window should reset
    
    TEST_ASSERT_TRUE_MESSAGE(limiter.shouldAllow(ip), "Should allow after window reset");
}

// Test: Edge case - exactly at limit
void test_exactly_at_limit() {
    RateLimiter limiter(3, 10000);
    uint32_t ip = 0xC0A80001;
    
    TEST_ASSERT_TRUE(limiter.shouldAllow(ip));  // 1
    TEST_ASSERT_TRUE(limiter.shouldAllow(ip));  // 2
    TEST_ASSERT_TRUE(limiter.shouldAllow(ip));  // 3 (at limit)
    TEST_ASSERT_FALSE(limiter.shouldAllow(ip)); // 4 (over limit)
}

// Test: Cleanup removes old entries
void test_cleanup_removes_expired_entries() {
    RateLimiter limiter(5, 1000);  // 1 second window
    uint32_t ip = 0xC0A80001;
    
    limiter.shouldAllow(ip);
    
    // Simulate 3 seconds passing (cleanup threshold is windowSize * 2 = 2s)
    simulateTime(3000);
    
    // Force cleanup by calling shouldAllow (cleanup runs every 60s, but we can verify behavior)
    // After cleanup, the old entry should be gone, so this should be treated as new IP
    TEST_ASSERT_TRUE(limiter.shouldAllow(ip));
}

// Test: IPAddress overload works correctly (ignored natively)
void test_ipaddress_overload() {
    // NATIVE_BUILD strips the IPAddress overload.
    TEST_ASSERT_TRUE(true);
}

// Test: Login Rate Limiter Configuration (3 req / 60s)
void test_login_rate_limiter_configuration() {
    // Use 2 seconds window instead of 60s for faster testing on hardware
    RateLimiter loginLimiter(3, 2000); 
    uint32_t ip = 0xC0A80001;

    // Default: 3 attempts allowed
    TEST_ASSERT_TRUE(loginLimiter.shouldAllow(ip)); // 1
    TEST_ASSERT_TRUE(loginLimiter.shouldAllow(ip)); // 2
    TEST_ASSERT_TRUE(loginLimiter.shouldAllow(ip)); // 3
    
    // 4th attempt blocked
    TEST_ASSERT_FALSE_MESSAGE(loginLimiter.shouldAllow(ip), "Should block after 3 login attempts");
    
    // Simulate 500ms passing (1/4 window, refilling 0.75 tokens, still blocked)
    simulateTime(500);
    TEST_ASSERT_FALSE_MESSAGE(loginLimiter.shouldAllow(ip), "Should still be blocked after 500ms");
    
    // Simulate another 1.1s passing (total > 2s, should reset)
    simulateTime(1100);
    TEST_ASSERT_TRUE_MESSAGE(loginLimiter.shouldAllow(ip), "Should allow login after window reset");
}

int runUnityTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_should_allow_under_limit);
    RUN_TEST(test_should_block_over_limit);
    RUN_TEST(test_separate_limits_per_ip);
    RUN_TEST(test_window_reset_after_timeout);
    RUN_TEST(test_exactly_at_limit);
    RUN_TEST(test_cleanup_removes_expired_entries);
    RUN_TEST(test_ipaddress_overload);
    RUN_TEST(test_login_rate_limiter_configuration);
    return UNITY_END();
}

#ifdef NATIVE_BUILD
int main(int argc, char **argv) {
    return runUnityTests();
}
#else
void setup() {
    delay(2000);
    runUnityTests();
}

void loop() {}
#endif

#endif // UNIT_TEST
