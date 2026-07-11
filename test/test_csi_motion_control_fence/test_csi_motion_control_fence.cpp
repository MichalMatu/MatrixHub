#include <unity.h>

#include "../../src/wifisensing/csi/core/CsiMotionControlFence.h"

using WIFISENSING::CSI::CsiMotionControlFence;
using WIFISENSING::CSI::CsiMotionSyncMailbox;

void setUp() {}
void tearDown() {}

void test_initial_epoch_is_publishable() {
    CsiMotionControlFence fence;
    TEST_ASSERT_EQUAL_UINT32(0, fence.requestedEpoch());
    TEST_ASSERT_TRUE(fence.canPublish(0));
}

void test_transition_rejects_old_and_new_epoch_until_applied() {
    CsiMotionControlFence fence;
    const uint32_t epoch = fence.beginTransition();

    TEST_ASSERT_EQUAL_UINT32(1, epoch);
    TEST_ASSERT_TRUE(fence.transitionInProgress());
    TEST_ASSERT_FALSE(fence.canPublish(0));
    TEST_ASSERT_FALSE(fence.canPublish(epoch));

    TEST_ASSERT_TRUE(fence.completeTransition(epoch));
    TEST_ASSERT_FALSE(fence.transitionInProgress());
    TEST_ASSERT_TRUE(fence.canPublish(epoch));
    TEST_ASSERT_FALSE(fence.canPublish(0));
}

void test_stale_completion_cannot_reopen_newer_transition() {
    CsiMotionControlFence fence;
    const uint32_t first = fence.beginTransition();
    const uint32_t second = fence.beginTransition();

    TEST_ASSERT_FALSE(fence.completeTransition(first));
    TEST_ASSERT_TRUE(fence.transitionInProgress());
    TEST_ASSERT_FALSE(fence.canPublish(first));
    TEST_ASSERT_FALSE(fence.canPublish(second));

    TEST_ASSERT_TRUE(fence.completeTransition(second));
    TEST_ASSERT_TRUE(fence.canPublish(second));
}

void test_motion_sync_mailbox_rejects_clear_from_older_epoch() {
    CsiMotionSyncMailbox mailbox;
    mailbox.queue(false, 4);

    bool value = true;
    TEST_ASSERT_FALSE(mailbox.takeForEpoch(5, value));
    TEST_ASSERT_TRUE(value);
    TEST_ASSERT_TRUE(mailbox.pending());

    mailbox.discard();
    TEST_ASSERT_FALSE(mailbox.pending());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_initial_epoch_is_publishable);
    RUN_TEST(test_transition_rejects_old_and_new_epoch_until_applied);
    RUN_TEST(test_stale_completion_cannot_reopen_newer_transition);
    RUN_TEST(test_motion_sync_mailbox_rejects_clear_from_older_epoch);
    return UNITY_END();
}
