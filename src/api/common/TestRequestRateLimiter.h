#pragma once

#include <Arduino.h>
#include "../../config/System.h"

#include <atomic>
#include <cstdint>

namespace API::TESTS {

constexpr const char* kBusyErrorCode = "busy/test_in_progress";

class TestRequestRateLimiter {
public:
    bool tryAcquire(uint32_t holdMs = GLOBAL_COOLDOWN_MS) {
        uint32_t now = millis();
        uint32_t current = _busyUntilMs.load(std::memory_order_relaxed);

        while (true) {
            if (isFuture(current, now)) {
                return false;
            }

            const uint32_t next = now + holdMs;
            if (_busyUntilMs.compare_exchange_weak(
                    current,
                    next,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed)) {
                return true;
            }

            now = millis();
        }
    }

    uint32_t remainingMs() const {
        const uint32_t now = millis();
        const uint32_t until = _busyUntilMs.load(std::memory_order_relaxed);
        if (!isFuture(until, now)) {
            return 0;
        }
        return until - now;
    }

private:
    static bool isFuture(uint32_t deadline, uint32_t now) {
        return static_cast<int32_t>(deadline - now) > 0;
    }

    std::atomic<uint32_t> _busyUntilMs{0};
};

// Shared process-local limiter for "send test now" style admin actions.
// This is intentionally a tiny helper, not a service/singleton with ownership
// semantics. A free accessor keeps call sites honest about that role.
inline TestRequestRateLimiter& testRequestRateLimiter() {
    static TestRequestRateLimiter limiter;
    return limiter;
}

}  // namespace API::TESTS
