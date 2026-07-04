#pragma once

#include <atomic>
#include <cstdint>
#include <freertos/FreeRTOS.h>

namespace SYSTEM {
namespace LOCK_DIAGNOSTICS {

struct LockCounterSnapshot {
    uint32_t attempts = 0;
    uint32_t successes = 0;
    uint32_t timeouts = 0;
    uint32_t slowAcquires = 0;
    uint32_t unlimitedWaits = 0;
    uint32_t maxWaitTicks = 0;
};

struct Snapshot {
    LockCounterSnapshot standard;
    LockCounterSnapshot recursive;
    TickType_t slowThresholdTicks = 0;
};

namespace detail {

struct AtomicLockCounters {
    std::atomic<uint32_t> attempts{0};
    std::atomic<uint32_t> successes{0};
    std::atomic<uint32_t> timeouts{0};
    std::atomic<uint32_t> slowAcquires{0};
    std::atomic<uint32_t> unlimitedWaits{0};
    std::atomic<uint32_t> maxWaitTicks{0};
};

inline AtomicLockCounters g_standardCounters;
inline AtomicLockCounters g_recursiveCounters;

constexpr TickType_t kSlowAcquireThresholdTicks =
    pdMS_TO_TICKS(20) > 0 ? pdMS_TO_TICKS(20) : 1;

inline void updateMax(std::atomic<uint32_t>& current, uint32_t value) {
    uint32_t observed = current.load(std::memory_order_relaxed);
    while (value > observed &&
           !current.compare_exchange_weak(
               observed,
               value,
               std::memory_order_relaxed,
               std::memory_order_relaxed)) {
    }
}

inline LockCounterSnapshot snapshotCounters(const AtomicLockCounters& counters) {
    LockCounterSnapshot snapshot;
    snapshot.attempts = counters.attempts.load(std::memory_order_relaxed);
    snapshot.successes = counters.successes.load(std::memory_order_relaxed);
    snapshot.timeouts = counters.timeouts.load(std::memory_order_relaxed);
    snapshot.slowAcquires = counters.slowAcquires.load(std::memory_order_relaxed);
    snapshot.unlimitedWaits = counters.unlimitedWaits.load(std::memory_order_relaxed);
    snapshot.maxWaitTicks = counters.maxWaitTicks.load(std::memory_order_relaxed);
    return snapshot;
}

inline void resetCounters(AtomicLockCounters& counters) {
    counters.attempts.store(0, std::memory_order_relaxed);
    counters.successes.store(0, std::memory_order_relaxed);
    counters.timeouts.store(0, std::memory_order_relaxed);
    counters.slowAcquires.store(0, std::memory_order_relaxed);
    counters.unlimitedWaits.store(0, std::memory_order_relaxed);
    counters.maxWaitTicks.store(0, std::memory_order_relaxed);
}

}  // namespace detail

inline void recordAcquire(
    bool recursive,
    TickType_t waitedTicks,
    TickType_t timeoutTicks,
    bool acquired) {
    detail::AtomicLockCounters& counters =
        recursive ? detail::g_recursiveCounters : detail::g_standardCounters;

    counters.attempts.fetch_add(1, std::memory_order_relaxed);
    if (timeoutTicks == portMAX_DELAY) {
        counters.unlimitedWaits.fetch_add(1, std::memory_order_relaxed);
    }
    if (acquired) {
        counters.successes.fetch_add(1, std::memory_order_relaxed);
    } else {
        counters.timeouts.fetch_add(1, std::memory_order_relaxed);
    }
    if (waitedTicks >= detail::kSlowAcquireThresholdTicks) {
        counters.slowAcquires.fetch_add(1, std::memory_order_relaxed);
    }
    detail::updateMax(counters.maxWaitTicks, static_cast<uint32_t>(waitedTicks));
}

inline Snapshot snapshot() {
    Snapshot snapshot;
    snapshot.standard = detail::snapshotCounters(detail::g_standardCounters);
    snapshot.recursive = detail::snapshotCounters(detail::g_recursiveCounters);
    snapshot.slowThresholdTicks = detail::kSlowAcquireThresholdTicks;
    return snapshot;
}

inline void reset() {
    detail::resetCounters(detail::g_standardCounters);
    detail::resetCounters(detail::g_recursiveCounters);
}

}  // namespace LOCK_DIAGNOSTICS
}  // namespace SYSTEM
