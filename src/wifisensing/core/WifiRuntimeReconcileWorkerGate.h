#pragma once

#include <atomic>

namespace WIFISENSING {

/**
 * Single-flight ownership and terminal shutdown fence for the deferred runtime
 * reconciler. The lifecycle owner must drain an already claimed worker before
 * releasing the claim with complete() or destroying its dependencies.
 */
class WifiRuntimeReconcileWorkerGate {
public:
    bool tryClaim() {
        if (_shutdownRequested.load(std::memory_order_acquire)) {
            return false;
        }

        bool expected = false;
        if (!_workerRunning.compare_exchange_strong(
                expected,
                true,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return false;
        }

        // Close the race where shutdown starts between the first check and the
        // ownership CAS. No task has been created yet, so the caller can safely
        // release the claim here.
        if (_shutdownRequested.load(std::memory_order_acquire)) {
            complete();
            return false;
        }
        return true;
    }

    void complete() {
        _workerRunning.store(false, std::memory_order_release);
    }

    void requestShutdown() {
        _shutdownRequested.store(true, std::memory_order_release);
    }

    bool shouldRun() const {
        return !_shutdownRequested.load(std::memory_order_acquire);
    }

    bool isWorkerRunning() const {
        return _workerRunning.load(std::memory_order_acquire);
    }

private:
    std::atomic<bool> _workerRunning{false};
    std::atomic<bool> _shutdownRequested{false};
};

}  // namespace WIFISENSING
