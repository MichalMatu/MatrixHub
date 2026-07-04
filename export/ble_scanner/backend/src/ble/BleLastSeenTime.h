#pragma once

#include <stdint.h>
#include <time.h>

namespace BLE::LastSeenTime {

constexpr int kMinValidYear = 2020;
constexpr int kMaxValidYear = 2099;

// BLE cache timestamps are tracked with millis() only. We convert them to epoch
// time solely for API/UI consumers, and only after the device wall-clock looks
// trustworthy enough to avoid manufacturing fake "last seen" dates.
inline bool isWallClockValid(time_t now) {
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    const int year = timeinfo.tm_year + 1900;
    return year >= kMinValidYear && year <= kMaxValidYear;
}

// Returns a real epoch-based last_seen when possible, or 0 as an explicit
// "time unavailable" sentinel that UI layers can render safely in AP/offline
// mode before NTP or manual time sync.
inline uint64_t toEpochMsOrZero(uint32_t nowMs,
                                uint32_t lastSeenMs,
                                uint64_t nowEpochMs,
                                bool wallClockValid) {
    if (!wallClockValid || lastSeenMs == 0) {
        return 0ULL;
    }

    const uint32_t deltaMs = (nowMs >= lastSeenMs)
                                 ? (nowMs - lastSeenMs)
                                 : (0xFFFFFFFFu - lastSeenMs + nowMs + 1u);
    return (nowEpochMs > static_cast<uint64_t>(deltaMs))
               ? (nowEpochMs - static_cast<uint64_t>(deltaMs))
               : 0ULL;
}

}  // namespace BLE::LastSeenTime
