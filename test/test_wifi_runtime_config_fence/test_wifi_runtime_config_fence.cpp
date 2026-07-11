#include <unity.h>

#include <atomic>
#include <thread>

#include "../../src/wifisensing/core/WifiAppliedRuntimeConfig.h"
#include "../../src/wifisensing/core/WifiRuntimeConfigFence.h"
#include "../../src/wifisensing/core/WifiRuntimeReconcileSchedule.h"
#include "../../src/wifisensing/core/WifiRuntimeReconcileWorkerGate.h"

void setUp(void) {}
void tearDown(void) {}

void test_snapshot_stays_current_without_config_update() {
    WIFISENSING::WifiRuntimeConfigFence fence;
    const uint32_t snapshot = fence.snapshot();

    TEST_ASSERT_TRUE(fence.isCurrent(snapshot));
}

void test_config_update_invalidates_reconcile_snapshot() {
    WIFISENSING::WifiRuntimeConfigFence fence;
    const uint32_t staleSnapshot = fence.snapshot();

    const uint32_t nextGeneration = fence.markConfigChanged();

    TEST_ASSERT_FALSE(fence.isCurrent(staleSnapshot));
    TEST_ASSERT_TRUE(fence.isCurrent(nextGeneration));
}

void test_each_update_advances_generation() {
    WIFISENSING::WifiRuntimeConfigFence fence;

    const uint32_t first = fence.markConfigChanged();
    const uint32_t second = fence.markConfigChanged();

    TEST_ASSERT_EQUAL_UINT32(first + 1, second);
    TEST_ASSERT_TRUE(fence.isCurrent(second));
}

void test_applied_runtime_detects_interval_and_threshold_drift() {
    WIFISENSING::WifiAppliedRuntimeConfig applied;
    applied.markEnabled(1000, 4.0f);

    TEST_ASSERT_FALSE(applied.needsReconcile(true, 1000, 4.0f, true, true));
    TEST_ASSERT_TRUE(applied.needsReconcile(true, 500, 4.0f, true, true));
    TEST_ASSERT_TRUE(applied.needsReconcile(true, 1000, 7.5f, true, true));
    TEST_ASSERT_TRUE(applied.needsReconcile(true, 1000, 4.0f, false, true));
}

void test_applied_runtime_keeps_failed_rollback_visible_to_reconciler() {
    WIFISENSING::WifiAppliedRuntimeConfig applied;
    applied.markEnabled(1000, 4.0f);

    // Runtime B reached the worker, but persistence later rolled desired state
    // back to A. A running-only health check would miss this mismatch forever.
    applied.markEnabled(250, 9.0f);
    TEST_ASSERT_TRUE(applied.needsReconcile(true, 1000, 4.0f, true, true));

    applied.markEnabled(1000, 4.0f);
    TEST_ASSERT_FALSE(applied.needsReconcile(true, 1000, 4.0f, true, true));
}

void test_disabled_runtime_still_reconciles_deferred_task_cleanup() {
    WIFISENSING::WifiAppliedRuntimeConfig applied;
    applied.markEnabled(1000, 4.0f);
    applied.markDisabled();

    TEST_ASSERT_TRUE(applied.needsReconcile(false, 1000, 4.0f, false, true));
    TEST_ASSERT_FALSE(applied.needsReconcile(false, 1000, 4.0f, false, false));
}

void test_applied_generation_advances_only_when_effective_snapshot_changes() {
    WIFISENSING::WifiAppliedRuntimeConfig applied;
    TEST_ASSERT_EQUAL_UINT32(0, applied.snapshot().generation);

    applied.markDisabled();
    TEST_ASSERT_EQUAL_UINT32(0, applied.snapshot().generation);

    applied.markEnabled(1000, 4.0f);
    TEST_ASSERT_EQUAL_UINT32(1, applied.snapshot().generation);
    applied.markEnabled(1000, 4.0f);
    TEST_ASSERT_EQUAL_UINT32(1, applied.snapshot().generation);

    applied.markEnabled(1000, 5.0f);
    TEST_ASSERT_EQUAL_UINT32(2, applied.snapshot().generation);
    applied.markDisabled();
    TEST_ASSERT_EQUAL_UINT32(3, applied.snapshot().generation);
    applied.markDisabled();
    TEST_ASSERT_EQUAL_UINT32(3, applied.snapshot().generation);
}

void test_reconcile_schedule_applies_bounded_exponential_backoff_and_reset() {
    WIFISENSING::WifiRuntimeReconcileSchedule schedule(5, 20);
    TEST_ASSERT_TRUE(schedule.isDue(100));

    schedule.markFailure(100);
    TEST_ASSERT_EQUAL_UINT32(105, schedule.nextAttemptMs());
    TEST_ASSERT_EQUAL_UINT32(10, schedule.backoffMs());
    TEST_ASSERT_FALSE(schedule.isDue(104));
    TEST_ASSERT_TRUE(schedule.isDue(105));

    schedule.markFailure(105);
    TEST_ASSERT_EQUAL_UINT32(115, schedule.nextAttemptMs());
    TEST_ASSERT_EQUAL_UINT32(20, schedule.backoffMs());
    schedule.markFailure(115);
    TEST_ASSERT_EQUAL_UINT32(135, schedule.nextAttemptMs());
    TEST_ASSERT_EQUAL_UINT32(20, schedule.backoffMs());

    schedule.markHealthy(135);
    TEST_ASSERT_EQUAL_UINT32(140, schedule.nextAttemptMs());
    TEST_ASSERT_EQUAL_UINT32(5, schedule.backoffMs());
}

