#pragma once

#include <atomic>
#include <cstdint>

namespace SHELLY {

/**
 * Allocation-free recovery schedule for a Shelly worker that could not start.
 *
 * The desired-state ledger survives task/stack allocation failure. The main
 * loop uses this schedule to retry task creation without requiring another
 * relay edge or an API client to repeat the command.
 */
class ShellyStartRetrySchedule {
public:
    void schedule(uint32_t nowMs) {
        uint8_t failures = _consecutiveFailures.load(std::memory_order_relaxed);
        if (failures < UINT8_MAX) {
            ++failures;
            _consecutiveFailures.store(failures, std::memory_order_relaxed);
        }
        _nextAttemptMs.store(nowMs + retryDelayMs(failures),
                             std::memory_order_relaxed);
        _pending.store(true, std::memory_order_release);
    }

    bool claimIfDue(uint32_t nowMs) {
        if (!_pending.load(std::memory_order_acquire)) {
            return false;
        }
        const uint32_t deadline =
            _nextAttemptMs.load(std::memory_order_relaxed);
        if (static_cast<int32_t>(nowMs - deadline) < 0) {
            return false;
        }

        bool expected = true;
        return _pending.compare_exchange_strong(
            expected,
            false,
            std::memory_order_acq_rel,
            std::memory_order_acquire);
    }

    void markStarted() {
        _consecutiveFailures.store(0, std::memory_order_relaxed);
        _nextAttemptMs.store(0, std::memory_order_relaxed);
        _pending.store(false, std::memory_order_release);
    }

    void cancel() { markStarted(); }

    bool isPending() const {
        return _pending.load(std::memory_order_acquire);
    }

    static uint32_t retryDelayMs(uint8_t consecutiveFailures) {
        if (consecutiveFailures == 0) {
            return 0;
        }

        constexpr uint32_t kInitialDelayMs = 250;
        constexpr uint32_t kMaxDelayMs = 5000;
        const uint8_t shift =
            consecutiveFailures > 5 ? 4 : consecutiveFailures - 1;
        const uint32_t delay = kInitialDelayMs << shift;
        return delay > kMaxDelayMs ? kMaxDelayMs : delay;
    }

private:
    std::atomic<bool> _pending{false};
    std::atomic<uint8_t> _consecutiveFailures{0};
    std::atomic<uint32_t> _nextAttemptMs{0};
};

}  // namespace SHELLY
