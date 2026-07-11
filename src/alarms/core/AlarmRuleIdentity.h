#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../types/AlarmRule.h"

namespace ALARMS {

namespace ALARM_RULE_IDENTITY_DETAIL {

constexpr uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr uint64_t kFnvPrime = 1099511628211ULL;

inline void appendByte(uint64_t& hash, uint8_t value) {
    hash ^= value;
    hash *= kFnvPrime;
}

inline void appendBoundedString(uint64_t& hash,
                                uint8_t fieldTag,
                                const char* value,
                                size_t capacity) {
    appendByte(hash, fieldTag);
    size_t length = 0;
    if (value) {
        while (length < capacity && value[length] != '\0') {
            ++length;
        }
    }
    appendByte(hash, static_cast<uint8_t>(length));
    for (size_t i = 0; i < length; ++i) {
        appendByte(hash, static_cast<uint8_t>(value[i]));
    }
}

inline bool sameBoundedString(const char* left,
                              const char* right,
                              size_t capacity) {
    return std::strncmp(left, right, capacity) == 0;
}

}  // namespace ALARM_RULE_IDENTITY_DETAIL

/**
 * Whether a rule is allowed to inherit retained runtime across a boot.
 * Disabled rules and non-finite analog thresholds always start clean.
 */
inline bool canRetainAlarmRuntime(const AlarmRule& rule) {
    return rule.enabled &&
           (rule.isBooleanLikeSource() || std::isfinite(rule.threshold));
}

/**
 * Exact live-update equivalence relation for retained alarm runtime.
 * Presentation, delivery and cooldown fields intentionally do not participate.
 */
inline bool hasSameAlarmRuntimeIdentity(const AlarmRule& oldRule,
                                        const AlarmRule& newRule) {
    using namespace ALARM_RULE_IDENTITY_DETAIL;

    if (!canRetainAlarmRuntime(oldRule) || !canRetainAlarmRuntime(newRule) ||
        !sameBoundedString(oldRule.id, newRule.id, kMaxIdLen) ||
        oldRule.source != newRule.source) {
        return false;
    }

    if (oldRule.isBleSource() &&
        !sameBoundedString(oldRule.bleDeviceMac,
                           newRule.bleDeviceMac,
                           kBleMacLen)) {
        return false;
    }
    if (oldRule.isGpioSource() &&
        !sameBoundedString(oldRule.gpioId, newRule.gpioId, kGpioIdLen)) {
        return false;
    }

    // Boolean sources have one canonical meaning: true is alarm. Legacy
    // operator/threshold values therefore cannot manufacture a false edge.
    if (newRule.isBooleanLikeSource()) {
        return true;
    }

    return oldRule.op == newRule.op && oldRule.threshold == newRule.threshold;
}

/**
 * Stable, allocation-free key correlating a LittleFS rule definition with its
 * RTC-retained runtime. The byte format is explicitly tagged and versioned so
 * field concatenation cannot alias. Float bytes use little-endian order and
 * -0.0 is canonicalized to +0.0 to match C++ equality semantics.
 */
inline uint64_t stableAlarmRuntimeIdentityHash(const AlarmRule& rule) {
    using namespace ALARM_RULE_IDENTITY_DETAIL;

    if (!canRetainAlarmRuntime(rule)) {
        return 0;
    }

    uint64_t hash = kFnvOffset;
    appendByte(hash, 0x01);  // Runtime identity byte-format version.
    appendBoundedString(hash, 0x10, rule.id, kMaxIdLen);
    appendByte(hash, 0x11);
    appendByte(hash, rule.enabled ? 1U : 0U);
    appendByte(hash, 0x12);
    appendByte(hash, static_cast<uint8_t>(rule.source));

    if (rule.isBleSource()) {
        appendBoundedString(hash, 0x13, rule.bleDeviceMac, kBleMacLen);
    } else if (rule.isGpioSource()) {
        appendBoundedString(hash, 0x14, rule.gpioId, kGpioIdLen);
    }

    appendByte(hash, 0x15);
    appendByte(hash, rule.isBooleanLikeSource() ? 1U : 0U);
    if (!rule.isBooleanLikeSource()) {
        appendByte(hash, 0x16);
        appendByte(hash, static_cast<uint8_t>(rule.op));

        float canonicalThreshold = rule.threshold;
        if (canonicalThreshold == 0.0f) {
            canonicalThreshold = 0.0f;
        }
        uint32_t thresholdBits = 0;
        std::memcpy(&thresholdBits, &canonicalThreshold, sizeof(thresholdBits));
        appendByte(hash, 0x17);
        for (uint8_t byteIndex = 0; byteIndex < sizeof(thresholdBits); ++byteIndex) {
            appendByte(hash,
                       static_cast<uint8_t>((thresholdBits >> (byteIndex * 8U)) &
                                            0xFFU));
        }
    }

    return hash;
}

}  // namespace ALARMS
