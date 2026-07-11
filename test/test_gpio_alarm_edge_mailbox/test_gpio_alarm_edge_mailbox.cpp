#include <unity.h>

#include "../../src/alarms/core/GpioAlarmEdgeMailbox.h"

using ALARMS::GpioAlarmEdgeMailbox;

void setUp() {}
void tearDown() {}

void test_complete_pulse_is_delivered_as_rising_then_clear() {
    GpioAlarmEdgeMailbox mailbox;
    TEST_ASSERT_TRUE(mailbox.submit("gpio1", true));
    TEST_ASSERT_TRUE(mailbox.submit("gpio1", false));

    GpioAlarmEdgeMailbox::PassSnapshot pass;
    bool value = false;
    TEST_ASSERT_TRUE(mailbox.peek(pass));
    TEST_ASSERT_TRUE(pass.valueFor("gpio1", value));
    TEST_ASSERT_TRUE(value);

    mailbox.complete(pass);
    TEST_ASSERT_TRUE(mailbox.peek(pass));
    TEST_ASSERT_TRUE(pass.valueFor("gpio1", value));
    TEST_ASSERT_FALSE(value);

    mailbox.complete(pass);
    TEST_ASSERT_FALSE(mailbox.hasPending());
}

void test_selectors_remain_independent_in_same_pass() {
    GpioAlarmEdgeMailbox mailbox;
    TEST_ASSERT_TRUE(mailbox.submit("gpio1", true));
    TEST_ASSERT_TRUE(mailbox.submit("gpio2", false));

    GpioAlarmEdgeMailbox::PassSnapshot pass;
    bool gpio1 = false;
    bool gpio2 = true;
    TEST_ASSERT_TRUE(mailbox.peek(pass));
    TEST_ASSERT_TRUE(pass.valueFor("gpio1", gpio1));
    TEST_ASSERT_TRUE(pass.valueFor("gpio2", gpio2));
    TEST_ASSERT_TRUE(gpio1);
    TEST_ASSERT_FALSE(gpio2);
    TEST_ASSERT_FALSE(pass.valueFor("gpio3", gpio2));
}

void test_failed_pass_remains_pending_until_acknowledged() {
    GpioAlarmEdgeMailbox mailbox;
    TEST_ASSERT_TRUE(mailbox.submit("gpio4", true));

    GpioAlarmEdgeMailbox::PassSnapshot first;
    GpioAlarmEdgeMailbox::PassSnapshot retry;
    bool firstValue = false;
    bool retryValue = false;
    TEST_ASSERT_TRUE(mailbox.peek(first));
    TEST_ASSERT_TRUE(first.valueFor("gpio4", firstValue));

    // No complete(first): model a coordinator/RTC transaction failure.
    TEST_ASSERT_TRUE(mailbox.peek(retry));
    TEST_ASSERT_TRUE(retry.valueFor("gpio4", retryValue));
    TEST_ASSERT_EQUAL(firstValue, retryValue);

    mailbox.complete(retry);
    TEST_ASSERT_FALSE(mailbox.hasPending());
}

void test_latest_true_suppresses_obsolete_clear_after_rising_pass() {
    GpioAlarmEdgeMailbox mailbox;
    TEST_ASSERT_TRUE(mailbox.submit("gpio5", true));
    TEST_ASSERT_TRUE(mailbox.submit("gpio5", false));
    TEST_ASSERT_TRUE(mailbox.submit("gpio5", true));

    GpioAlarmEdgeMailbox::PassSnapshot pass;
    bool value = false;
    TEST_ASSERT_TRUE(mailbox.peek(pass));
    TEST_ASSERT_TRUE(pass.valueFor("gpio5", value));
    TEST_ASSERT_TRUE(value);
    mailbox.complete(pass);

    TEST_ASSERT_FALSE(mailbox.hasPending());
}

void test_invalid_selector_is_rejected() {
    GpioAlarmEdgeMailbox mailbox;
    char unterminated[ALARMS::kGpioIdLen];
    std::memset(unterminated, 'x', sizeof(unterminated));

    TEST_ASSERT_FALSE(mailbox.submit(nullptr, true));
    TEST_ASSERT_FALSE(mailbox.submit("", true));
    TEST_ASSERT_FALSE(mailbox.submit(unterminated, true));
    TEST_ASSERT_FALSE(mailbox.hasPending());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_complete_pulse_is_delivered_as_rising_then_clear);
    RUN_TEST(test_selectors_remain_independent_in_same_pass);
    RUN_TEST(test_failed_pass_remains_pending_until_acknowledged);
    RUN_TEST(test_latest_true_suppresses_obsolete_clear_after_rising_pass);
    RUN_TEST(test_invalid_selector_is_rejected);
    return UNITY_END();
}
