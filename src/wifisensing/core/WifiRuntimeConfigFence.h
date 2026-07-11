#pragma once

#include <atomic>
#include <cstdint>

namespace WIFISENSING {

/**
 * Rejects a runtime-reconcile snapshot after a newer persisted configuration
 * has entered its apply transaction.
 */
class WifiRuntimeConfigFence {
public:
    uint32_t snapshot() const {
        return _generation.load(std::memory_order_acquire);
    }

    uint32_t markConfigChanged() {
        return _generation.fetch_add(1, std::memory_order_acq_rel) + 1;
    }

    bool isCurrent(uint32_t generation) const {
        return snapshot() == generation;
    }

private:
    std::atomic<uint32_t> _generation{0};
};

}  // namespace WIFISENSING
