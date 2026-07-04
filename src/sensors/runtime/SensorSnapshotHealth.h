#pragma once

#include <cstdint>

namespace SENSORS {

// Centralize the "fresh vs stale" decision so runtime code, REST snapshots and
// tests all use the same wall-clock rule instead of drifting over time.
// Unsigned subtraction is intentional here; millis() wrap-around still yields
// the correct elapsed interval under normal modular arithmetic.
constexpr bool isSnapshotFresh(uint32_t timestampMs, uint32_t nowMs, uint32_t timeoutMs) {
    return timestampMs != 0 && (nowMs - timestampMs) < timeoutMs;
}

// Repeated NO_DATA polls are normal between 5 s SCD4x measurements, but once
// the newest retained sample ages past the freshness timeout we should promote
// that condition to an explicit stale state exactly once.
constexpr bool shouldPromoteNoDataToStale(uint32_t timestampMs,
                                          uint32_t nowMs,
                                          uint32_t timeoutMs,
                                          bool alreadyStale) {
    return !alreadyStale && timestampMs != 0 && (nowMs - timestampMs) >= timeoutMs;
}

}  // namespace SENSORS
