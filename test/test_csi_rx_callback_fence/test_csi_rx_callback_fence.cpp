#include <unity.h>

#include "../../src/wifisensing/csi/core/CsiRxCallbackFence.h"

using WIFISENSING::CSI::CsiRxCallbackFence;

void setUp() {}
void tearDown() {}

void test_lease_started_before_detach_keeps_owner_and_is_drainable() {
    CsiRxCallbackFence fence;
    int owner = 1;
    TEST_ASSERT_TRUE(fence.attach(&owner));

    void* leasedOwner = fence.enter();
    TEST_ASSERT_EQUAL_PTR(&owner, leasedOwner);
    fence.detach(&owner);
    TEST_ASSERT_TRUE(fence.hasInFlight());
    TEST_ASSERT_EQUAL_PTR(&owner, leasedOwner);
    fence.leave();

    TEST_ASSERT_FALSE(fence.hasInFlight());
}

void test_callback_entering_after_detach_never_observes_old_owner() {
    CsiRxCallbackFence fence;
    int owner = 1;
    TEST_ASSERT_TRUE(fence.attach(&owner));
    fence.detach(&owner);

    TEST_ASSERT_NULL(fence.enter());
    fence.leave();
}

void test_wrong_owner_cannot_detach_current_generation() {
    CsiRxCallbackFence fence;
    int owner = 1;
    int other = 2;
    TEST_ASSERT_TRUE(fence.attach(&owner));

    fence.detach(&other);
    TEST_ASSERT_EQUAL_PTR(&owner, fence.enter());
    fence.leave();
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_lease_started_before_detach_keeps_owner_and_is_drainable);
    RUN_TEST(test_callback_entering_after_detach_never_observes_old_owner);
    RUN_TEST(test_wrong_owner_cannot_detach_current_generation);
    return UNITY_END();
}
