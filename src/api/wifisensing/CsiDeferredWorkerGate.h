#pragma once

#include <atomic>
#include <cstdint>

namespace API {

/**
 * Tracks deferred lifecycle requests independently from worker ownership.
 *
 * A boolean "worker running" flag alone has a lost-wakeup window when a new
 * request arrives while the old worker is publishing its exit.  Generations
 * make the request durable until a worker explicitly completes it, while the
 * release-and-reclaim handshake assigns every exit-race request to either the
 * old worker or a newly scheduled one.
 */
class CsiDeferredWorkerGate {
public:
    uint32_t request() {
        return _requested.fetch_add(1, std::memory_order_acq_rel) + 1;
    }

    uint32_t requestedGeneration() const {
        return _requested.load(std::memory_order_acquire);
    }

    uint32_t completedGeneration() const {
        return _completed.load(std::memory_order_acquire);
    }

    bool hasPendingRequest() const {
        return requestedGeneration() != completedGeneration();
    }

    bool tryClaimWorker() {
        bool expected = false;
        return _workerRunning.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel, std::memory_order_acquire);
    }

    void completeThrough(uint32_t generation) {
        _completed.store(generation, std::memory_order_release);
    }

    /**
     * Completes a request only when every external lifecycle step reached its
     * terminal state and no newer request superseded this worker snapshot.
     *
     * Keeping the success predicate in the gate makes it hard for callers to
     * accidentally acknowledge a failed transport teardown.  A false return
     * deliberately leaves the request pending for the current worker or the
     * main-loop recovery path to retry.
     */
    bool completeCurrentIf(uint32_t generation, bool terminalStateReached) {
        if (!terminalStateReached || requestedGeneration() != generation) {
            return false;
        }
        completeThrough(generation);
        return true;
    }

    void releaseWorker() {
        _workerRunning.store(false, std::memory_order_release);
    }

    /**
     * Releases this worker and reclaims ownership if an uncompleted request
     * raced with the release.  A failed reclaim means another scheduler owns
     * the pending work.
     */
    bool releaseWorkerAndTryReclaim() {
        releaseWorker();
        if (!hasPendingRequest()) {
            return false;
        }
        return tryClaimWorker();
    }

    bool isWorkerRunning() const {
        return _workerRunning.load(std::memory_order_acquire);
    }

private:
    std::atomic<uint32_t> _requested{0};
    std::atomic<uint32_t> _completed{0};
    std::atomic<bool> _workerRunning{false};
};

}  // namespace API
