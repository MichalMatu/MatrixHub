#include <unity.h>

#include "../../src/notifications/runtime/NotificationRuntimeReconciler.h"

using NOTIFICATIONS::NotificationRuntimeReconciler;
using NOTIFICATIONS::NotificationRuntimeSnapshot;
using NOTIFICATIONS::NotificationWorkerTransition;

void setUp(void) {}

void tearDown(void) {}

void test_all_channels_disabled_and_worker_stopped_requires_no_transition() {
    const NotificationRuntimeSnapshot snapshot{
        .telegramEnabled = false,
        .webhookEnabled = false,
        .pushoverEnabled = false,
        .workerRunning = false,
    };

    TEST_ASSERT_FALSE(NotificationRuntimeReconciler::shouldWorkerRun(snapshot));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NotificationWorkerTransition::None),
                          static_cast<int>(NotificationRuntimeReconciler::computeTransition(snapshot)));
}

void test_any_enabled_channel_starts_worker_when_stopped() {
    const NotificationRuntimeSnapshot snapshot{
        .telegramEnabled = false,
        .webhookEnabled = true,
        .pushoverEnabled = false,
        .workerRunning = false,
    };

    TEST_ASSERT_TRUE(NotificationRuntimeReconciler::shouldWorkerRun(snapshot));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NotificationWorkerTransition::Start),
                          static_cast<int>(NotificationRuntimeReconciler::computeTransition(snapshot)));
}

void test_running_worker_stays_running_when_any_channel_remains_enabled() {
    const NotificationRuntimeSnapshot snapshot{
        .telegramEnabled = false,
        .webhookEnabled = false,
        .pushoverEnabled = true,
        .workerRunning = true,
    };

    TEST_ASSERT_TRUE(NotificationRuntimeReconciler::shouldWorkerRun(snapshot));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NotificationWorkerTransition::None),
                          static_cast<int>(NotificationRuntimeReconciler::computeTransition(snapshot)));
}

void test_running_worker_stops_when_all_channels_are_disabled() {
    const NotificationRuntimeSnapshot snapshot{
        .telegramEnabled = false,
        .webhookEnabled = false,
        .pushoverEnabled = false,
        .workerRunning = true,
    };

    TEST_ASSERT_FALSE(NotificationRuntimeReconciler::shouldWorkerRun(snapshot));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NotificationWorkerTransition::Stop),
                          static_cast<int>(NotificationRuntimeReconciler::computeTransition(snapshot)));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_all_channels_disabled_and_worker_stopped_requires_no_transition);
    RUN_TEST(test_any_enabled_channel_starts_worker_when_stopped);
    RUN_TEST(test_running_worker_stays_running_when_any_channel_remains_enabled);
    RUN_TEST(test_running_worker_stops_when_all_channels_are_disabled);
    return UNITY_END();
}
