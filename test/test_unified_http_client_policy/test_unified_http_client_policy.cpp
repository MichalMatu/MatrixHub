#include <unity.h>

#include "../../src/system/network/UnifiedHttpClientPolicy.h"

using NETWORK::shouldCloseConnectionAfterSuccess;

void test_keeps_connection_only_when_success_and_body_consumed() {
    TEST_ASSERT_FALSE(shouldCloseConnectionAfterSuccess(true, true, true));
}

void test_closes_connection_when_response_body_not_consumed() {
    TEST_ASSERT_TRUE(shouldCloseConnectionAfterSuccess(true, true, false));
}

void test_closes_connection_when_reuse_disabled() {
    TEST_ASSERT_TRUE(shouldCloseConnectionAfterSuccess(true, false, true));
}

void test_closes_connection_when_read_failed() {
    TEST_ASSERT_TRUE(shouldCloseConnectionAfterSuccess(false, true, true));
    TEST_ASSERT_TRUE(shouldCloseConnectionAfterSuccess(false, false, false));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_keeps_connection_only_when_success_and_body_consumed);
    RUN_TEST(test_closes_connection_when_response_body_not_consumed);
    RUN_TEST(test_closes_connection_when_reuse_disabled);
    RUN_TEST(test_closes_connection_when_read_failed);
    return UNITY_END();
}