void test_reconcile_schedule_deadline_is_wrap_safe_even_when_it_wraps_through_zero() {
    WIFISENSING::WifiRuntimeReconcileSchedule schedule(32, 128);
    schedule.markHealthy(0xFFFFFFF0u);

    TEST_ASSERT_EQUAL_UINT32(16, schedule.nextAttemptMs());
    TEST_ASSERT_FALSE(schedule.isDue(0xFFFFFFFFu));
    TEST_ASSERT_FALSE(schedule.isDue(15));
    TEST_ASSERT_TRUE(schedule.isDue(16));
}

void test_reconcile_backoff_is_anchored_to_attempt_completion() {
    WIFISENSING::WifiRuntimeReconcileSchedule schedule(5, 20);

    // The attempt started at 100 but did not finish until 140. Scheduling from
    // completion prevents an already-expired retry deadline after slow cleanup.
    schedule.markFailure(140);

    TEST_ASSERT_EQUAL_UINT32(145, schedule.nextAttemptMs());
    TEST_ASSERT_FALSE(schedule.isDue(144));
    TEST_ASSERT_TRUE(schedule.isDue(145));
}

void test_reconcile_worker_gate_is_single_flight() {
    WIFISENSING::WifiRuntimeReconcileWorkerGate gate;

    TEST_ASSERT_TRUE(gate.tryClaim());
    TEST_ASSERT_TRUE(gate.isWorkerRunning());
    TEST_ASSERT_FALSE(gate.tryClaim());

    gate.complete();
    TEST_ASSERT_FALSE(gate.isWorkerRunning());
    TEST_ASSERT_TRUE(gate.tryClaim());
    gate.complete();
}

void test_reconcile_worker_gate_rejects_work_after_terminal_shutdown() {
    WIFISENSING::WifiRuntimeReconcileWorkerGate gate;
    TEST_ASSERT_TRUE(gate.tryClaim());

    gate.requestShutdown();

    TEST_ASSERT_FALSE(gate.shouldRun());
    TEST_ASSERT_FALSE(gate.tryClaim());
    TEST_ASSERT_TRUE(gate.isWorkerRunning());
    gate.complete();
    TEST_ASSERT_FALSE(gate.isWorkerRunning());
    TEST_ASSERT_FALSE(gate.tryClaim());
}

void test_reconcile_worker_claim_shutdown_race_is_drained_or_rejected() {
    for (int iteration = 0; iteration < 1000; ++iteration) {
        WIFISENSING::WifiRuntimeReconcileWorkerGate gate;
        std::atomic<bool> go{false};
        std::atomic<bool> claimed{false};

        std::thread claimant([&]() {
            while (!go.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            claimed.store(gate.tryClaim(), std::memory_order_release);
        });

        go.store(true, std::memory_order_release);
        gate.requestShutdown();
        claimant.join();

        TEST_ASSERT_FALSE(gate.shouldRun());
        if (claimed.load(std::memory_order_acquire)) {
            TEST_ASSERT_TRUE(gate.isWorkerRunning());
            gate.complete();
        }
        TEST_ASSERT_FALSE(gate.isWorkerRunning());
        TEST_ASSERT_FALSE(gate.tryClaim());
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_snapshot_stays_current_without_config_update);
    RUN_TEST(test_config_update_invalidates_reconcile_snapshot);
    RUN_TEST(test_each_update_advances_generation);
    RUN_TEST(test_applied_runtime_detects_interval_and_threshold_drift);
    RUN_TEST(test_applied_runtime_keeps_failed_rollback_visible_to_reconciler);
    RUN_TEST(test_disabled_runtime_still_reconciles_deferred_task_cleanup);
    RUN_TEST(test_applied_generation_advances_only_when_effective_snapshot_changes);
    RUN_TEST(test_reconcile_schedule_applies_bounded_exponential_backoff_and_reset);
    RUN_TEST(test_reconcile_schedule_deadline_is_wrap_safe_even_when_it_wraps_through_zero);
    RUN_TEST(test_reconcile_backoff_is_anchored_to_attempt_completion);
    RUN_TEST(test_reconcile_worker_gate_is_single_flight);
    RUN_TEST(test_reconcile_worker_gate_rejects_work_after_terminal_shutdown);
    RUN_TEST(test_reconcile_worker_claim_shutdown_race_is_drained_or_rejected);
    return UNITY_END();
}
