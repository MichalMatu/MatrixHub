#pragma once

#include <atomic>
#include <stdint.h>

namespace WIFISENSING {
namespace CSI {

// Stable-lifetime hand-off between the closed-source Wi-Fi driver and the
// service instance. The driver receives a pointer to this fence, not `this`.
// Therefore a callback that was dispatched but has not executed its first
// instruction yet can safely arrive after the service detaches: it observes a
// null owner without dereferencing freed service memory.
class CsiRxCallbackFence {
public:
    // These are deliberately trivial/always-inline: the Wi-Fi callback is in
    // IRAM and must not call a generated C++ RAII destructor from flash.
    __attribute__((always_inline)) inline void* enter() {
        _inFlight.fetch_add(1, std::memory_order_acq_rel);
        return _owner.load(std::memory_order_acquire);
    }

    __attribute__((always_inline)) inline void leave() {
        _inFlight.fetch_sub(1, std::memory_order_acq_rel);
    }

    bool attach(void* owner) {
        if (!owner) {
            return false;
        }
        void* expected = nullptr;
        return _owner.compare_exchange_strong(
            expected, owner, std::memory_order_acq_rel, std::memory_order_acquire);
    }

    void detach(void* owner) {
        void* expected = owner;
        (void)_owner.compare_exchange_strong(
            expected, nullptr, std::memory_order_acq_rel, std::memory_order_acquire);
    }

    bool hasInFlight() const {
        return _inFlight.load(std::memory_order_acquire) != 0;
    }

private:
    std::atomic<void*> _owner{nullptr};
    std::atomic<uint32_t> _inFlight{0};
};

} // namespace CSI
} // namespace WIFISENSING
