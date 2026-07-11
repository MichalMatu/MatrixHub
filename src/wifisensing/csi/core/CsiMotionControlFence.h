#pragma once

#include <atomic>
#include <cstdint>

namespace WIFISENSING {
namespace CSI {

// Linearizes motion control commands with detector snapshots and alarm edges.
// A command closes the fence before publishing its new epoch. Consumers may
// commit work only after that exact epoch has been marked applied.
class CsiMotionControlFence {
public:
    uint32_t beginTransition() {
        _transitionInProgress.store(true, std::memory_order_release);
        return _requestedEpoch.fetch_add(1, std::memory_order_acq_rel) + 1u;
    }

    bool completeTransition(uint32_t epoch) {
        if (_requestedEpoch.load(std::memory_order_acquire) != epoch) {
            return false;
        }
        _appliedEpoch.store(epoch, std::memory_order_release);
        _transitionInProgress.store(false, std::memory_order_release);
        return true;
    }

    bool canPublish(uint32_t expectedEpoch) const {
        return !_transitionInProgress.load(std::memory_order_acquire) &&
               _requestedEpoch.load(std::memory_order_acquire) == expectedEpoch &&
               _appliedEpoch.load(std::memory_order_acquire) == expectedEpoch;
    }

    uint32_t requestedEpoch() const {
        return _requestedEpoch.load(std::memory_order_acquire);
    }

    bool transitionInProgress() const {
        return _transitionInProgress.load(std::memory_order_acquire);
    }

private:
    std::atomic<uint32_t> _requestedEpoch{0};
    std::atomic<uint32_t> _appliedEpoch{0};
    std::atomic<bool> _transitionInProgress{false};
};

// Single-value mailbox used for the terminal disabled=false synchronization
// when no worker/callback is available. Epoch matching prevents an old clear
// from leaking into a later enabled configuration.
class CsiMotionSyncMailbox {
public:
    void queue(bool value, uint32_t epoch) {
        _value = value;
        _epoch = epoch;
        _pending = true;
    }

    bool takeForEpoch(uint32_t epoch, bool& value) {
        if (!_pending || _epoch != epoch) {
            return false;
        }
        value = _value;
        _pending = false;
        return true;
    }

    void discard() { _pending = false; }
    bool pending() const { return _pending; }

private:
    uint32_t _epoch = 0;
    bool _value = false;
    bool _pending = false;
};

} // namespace CSI
} // namespace WIFISENSING
