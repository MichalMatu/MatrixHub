#pragma once

#include <cstddef>
#include <cstdint>

namespace SYSTEM {

constexpr size_t kBootIdHexLength = 16;

inline void formatBootIdHex(uint64_t bootId, char (&out)[kBootIdHexLength + 1]) {
    static constexpr char kHexDigits[] = "0123456789abcdef";
    for (size_t i = 0; i < kBootIdHexLength; ++i) {
        out[kBootIdHexLength - 1 - i] = kHexDigits[bootId & 0x0FU];
        bootId >>= 4U;
    }
    out[kBootIdHexLength] = '\0';
}

}  // namespace SYSTEM
