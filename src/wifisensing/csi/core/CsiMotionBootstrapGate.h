#pragma once

#include <atomic>

namespace WIFISENSING {
namespace CSI {

// Suppresses callback synchronization while boot still has only the
// constructor-default motion configuration. The persisted configuration owns
// the first definitive publication.
class CsiMotionBootstrapGate {
public:
    void begin(bool retainedMotion) {
        _retainedMotion.store(retainedMotion, std::memory_order_release);
        _pending.store(true, std::memory_order_release);
    }

    void complete() {
        _pending.store(false, std::memory_order_release);
    }

    bool pending() const {
        return _pending.load(std::memory_order_acquire);
    }

    void retain(bool motion) {
        _retainedMotion.store(motion, std::memory_order_release);
    }

    bool retained() const {
        return _retainedMotion.load(std::memory_order_acquire);
    }

private:
    std::atomic<bool> _pending{false};
    std::atomic<bool> _retainedMotion{false};
};

} // namespace CSI
} // namespace WIFISENSING
