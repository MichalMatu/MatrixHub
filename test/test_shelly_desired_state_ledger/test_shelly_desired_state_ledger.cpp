#include <unity.h>

#include "../../src/shelly/worker/ShellyDesiredStateLedger.h"
#include "../../src/shelly/ShellyPeerRevision.h"
#include "../../src/shelly/worker/ShellyWorkerLifecycleState.h"
#include "../../src/shelly/worker/ShellyStartRetrySchedule.h"

using SHELLY::ShellyDesiredStateCompletion;
using SHELLY::ShellyDesiredStateLease;
using SHELLY::ShellyDesiredStateLedger;
using SHELLY::ShellyWorkerLifecycleState;
using SHELLY::ShellyStartRetrySchedule;

void setUp(void) {}
void tearDown(void) {}

void test_duplicate_pending_state_is_coalesced() {
    ShellyDesiredStateLedger ledger;
    uint32_t firstGeneration = 0;
    uint32_t duplicateGeneration = 0;

    TEST_ASSERT_TRUE(ledger.upsert("relay-a", true, 10, &firstGeneration));
    TEST_ASSERT_TRUE(ledger.upsert("relay-a", true, 20, &duplicateGeneration));

    TEST_ASSERT_EQUAL_UINT32(firstGeneration, duplicateGeneration);
    TEST_ASSERT_EQUAL_UINT32(1, ledger.pendingCount());

    ShellyDesiredStateLease lease;
    TEST_ASSERT_TRUE(ledger.beginNextReady(20, lease));
    TEST_ASSERT_EQUAL_STRING("relay-a", lease.id);
    TEST_ASSERT_TRUE(lease.value);
    TEST_ASSERT_EQUAL_UINT32(firstGeneration, lease.generation);
}

void test_many_coalesced_wake_hints_preserve_final_off() {
    ShellyDesiredStateLedger ledger;

    // A real worker wake queue has one slot. Submitting more transitions than
    // it can signal must only replace this per-device ledger entry.
    for (uint32_t i = 0; i < 32; ++i) {
        const bool desired = (i % 2) == 0;
        TEST_ASSERT_TRUE(ledger.upsert("relay-a", desired, i));
    }

    TEST_ASSERT_EQUAL_UINT32(1, ledger.pendingCount());
    ShellyDesiredStateLease lease;
    TEST_ASSERT_TRUE(ledger.beginNextReady(32, lease));
    TEST_ASSERT_EQUAL_STRING("relay-a", lease.id);
    TEST_ASSERT_FALSE(lease.value);
}

void test_stale_success_cannot_ack_newer_off() {
    ShellyDesiredStateLedger ledger;
    uint32_t onGeneration = 0;
    uint32_t offGeneration = 0;

    TEST_ASSERT_TRUE(ledger.upsert("relay-a", true, 0, &onGeneration));
    ShellyDesiredStateLease inFlightOn;
    TEST_ASSERT_TRUE(ledger.beginNextReady(0, inFlightOn));

    TEST_ASSERT_TRUE(ledger.upsert("relay-a", false, 50, &offGeneration));
    TEST_ASSERT_NOT_EQUAL(onGeneration, offGeneration);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ShellyDesiredStateCompletion::Superseded),
        static_cast<int>(ledger.complete("relay-a", onGeneration, true, 100)));

    ShellyDesiredStateLease latest;
    TEST_ASSERT_TRUE(ledger.beginNextReady(100, latest));
    TEST_ASSERT_FALSE(latest.value);
    TEST_ASSERT_EQUAL_UINT32(offGeneration, latest.generation);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ShellyDesiredStateCompletion::Applied),
        static_cast<int>(ledger.complete("relay-a", offGeneration, true, 101)));
    TEST_ASSERT_EQUAL_UINT32(0, ledger.pendingCount());
}

void test_stale_failure_does_not_back_off_newer_state() {
    ShellyDesiredStateLedger ledger;
    uint32_t onGeneration = 0;

    TEST_ASSERT_TRUE(ledger.upsert("relay-a", true, 0, &onGeneration));
    ShellyDesiredStateLease inFlightOn;
    TEST_ASSERT_TRUE(ledger.beginNextReady(0, inFlightOn));
    TEST_ASSERT_TRUE(ledger.upsert("relay-a", false, 10));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ShellyDesiredStateCompletion::Superseded),
        static_cast<int>(ledger.complete("relay-a", onGeneration, false, 20)));

    ShellyDesiredStateLease latest;
    TEST_ASSERT_TRUE(ledger.beginNextReady(20, latest));
    TEST_ASSERT_FALSE(latest.value);
}

