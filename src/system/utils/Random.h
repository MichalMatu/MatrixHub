#pragma once

#include <cstdint>
#include <climits>
#include <cstdlib>

#ifdef ESP_PLATFORM
#include <esp_random.h>
#endif

namespace UTILS {
namespace RNG {

inline uint32_t randomU32() {
#ifdef ESP_PLATFORM
    return esp_random();
#else
    // RAND_MAX can be as low as 32767; combine two draws to fill 32 bits.
    const uint32_t hi = static_cast<uint32_t>(rand()) & 0xFFFFu;
    const uint32_t lo = static_cast<uint32_t>(rand()) & 0xFFFFu;
    return (hi << 16) | lo;
#endif
}

inline uint64_t randomU64() {
    return (static_cast<uint64_t>(randomU32()) << 32) | randomU32();
}

// Inclusive range for unsigned values.
inline uint32_t rangeU32(uint32_t min, uint32_t max) {
    if (min >= max) return min;
    const uint64_t range = static_cast<uint64_t>(max) - min + 1u;
    const uint64_t limit = UINT64_MAX - (UINT64_MAX % range);
    uint64_t value;
    do {
        value = randomU64();
    } while (value >= limit);
    return min + static_cast<uint32_t>(value % range);
}

// Exclusive upper bound for unsigned values.
inline uint32_t rangeU32Exclusive(uint32_t maxExclusive) {
    if (maxExclusive == 0) return 0;
    return rangeU32(0u, maxExclusive - 1u);
}

// Inclusive range for signed values (supports negative min/max).
inline int32_t rangeI32(int32_t min, int32_t max) {
    if (min >= max) return min;
    const uint64_t range =
        static_cast<uint64_t>(static_cast<int64_t>(max) - static_cast<int64_t>(min) + 1);
    const uint64_t limit = UINT64_MAX - (UINT64_MAX % range);
    uint64_t value;
    do {
        value = randomU64();
    } while (value >= limit);
    return static_cast<int32_t>(static_cast<int64_t>(min) + static_cast<int64_t>(value % range));
}

}  // namespace RNG
}  // namespace UTILS
