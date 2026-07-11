#include <unity.h>

#include "../../src/api/wifisensing/CsiCaptureAdmission.h"
#include "../../src/wifisensing/csi/core/CsiRuntimeReadiness.h"

void setUp(void) {}
void tearDown(void) {}

void test_runtime_readiness_requires_every_live_source_component() {
    using WIFISENSING::CSI::isCsiRuntimeReadyState;

    TEST_ASSERT_TRUE(isCsiRuntimeReadyState(
        true, true, true, false, true, true, true));
    TEST_ASSERT_FALSE(isCsiRuntimeReadyState(
        false, true, true, false, true, true, true));
    TEST_ASSERT_FALSE(isCsiRuntimeReadyState(
        true, false, true, false, true, true, true));
    TEST_ASSERT_FALSE(isCsiRuntimeReadyState(
        true, true, false, false, true, true, true));
    TEST_ASSERT_FALSE(isCsiRuntimeReadyState(
        true, true, true, true, true, true, true));
    TEST_ASSERT_FALSE(isCsiRuntimeReadyState(
        true, true, true, false, false, true, true));
    TEST_ASSERT_FALSE(isCsiRuntimeReadyState(
        true, true, true, false, true, false, true));
    TEST_ASSERT_FALSE(isCsiRuntimeReadyState(
        true, true, true, false, true, true, false));
}

void test_capture_rejects_desired_bit_and_retained_queue_without_runtime() {
    TEST_ASSERT_FALSE(API::isCsiCaptureStartReady(
        false, true, false, false, true, true));
    TEST_ASSERT_FALSE(API::isCsiCaptureStartReady(
        true, true, false, false, true, true));
    TEST_ASSERT_FALSE(API::isCsiCaptureStartReady(
        true, true, true, false, true, true));
}

void test_capture_accepts_only_fully_ready_runtime() {
    TEST_ASSERT_TRUE(API::isCsiCaptureStartReady(
        true, true, true, true, true, true));
    TEST_ASSERT_FALSE(API::isCsiCaptureStartReady(
        true, false, true, true, true, true));
    TEST_ASSERT_FALSE(API::isCsiCaptureStartReady(
        true, true, true, true, false, true));
    TEST_ASSERT_FALSE(API::isCsiCaptureStartReady(
        true, true, true, true, true, false));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_runtime_readiness_requires_every_live_source_component);
    RUN_TEST(test_capture_rejects_desired_bit_and_retained_queue_without_runtime);
    RUN_TEST(test_capture_accepts_only_fully_ready_runtime);
    return UNITY_END();
}