void test_failures_follow_fast_backoff_then_recovery_cadence() {
    ShellyDesiredStateLedger ledger;
    TEST_ASSERT_TRUE(ledger.upsert("relay-a", true, 0));

    ShellyDesiredStateLease lease;
    TEST_ASSERT_TRUE(ledger.beginNextReady(0, lease));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ShellyDesiredStateCompletion::RetryScheduled),
        static_cast<int>(ledger.complete("relay-a", lease.generation, false, 100)));
    TEST_ASSERT_FALSE(ledger.beginNextReady(299, lease));
    TEST_ASSERT_TRUE(ledger.beginNextReady(300, lease));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ShellyDesiredStateCompletion::RetryScheduled),
        static_cast<int>(ledger.complete("relay-a", lease.generation, false, 300)));
    TEST_ASSERT_FALSE(ledger.beginNextReady(699, lease));
    TEST_ASSERT_TRUE(ledger.beginNextReady(700, lease));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ShellyDesiredStateCompletion::RetryScheduled),
        static_cast<int>(ledger.complete("relay-a", lease.generation, false, 700)));
    TEST_ASSERT_FALSE(ledger.beginNextReady(1499, lease));
    TEST_ASSERT_TRUE(ledger.beginNextReady(1500, lease));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ShellyDesiredStateCompletion::RetryScheduled),
        static_cast<int>(ledger.complete("relay-a", lease.generation, false, 1500)));
    TEST_ASSERT_FALSE(ledger.beginNextReady(31499, lease));
    TEST_ASSERT_TRUE(ledger.beginNextReady(31500, lease));
}

void test_repeated_transport_allocation_failures_never_drop_intent() {
    ShellyDesiredStateLedger ledger;
    TEST_ASSERT_TRUE(ledger.upsert("relay-a", true, 0));

    uint32_t now = 0;
    for (uint8_t failure = 0; failure < 12; ++failure) {
        ShellyDesiredStateLease lease;
        TEST_ASSERT_TRUE(ledger.beginNextReady(now, lease));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(ShellyDesiredStateCompletion::RetryScheduled),
            static_cast<int>(ledger.complete(
                lease.id, lease.generation, false, now)));
        TEST_ASSERT_EQUAL_UINT32(1, ledger.pendingCount());
        now += ShellyDesiredStateLedger::retryDelayMs(failure + 1);
    }

    ShellyDesiredStateLease recovered;
    TEST_ASSERT_TRUE(ledger.beginNextReady(now, recovered));
    TEST_ASSERT_TRUE(recovered.value);
}

void test_same_state_can_be_reasserted_after_success() {
    ShellyDesiredStateLedger ledger;
    uint32_t firstGeneration = 0;
    uint32_t secondGeneration = 0;

    TEST_ASSERT_TRUE(ledger.upsert("relay-a", false, 0, &firstGeneration));
    ShellyDesiredStateLease lease;
    TEST_ASSERT_TRUE(ledger.beginNextReady(0, lease));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ShellyDesiredStateCompletion::Applied),
        static_cast<int>(ledger.complete("relay-a", lease.generation, true, 1)));

    TEST_ASSERT_TRUE(ledger.upsert("relay-a", false, 2, &secondGeneration));
    TEST_ASSERT_NOT_EQUAL(firstGeneration, secondGeneration);
    TEST_ASSERT_TRUE(ledger.beginNextReady(2, lease));
    TEST_ASSERT_FALSE(lease.value);
}

