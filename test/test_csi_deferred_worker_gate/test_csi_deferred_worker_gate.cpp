#include <unity.h>

#include <atomic>
#include <thread>

#include "../../src/api/wifisensing/CsiCaptureCleanupGate.h"
#include "../../src/api/wifisensing/CsiDeferredWorkerGate.h"

void setUp(void) {}
void tearDown(void) {}

void test_request_remains_pending_until_completed() {
    API::CsiDeferredWorkerGate gate;

    const uint32_t generation = gate.request();

    TEST_ASSERT_TRUE(gate.hasPendingRequest());
    TEST_ASSERT_TRUE(gate.tryClaimWorker());
    TEST_ASSERT_FALSE(gate.tryClaimWorker());

    gate.completeThrough(generation);
    gate.releaseWorker();

    TEST_ASSERT_FALSE(gate.hasPendingRequest());
    TEST_ASSERT_FALSE(gate.isWorkerRunning());
}

void test_creation_failure_release_preserves_request_for_retry() {
    API::CsiDeferredWorkerGate gate;
    const uint32_t generation = gate.request();
    TEST_ASSERT_TRUE(gate.tryClaimWorker());

    gate.releaseWorker();

    TEST_ASSERT_TRUE(gate.hasPendingRequest());
    TEST_ASSERT_TRUE(gate.tryClaimWorker());
    gate.completeThrough(generation);
    gate.releaseWorker();
    TEST_ASSERT_FALSE(gate.hasPendingRequest());
}

void test_failed_terminal_step_keeps_request_pending_for_retry() {
    API::CsiDeferredWorkerGate gate;
    const uint32_t generation = gate.request();
    TEST_ASSERT_TRUE(gate.tryClaimWorker());

    TEST_ASSERT_FALSE(gate.completeCurrentIf(generation, false));
    TEST_ASSERT_TRUE(gate.hasPendingRequest());
    TEST_ASSERT_TRUE(gate.isWorkerRunning());

    TEST_ASSERT_TRUE(gate.completeCurrentIf(generation, true));
    gate.releaseWorker();
    TEST_ASSERT_FALSE(gate.hasPendingRequest());
}

void test_superseded_worker_cannot_complete_newer_request() {
    API::CsiDeferredWorkerGate gate;
    const uint32_t first = gate.request();
    TEST_ASSERT_TRUE(gate.tryClaimWorker());
    const uint32_t second = gate.request();

    TEST_ASSERT_FALSE(gate.completeCurrentIf(first, true));
    TEST_ASSERT_TRUE(gate.hasPendingRequest());
    TEST_ASSERT_TRUE(gate.completeCurrentIf(second, true));
    gate.releaseWorker();
}

void test_worker_reclaims_request_that_arrives_before_exit() {
    API::CsiDeferredWorkerGate gate;
    const uint32_t first = gate.request();
    TEST_ASSERT_TRUE(gate.tryClaimWorker());
    gate.completeThrough(first);

    const uint32_t second = gate.request();

    TEST_ASSERT_TRUE(gate.releaseWorkerAndTryReclaim());
    TEST_ASSERT_TRUE(gate.isWorkerRunning());
    TEST_ASSERT_TRUE(gate.hasPendingRequest());
    gate.completeThrough(second);
    gate.releaseWorker();
}

void test_request_exit_race_is_owned_or_remains_claimable() {
    for (int iteration = 0; iteration < 2000; ++iteration) {
        API::CsiDeferredWorkerGate gate;
        const uint32_t first = gate.request();
        TEST_ASSERT_TRUE(gate.tryClaimWorker());
        gate.completeThrough(first);

        std::atomic<bool> go{false};
        std::thread requester([&]() {
            while (!go.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            (void)gate.request();
        });

        go.store(true, std::memory_order_release);
        const bool reclaimed = gate.releaseWorkerAndTryReclaim();
        requester.join();

        if (reclaimed) {
            gate.completeThrough(gate.requestedGeneration());
            gate.releaseWorker();
        } else if (gate.hasPendingRequest()) {
            TEST_ASSERT_TRUE(gate.tryClaimWorker());
            gate.completeThrough(gate.requestedGeneration());
            gate.releaseWorker();
        }

        TEST_ASSERT_FALSE(gate.hasPendingRequest());
        TEST_ASSERT_FALSE(gate.isWorkerRunning());
    }
}

void test_capture_cleanup_timeout_remains_durable() {
    API::CsiCaptureCleanupGate gate;
    const uint32_t generation = gate.requestIfOwned(17, 41, 17, 41);
    API::CsiCaptureCleanupRequest request;

    TEST_ASSERT_TRUE(gate.snapshot(request));
    TEST_ASSERT_EQUAL_UINT32(generation, request.requestGeneration);
    TEST_ASSERT_EQUAL_INT(17, request.fd);
    TEST_ASSERT_EQUAL_UINT32(41, request.sessionGeneration);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(API::CsiCaptureCleanupResolution::Retry),
        static_cast<int>(API::CsiCaptureCleanupGate::resolve(
            request, false, 17, 41)));
    TEST_ASSERT_TRUE(gate.hasPendingRequest());
}

