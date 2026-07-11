#include <unity.h>

#include "../../src/wifisensing/csi/core/CsiRuntimeCleanupGate.h"

using WIFISENSING::CSI::canReleaseCsiRuntimeResources;

namespace {

void test_releases_only_after_every_runtime_owner_is_inactive() {
    TEST_ASSERT_TRUE(canReleaseCsiRuntimeResources(true, true, true));

    for (unsigned mask = 0; mask < 7; ++mask) {
        const bool callbackDetached = (mask & 0x1u) != 0;
        const bool callbacksDrained = (mask & 0x2u) != 0;
        const bool processingTaskStopped = (mask & 0x4u) != 0;
        TEST_ASSERT_FALSE(canReleaseCsiRuntimeResources(
            callbackDetached, callbacksDrained, processingTaskStopped));
    }
}

void test_detach_failure_blocks_release_even_when_no_callback_is_currently_running() {
    TEST_ASSERT_FALSE(canReleaseCsiRuntimeResources(false, true, true));
}

void test_drain_timeout_blocks_release_after_successful_detach() {
    TEST_ASSERT_FALSE(canReleaseCsiRuntimeResources(true, false, true));
}

} // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_releases_only_after_every_runtime_owner_is_inactive);
    RUN_TEST(test_detach_failure_blocks_release_even_when_no_callback_is_currently_running);
    RUN_TEST(test_drain_timeout_blocks_release_after_successful_detach);
    return UNITY_END();
}