void test_peer_revision_change_supersedes_old_ack_and_retries_same_intent() {
    ShellyDesiredStateLedger ledger;
    constexpr uint64_t oldPeer = 0x1111ULL;
    constexpr uint64_t newPeer = 0x2222ULL;
    uint32_t oldGeneration = 0;

    TEST_ASSERT_TRUE(ledger.upsert(
        "relay-a", true, oldPeer, 0, &oldGeneration));
    ShellyDesiredStateLease oldLease;
    TEST_ASSERT_TRUE(ledger.beginNextReady(0, oldLease));
    TEST_ASSERT_EQUAL_UINT64(oldPeer, oldLease.peerRevision);

    TEST_ASSERT_TRUE(ledger.rebind("relay-a", newPeer, 10));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ShellyDesiredStateCompletion::Superseded),
        static_cast<int>(ledger.complete(
            "relay-a", oldGeneration, true, 20)));

    ShellyDesiredStateLease newLease;
    TEST_ASSERT_TRUE(ledger.beginNextReady(20, newLease));
    TEST_ASSERT_TRUE(newLease.value);
    TEST_ASSERT_EQUAL_UINT64(newPeer, newLease.peerRevision);
    TEST_ASSERT_NOT_EQUAL(oldGeneration, newLease.generation);
}

void test_remove_during_in_flight_command_frees_slot_and_ignores_old_ack() {
    ShellyDesiredStateLedger ledger;
    ShellyDesiredStateLease removedLease;
    TEST_ASSERT_TRUE(ledger.upsert("removed", true, 1, 0, nullptr));
    TEST_ASSERT_TRUE(ledger.beginNextReady(0, removedLease));
    TEST_ASSERT_TRUE(ledger.remove("removed"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ShellyDesiredStateCompletion::Ignored),
        static_cast<int>(ledger.complete(
            "removed", removedLease.generation, true, 1)));

    // A removed/disabled in-flight device cannot remain as a zombie and
    // consume one of the four fixed ledger slots.
    for (size_t i = 0; i < SHELLY::kMaxDevices; ++i) {
        char id[16];
        snprintf(id, sizeof(id), "relay-%u", static_cast<unsigned>(i));
        TEST_ASSERT_TRUE(ledger.upsert(id, false, i + 2));
    }
    TEST_ASSERT_EQUAL_UINT32(SHELLY::kMaxDevices, ledger.pendingCount());
}

void test_busy_device_lookup_retries_instead_of_deleting_intent() {
    ShellyDesiredStateLedger ledger;
    TEST_ASSERT_TRUE(ledger.upsert(
        "relay-busy", false, static_cast<uint64_t>(77), 0));

    ShellyDesiredStateLease lease;
    TEST_ASSERT_TRUE(ledger.beginNextReady(0, lease));

    // Models DeviceManager::lookupDevice returning Busy before or after HTTP.
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ShellyDesiredStateCompletion::RetryScheduled),
        static_cast<int>(ledger.complete(
            lease.id, lease.generation, false, 10)));
    TEST_ASSERT_EQUAL_UINT32(1, ledger.pendingCount());
    TEST_ASSERT_FALSE(ledger.beginNextReady(209, lease));
    TEST_ASSERT_TRUE(ledger.beginNextReady(210, lease));
    TEST_ASSERT_FALSE(lease.value);
}

void test_old_not_found_result_cannot_remove_newer_generation() {
    ShellyDesiredStateLedger ledger;
    uint32_t oldGeneration = 0;
    TEST_ASSERT_TRUE(ledger.upsert(
        "relay-race", true, 10, 0, &oldGeneration));
    ShellyDesiredStateLease oldLease;
    TEST_ASSERT_TRUE(ledger.beginNextReady(0, oldLease));

    uint32_t newGeneration = 0;
    TEST_ASSERT_TRUE(ledger.upsert(
        "relay-race", false, 20, 1, &newGeneration));
    TEST_ASSERT_NOT_EQUAL(oldGeneration, newGeneration);

    // An old definitive lookup may retire only its own generation.
    TEST_ASSERT_FALSE(ledger.removeIfGeneration("relay-race", oldGeneration));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ShellyDesiredStateCompletion::Superseded),
        static_cast<int>(ledger.complete(
            "relay-race", oldGeneration, false, 2)));

    ShellyDesiredStateLease latest;
    TEST_ASSERT_TRUE(ledger.beginNextReady(2, latest));
    TEST_ASSERT_EQUAL_UINT32(newGeneration, latest.generation);
    TEST_ASSERT_EQUAL_UINT64(20, latest.peerRevision);
    TEST_ASSERT_FALSE(latest.value);
}