void test_capture_cleanup_releases_only_owned_session_generation() {
    API::CsiCaptureCleanupGate gate;
    (void)gate.requestIfOwned(17, 41, 17, 41);
    API::CsiCaptureCleanupRequest request;
    TEST_ASSERT_TRUE(gate.snapshot(request));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(API::CsiCaptureCleanupResolution::ReleaseOwnedSession),
        static_cast<int>(API::CsiCaptureCleanupGate::resolve(
            request, true, 17, 41)));
    TEST_ASSERT_TRUE(gate.completeIfCurrent(request.requestGeneration));
    TEST_ASSERT_FALSE(gate.hasPendingRequest());
}

void test_capture_cleanup_cannot_release_reused_fd() {
    API::CsiCaptureCleanupGate gate;
    (void)gate.requestIfOwned(17, 41, 17, 41);
    API::CsiCaptureCleanupRequest request;
    TEST_ASSERT_TRUE(gate.snapshot(request));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(API::CsiCaptureCleanupResolution::Obsolete),
        static_cast<int>(API::CsiCaptureCleanupGate::resolve(
            request, true, 17, 42)));
}

void test_newer_capture_cleanup_supersedes_old_completion() {
    API::CsiCaptureCleanupGate gate;
    const uint32_t firstGeneration =
        gate.requestIfOwned(17, 41, 17, 41);
    const uint32_t secondGeneration =
        gate.requestIfOwned(18, 42, 18, 42);

    TEST_ASSERT_FALSE(gate.completeIfCurrent(firstGeneration));
    TEST_ASSERT_TRUE(gate.hasPendingRequest());

    API::CsiCaptureCleanupRequest request;
    TEST_ASSERT_TRUE(gate.snapshot(request));
    TEST_ASSERT_EQUAL_UINT32(secondGeneration, request.requestGeneration);
    TEST_ASSERT_EQUAL_INT(18, request.fd);
    TEST_ASSERT_TRUE(gate.completeIfCurrent(secondGeneration));
    TEST_ASSERT_FALSE(gate.hasPendingRequest());
}

void test_capture_cleanup_reconciles_already_released_session() {
    API::CsiCaptureCleanupGate gate;
    (void)gate.requestIfOwned(17, 41, 17, 41);
    API::CsiCaptureCleanupRequest request;
    TEST_ASSERT_TRUE(gate.snapshot(request));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(API::CsiCaptureCleanupResolution::EnsureInactive),
        static_cast<int>(API::CsiCaptureCleanupGate::resolve(
            request, true, -1, 41)));
}

void test_non_owner_cleanup_cannot_replace_pending_owner() {
    API::CsiCaptureCleanupGate gate;
    const uint32_t ownerRequest =
        gate.requestIfOwned(17, 41, 17, 41);

    TEST_ASSERT_EQUAL_UINT32(
        0, gate.requestIfOwned(18, 41, 17, 41));

    API::CsiCaptureCleanupRequest request;
    TEST_ASSERT_TRUE(gate.snapshot(request));
    TEST_ASSERT_EQUAL_UINT32(ownerRequest, request.requestGeneration);
    TEST_ASSERT_EQUAL_INT(17, request.fd);
    TEST_ASSERT_EQUAL_UINT32(41, request.sessionGeneration);
}

void test_stale_generation_cleanup_cannot_replace_current_owner() {
    API::CsiCaptureCleanupGate gate;

    TEST_ASSERT_EQUAL_UINT32(
        0, gate.requestIfOwned(17, 41, 17, 42));
    TEST_ASSERT_FALSE(gate.hasPendingRequest());
}

void test_repeated_owner_cleanup_is_idempotent() {
    API::CsiCaptureCleanupGate gate;
    const uint32_t first = gate.requestIfOwned(17, 41, 17, 41);
    const uint32_t repeated = gate.requestIfOwned(17, 41, 17, 41);

    TEST_ASSERT_EQUAL_UINT32(first, repeated);
    TEST_ASSERT_EQUAL_UINT32(first, gate.requestedGeneration());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_request_remains_pending_until_completed);
    RUN_TEST(test_creation_failure_release_preserves_request_for_retry);
    RUN_TEST(test_failed_terminal_step_keeps_request_pending_for_retry);
    RUN_TEST(test_superseded_worker_cannot_complete_newer_request);
    RUN_TEST(test_worker_reclaims_request_that_arrives_before_exit);
    RUN_TEST(test_request_exit_race_is_owned_or_remains_claimable);
    RUN_TEST(test_capture_cleanup_timeout_remains_durable);
    RUN_TEST(test_capture_cleanup_releases_only_owned_session_generation);
    RUN_TEST(test_capture_cleanup_cannot_release_reused_fd);
    RUN_TEST(test_newer_capture_cleanup_supersedes_old_completion);
    RUN_TEST(test_capture_cleanup_reconciles_already_released_session);
    RUN_TEST(test_non_owner_cleanup_cannot_replace_pending_owner);
    RUN_TEST(test_stale_generation_cleanup_cannot_replace_current_owner);
    RUN_TEST(test_repeated_owner_cleanup_is_idempotent);
    return UNITY_END();
}
