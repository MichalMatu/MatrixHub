#include <unity.h>

#include "../../src/alarms/core/CsiAlarmEdgeLatch.h"

using ALARMS::CsiAlarmEdgeLatch;

void setUp(void) {}
void tearDown(void) {}

void test_true_then_false_preserves_rising_before_clear() {
    CsiAlarmEdgeLatch latch;
    latch.submit(true);
    latch.submit(false);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiAlarmEdgeLatch::PendingDecision::Rising),
        static_cast<uint8_t>(latch.next()));
    latch.complete(CsiAlarmEdgeLatch::PendingDecision::Rising);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiAlarmEdgeLatch::PendingDecision::Clear),
        static_cast<uint8_t>(latch.next()));
}

void test_true_false_true_converges_to_latest_true() {
    CsiAlarmEdgeLatch latch;
    latch.submit(true);
    latch.submit(false);
    latch.submit(true);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiAlarmEdgeLatch::PendingDecision::Rising),
        static_cast<uint8_t>(latch.next()));
    latch.complete(CsiAlarmEdgeLatch::PendingDecision::Rising);
    TEST_ASSERT_FALSE(latch.hasPending());
}

void test_failed_pass_is_retried_until_acknowledged() {
    CsiAlarmEdgeLatch latch;
    latch.submit(true);
    const auto pending = latch.next();

    // No complete(): this models a coordinator lock timeout.
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(pending),
        static_cast<uint8_t>(latch.next()));
    latch.complete(pending);
    TEST_ASSERT_FALSE(latch.hasPending());
}

void test_repeated_keepalive_does_not_create_fake_edge() {
    CsiAlarmEdgeLatch latch;
    latch.submit(true);
    latch.complete(latch.next());
    latch.submit(true);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiAlarmEdgeLatch::PendingDecision::None),
        static_cast<uint8_t>(latch.next()));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_true_then_false_preserves_rising_before_clear);
    RUN_TEST(test_true_false_true_converges_to_latest_true);
    RUN_TEST(test_failed_pass_is_retried_until_acknowledged);
    RUN_TEST(test_repeated_keepalive_does_not_create_fake_edge);
    return UNITY_END();
}
