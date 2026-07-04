/**
 * @file test_backoff.cpp
 * @brief Unit tests for Exponential Backoff Logic
 */

#ifdef NATIVE_BUILD
#include <unity.h>
#include "../../src/notifications/telegram/utils/BackoffCalculator.h"

using namespace TELEGRAM;

void setUp(void) {}
void tearDown(void) {}

void test_backoff_zero_errors() {
    uint32_t ms = BackoffCalculator::calculateMs(0, 5000);
    TEST_ASSERT_EQUAL(0, ms);
}

void test_backoff_growth() {
    uint32_t baseInterval = 5000; // 5s
    
    // 1 error -> exp = 1 -> (2^1 - 1) * 5000 = 1 * 5000 = 5000
    TEST_ASSERT_EQUAL(5000, BackoffCalculator::calculateMs(1, baseInterval));
    
    // 2 errors -> exp = 2 -> (2^2 - 1) * 5000 = 3 * 5000 = 15000
    TEST_ASSERT_EQUAL(15000, BackoffCalculator::calculateMs(2, baseInterval));
    
    // 3 errors -> exp = 3 -> (2^3 - 1) * 5000 = 7 * 5000 = 35000
    TEST_ASSERT_EQUAL(35000, BackoffCalculator::calculateMs(3, baseInterval));
    
    // 4 errors -> exp = 4 -> (2^4 - 1) * 5000 = 15 * 5000 = 75000
    TEST_ASSERT_EQUAL(75000, BackoffCalculator::calculateMs(4, baseInterval));
}

void test_backoff_exponent_cap() {
    uint32_t baseInterval = 5000;
    
    // The exponent is capped at TELEGRAM_MAX_BACKOFF_EXPONENT (5)
    // (2^5 - 1) * 5000 = 31 * 5000 = 155000 ms
    uint32_t ms = BackoffCalculator::calculateMs(50, baseInterval);
    
    TEST_ASSERT_EQUAL(155000, ms);
}

void test_backoff_hard_max_limit() {
    uint32_t baseInterval = 10000; // 10s. This helps hit the hard cap limit.
    
    // (2^5 - 1) * 10000 = 310000 ms -> should be capped at 300000 ms
    uint32_t ms = BackoffCalculator::calculateMs(50, baseInterval);
    
    TEST_ASSERT_EQUAL(APP::NOTIFICATIONS::TELEGRAM_MAX_BACKOFF_MS, ms);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_backoff_zero_errors);
    RUN_TEST(test_backoff_growth);
    RUN_TEST(test_backoff_exponent_cap);
    RUN_TEST(test_backoff_hard_max_limit);
    return UNITY_END();
}
#endif // NATIVE_BUILD