void test_disabled_peer_parks_active_intent_and_reenable_requeues_it() {
    ShellyDesiredStateLedger ledger;
    constexpr uint64_t enabledPeer = 101;
    constexpr uint64_t disabledPeer = 102;
    constexpr uint64_t reenabledPeer = 103;
    TEST_ASSERT_TRUE(ledger.upsert(
        "relay-toggle", true, enabledPeer, 0));

    ShellyDesiredStateLease inFlight;
    TEST_ASSERT_TRUE(ledger.beginNextReady(0, inFlight));
    TEST_ASSERT_TRUE(ledger.parkIfGeneration(
        inFlight.id, inFlight.generation, disabledPeer));
    TEST_ASSERT_EQUAL_UINT32(0, ledger.pendingCount());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ShellyDesiredStateCompletion::Ignored),
        static_cast<int>(ledger.complete(
            inFlight.id, inFlight.generation, true, 1)));

    TEST_ASSERT_TRUE(ledger.rebind(
        "relay-toggle", reenabledPeer, 2));
    ShellyDesiredStateLease resumed;
    TEST_ASSERT_TRUE(ledger.beginNextReady(2, resumed));
    TEST_ASSERT_TRUE(resumed.value);
    TEST_ASSERT_EQUAL_UINT64(reenabledPeer, resumed.peerRevision);
}

void test_peer_revision_tracks_transport_config_only() {
    SHELLY::ShellyDevice device;
    strlcpy(device.id, "relay-a", sizeof(device.id));
    strlcpy(device.ip, "192.168.1.20", sizeof(device.ip));
    device.relayIndex = 0;
    device.generation = 2;
    device.enabled = true;
    const uint64_t base = SHELLY::shellyPeerRevision(device);

    SHELLY::ShellyDevice changed = device;
    strlcpy(changed.ip, "192.168.1.21", sizeof(changed.ip));
    TEST_ASSERT_NOT_EQUAL(base, SHELLY::shellyPeerRevision(changed));
    changed = device;
    changed.relayIndex = 1;
    TEST_ASSERT_NOT_EQUAL(base, SHELLY::shellyPeerRevision(changed));
    changed = device;
    changed.generation = 1;
    TEST_ASSERT_NOT_EQUAL(base, SHELLY::shellyPeerRevision(changed));
    changed = device;
    changed.enabled = false;
    TEST_ASSERT_NOT_EQUAL(base, SHELLY::shellyPeerRevision(changed));

    changed = device;
    strlcpy(changed.name, "Presentation-only rename", sizeof(changed.name));
    changed.isOn = true;
    changed.power = 42.0f;
    TEST_ASSERT_EQUAL_UINT64(base, SHELLY::shellyPeerRevision(changed));
}

void test_stale_ack_fence_rejects_concurrent_peer_reconfiguration() {
    SHELLY::ShellyDevice commandPeer;
    strlcpy(commandPeer.id, "relay-a", sizeof(commandPeer.id));
    strlcpy(commandPeer.ip, "192.168.1.20", sizeof(commandPeer.ip));
    commandPeer.relayIndex = 0;
    commandPeer.generation = 2;
    commandPeer.enabled = true;

    SHELLY::ShellyDevice currentPeer = commandPeer;
    TEST_ASSERT_TRUE(SHELLY::shellyPeerMatches(currentPeer, commandPeer));

    // Models an upsert that wins the DeviceManager mutex after HTTP completed
    // but before its ACK is published. updateCommandState() evaluates this
    // same predicate while holding that mutex and must reject the old ACK.
    strlcpy(currentPeer.ip, "192.168.1.21", sizeof(currentPeer.ip));
    TEST_ASSERT_FALSE(SHELLY::shellyPeerMatches(currentPeer, commandPeer));

    currentPeer = commandPeer;
    currentPeer.relayIndex = 1;
    TEST_ASSERT_FALSE(SHELLY::shellyPeerMatches(currentPeer, commandPeer));
}

void test_late_worker_finish_becomes_reclaimable_and_not_running() {
    ShellyWorkerLifecycleState lifecycle;
    lifecycle.prepareStart();
    TEST_ASSERT_TRUE(lifecycle.isLive(true));
    TEST_ASSERT_FALSE(lifecycle.isReclaimable(true));

    // A stop timeout leaves the handle intact. When taskEntry later reaches
    // its suspension point, service-level isRunning() must stop reporting a
    // live worker and the next start()/stop() can reap that handle.
    TEST_ASSERT_FALSE(lifecycle.finishOrRestart());
    TEST_ASSERT_FALSE(lifecycle.isLive(true));
    TEST_ASSERT_TRUE(lifecycle.isReclaimable(true));
    TEST_ASSERT_FALSE(lifecycle.isLive(false));
    TEST_ASSERT_FALSE(lifecycle.isReclaimable(false));
}

