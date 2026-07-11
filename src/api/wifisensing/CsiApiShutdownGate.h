#pragma once

#include <atomic>
#include <cstdint>

namespace API {

// Prevents new asynchronous API handlers from entering after shutdown starts
// and lets the owner drain handlers that entered before the shutdown fence.
// The closed bit and lease count share one atomic word so close() cannot race
// with a late lease acquisition.
class CsiApiShutdownGate {
public:
    class Lease {
    public:
        explicit Lease(CsiApiShutdownGate& gate)
            : _gate(gate.tryAcquire() ? &gate : nullptr) {}

        ~Lease() {
            if (_gate) {
                _gate->release();
            }
        }

        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;

        Lease(Lease&& other) noexcept : _gate(other._gate) {
            other._gate = nullptr;
        }

        Lease& operator=(Lease&&) = delete;

        explicit operator bool() const {
            return _gate != nullptr;
        }

    private:
        CsiApiShutdownGate* _gate = nullptr;
    };

    void close() {
        _state.fetch_or(kClosedBit, std::memory_order_acq_rel);
    }

    bool isClosed() const {
        return (_state.load(std::memory_order_acquire) & kClosedBit) != 0;
    }

    bool hasInFlight() const {
        return (_state.load(std::memory_order_acquire) & kLeaseCountMask) != 0;
    }

private:
    static constexpr uint32_t kClosedBit = uint32_t{1} << 31;
    static constexpr uint32_t kLeaseCountMask = ~kClosedBit;

    bool tryAcquire() {
        uint32_t state = _state.load(std::memory_order_acquire);
        while ((state & kClosedBit) == 0) {
            if ((state & kLeaseCountMask) == kLeaseCountMask) {
                return false;
            }
            if (_state.compare_exchange_weak(
                    state,
                    state + 1,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return true;
            }
        }
        return false;
    }

    void release() {
        _state.fetch_sub(1, std::memory_order_release);
    }

    std::atomic<uint32_t> _state{0};
};

} // namespace API
