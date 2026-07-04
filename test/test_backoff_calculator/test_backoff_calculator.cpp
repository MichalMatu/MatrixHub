/**
 * @file test_backoff_calculator.cpp
 * @brief Unit tests for BackoffCalculator - self-contained native build
 */

#include <unity.h>
#include <cstdint>

// ============================================================================
// Inline copy of BackoffCalculator for native testing
// ============================================================================

namespace WIFI_UTILS {

uint32_t calculateBackoffDelay(uint8_t level, uint32_t baseDelayMs, uint32_t maxDelayMs) {
    if (baseDelayMs == 0) return 0;
    if (maxDelayMs == 0) return baseDelayMs;
    
    if (level >= 32) {
        return maxDelayMs;
    }
    
    uint32_t delay = baseDelayMs << level;
    
    if (delay < baseDelayMs) {
        return maxDelayMs;
    }
    
    return (delay > maxDelayMs) ? maxDelayMs : delay;
}

uint8_t nextBackoffLevel(uint8_t currentLevel, uint8_t maxLevel = 4) {
    if (currentLevel >= maxLevel) {
        return maxLevel;
    }
    return currentLevel + 1;
}

}  // namespace WIFI_UTILS

using namespace WIFI_UTILS;

// ============================================================================
// Test fixtures
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// calculateBackoffDelay tests
// ============================================================================

void test_backoff_level_0_returns_base() {
    TEST_ASSERT_EQUAL(30000, calculateBackoffDelay(0, 30000, 300000));
}

void test_backoff_level_1_doubles() {
    TEST_ASSERT_EQUAL(60000, calculateBackoffDelay(1, 30000, 300000));
}

void test_backoff_level_2_quadruples() {
    TEST_ASSERT_EQUAL(120000, calculateBackoffDelay(2, 30000, 300000));
}

void test_backoff_level_3_octuples() {
    TEST_ASSERT_EQUAL(240000, calculateBackoffDelay(3, 30000, 300000));
}

void test_backoff_caps_at_max() {
    TEST_ASSERT_EQUAL(300000, calculateBackoffDelay(4, 30000, 300000));
    TEST_ASSERT_EQUAL(300000, calculateBackoffDelay(10, 30000, 300000));
}

void test_backoff_handles_zero_base() {
    TEST_ASSERT_EQUAL(0, calculateBackoffDelay(0, 0, 300000));
}

void test_backoff_handles_zero_max() {
    TEST_ASSERT_EQUAL(30000, calculateBackoffDelay(0, 30000, 0));
}

void test_backoff_overflow_protection() {
    TEST_ASSERT_EQUAL(300000, calculateBackoffDelay(32, 30000, 300000));
    TEST_ASSERT_EQUAL(300000, calculateBackoffDelay(255, 30000, 300000));
}

// ============================================================================
// nextBackoffLevel tests
// ============================================================================

void test_next_level_increments() {
    TEST_ASSERT_EQUAL(1, nextBackoffLevel(0, 4));
    TEST_ASSERT_EQUAL(2, nextBackoffLevel(1, 4));
    TEST_ASSERT_EQUAL(3, nextBackoffLevel(2, 4));
}

void test_next_level_caps_at_max() {
    TEST_ASSERT_EQUAL(4, nextBackoffLevel(4, 4));
    TEST_ASSERT_EQUAL(4, nextBackoffLevel(10, 4));
}

void test_next_level_custom_max() {
    TEST_ASSERT_EQUAL(2, nextBackoffLevel(2, 2));
    TEST_ASSERT_EQUAL(6, nextBackoffLevel(5, 6));
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    UNITY_BEGIN();
    
    RUN_TEST(test_backoff_level_0_returns_base);
    RUN_TEST(test_backoff_level_1_doubles);
    RUN_TEST(test_backoff_level_2_quadruples);
    RUN_TEST(test_backoff_level_3_octuples);
    RUN_TEST(test_backoff_caps_at_max);
    RUN_TEST(test_backoff_handles_zero_base);
    RUN_TEST(test_backoff_handles_zero_max);
    RUN_TEST(test_backoff_overflow_protection);
    RUN_TEST(test_next_level_increments);
    RUN_TEST(test_next_level_caps_at_max);
    RUN_TEST(test_next_level_custom_max);
    
    return UNITY_END();
}
