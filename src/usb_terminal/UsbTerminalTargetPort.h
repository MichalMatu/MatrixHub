#pragma once

#include <cctype>
#include <cstddef>
#include <cstring>

namespace USB_TERMINAL {

constexpr const char* kAutoTargetPort = "auto";
constexpr size_t kMaxTargetPortLength = 31;

inline bool isAutoTargetPort(const char* targetPort) {
    if (!targetPort) {
        return false;
    }

    const char* expected = kAutoTargetPort;
    while (*targetPort && *expected) {
        const auto lhs = static_cast<unsigned char>(*targetPort++);
        const auto rhs = static_cast<unsigned char>(*expected++);
        if (std::tolower(lhs) != std::tolower(rhs)) {
            return false;
        }
    }

    return *targetPort == '\0' && *expected == '\0';
}

inline bool isAllowedManualTargetPortChar(char c) {
    const auto value = static_cast<unsigned char>(c);
    return std::isalnum(value) || c == '/' || c == '.' || c == '_' || c == '-' ||
           c == ':' || c == '+' || c == '@';
}

inline bool isValidManualTargetPort(const char* targetPort) {
    if (!targetPort || targetPort[0] == '\0') {
        return false;
    }

    size_t len = 0;
    for (const char* p = targetPort; *p; ++p) {
        if (++len > kMaxTargetPortLength || !isAllowedManualTargetPortChar(*p)) {
            return false;
        }
    }

    return true;
}

inline bool isValidTargetPortSetting(const char* targetPort) {
    return isAutoTargetPort(targetPort) || isValidManualTargetPort(targetPort);
}

inline bool copyNormalizedTargetPort(char* out, size_t outSize, const char* targetPort) {
    if (!out || outSize == 0 || !isValidTargetPortSetting(targetPort)) {
        return false;
    }

    if (isAutoTargetPort(targetPort)) {
        std::strncpy(out, kAutoTargetPort, outSize);
    } else {
        std::strncpy(out, targetPort, outSize);
    }
    out[outSize - 1] = '\0';
    return true;
}

inline bool shellQuoteTargetPort(char* out, size_t outSize, const char* targetPort) {
    if (!out || outSize < 3 || !isValidManualTargetPort(targetPort)) {
        return false;
    }

    const size_t len = std::strlen(targetPort);
    if (len + 3 > outSize) {
        return false;
    }

    out[0] = '\'';
    std::memcpy(out + 1, targetPort, len);
    out[len + 1] = '\'';
    out[len + 2] = '\0';
    return true;
}

}  // namespace USB_TERMINAL