void test_restart_request_linearizes_with_late_finish_after_stop_timeout() {
    ShellyWorkerLifecycleState lifecycle;
    lifecycle.prepareStart();

    // remove-last timed out with a live handle; a new relay intent arrives
    // before taskEntry has published Finished.
    TEST_ASSERT_TRUE(lifecycle.requestRestart(true));
    TEST_ASSERT_TRUE(lifecycle.finishOrRestart());
    TEST_ASSERT_TRUE(lifecycle.isLive(true));
    TEST_ASSERT_FALSE(lifecycle.isReclaimable(true));

    // Once the resumed loop acknowledges running=true, a later real stop is
    // not defeated by a stale restart request.
    lifecycle.acknowledgeLive();
    lifecycle.cancelRestartRequest();
    TEST_ASSERT_FALSE(lifecycle.finishOrRestart());
    TEST_ASSERT_TRUE(lifecycle.isReclaimable(true));
}

void test_finish_winning_restart_race_is_reaped_instead_of_stuck_live() {
    ShellyWorkerLifecycleState lifecycle;
    lifecycle.prepareStart();
    TEST_ASSERT_FALSE(lifecycle.finishOrRestart());
    TEST_ASSERT_FALSE(lifecycle.requestRestart(true));
    TEST_ASSERT_TRUE(lifecycle.isReclaimable(true));
}

void test_task_start_allocation_failure_retries_without_new_command() {
    ShellyStartRetrySchedule schedule;

    // First task stack/control-block allocation fails after the desired state
    // was stored. No second queueCommand call is needed to claim the retry.
    schedule.schedule(100);
    TEST_ASSERT_TRUE(schedule.isPending());
    TEST_ASSERT_FALSE(schedule.claimIfDue(349));
    TEST_ASSERT_TRUE(schedule.claimIfDue(350));

    // A second create failure backs off; the eventual successful start clears
    // recovery state and prevents spurious restarts.
    schedule.schedule(350);
    TEST_ASSERT_FALSE(schedule.claimIfDue(849));
    TEST_ASSERT_TRUE(schedule.claimIfDue(850));
    schedule.markStarted();
    TEST_ASSERT_FALSE(schedule.isPending());
    TEST_ASSERT_FALSE(schedule.claimIfDue(10000));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_duplicate_pending_state_is_coalesced);
    RUN_TEST(test_many_coalesced_wake_hints_preserve_final_off);
    RUN_TEST(test_stale_success_cannot_ack_newer_off);
    RUN_TEST(test_stale_failure_does_not_back_off_newer_state);
    RUN_TEST(test_failures_follow_fast_backoff_then_recovery_cadence);
    RUN_TEST(test_repeated_transport_allocation_failures_never_drop_intent);
    RUN_TEST(test_same_state_can_be_reasserted_after_success);
    RUN_TEST(test_peer_revision_change_supersedes_old_ack_and_retries_same_intent);
    RUN_TEST(test_remove_during_in_flight_command_frees_slot_and_ignores_old_ack);
    RUN_TEST(test_busy_device_lookup_retries_instead_of_deleting_intent);
    RUN_TEST(test_old_not_found_result_cannot_remove_newer_generation);
    RUN_TEST(test_disabled_peer_parks_active_intent_and_reenable_requeues_it);
    RUN_TEST(test_peer_revision_tracks_transport_config_only);
    RUN_TEST(test_stale_ack_fence_rejects_concurrent_peer_reconfiguration);
    RUN_TEST(test_late_worker_finish_becomes_reclaimable_and_not_running);
    RUN_TEST(test_restart_request_linearizes_with_late_finish_after_stop_timeout);
    RUN_TEST(test_finish_winning_restart_race_is_reaped_instead_of_stuck_live);
    RUN_TEST(test_task_start_allocation_failure_retries_without_new_command);
    return UNITY_END();
}
