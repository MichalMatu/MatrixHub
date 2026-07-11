#pragma once

#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../types/AlarmRule.h"
#include "../../config/Hardware.h"

namespace ALARMS {

namespace ALARM_RULE_VALIDATION_DETAIL {

inline size_t boundedLength(const char* value, size_t capacity) {
    if (!value) {
        return capacity;
    }
    size_t length = 0;
    while (length < capacity && value[length] != '\0') {
        ++length;
    }
    return length;
}

inline bool hasVisibleCharacter(const char* value, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        if (!std::isspace(static_cast<unsigned char>(value[i]))) {
            return true;
        }
    }
    return false;
}

inline bool isHexDigit(char value) {
    return std::isxdigit(static_cast<unsigned char>(value)) != 0;
}

inline bool isValidBleMac(const char* mac) {
    if (boundedLength(mac, kBleMacLen) != kBleMacLen - 1) {
        return false;
    }
    for (size_t i = 0; i < kBleMacLen - 1; ++i) {
        if ((i + 1) % 3 == 0) {
            if (mac[i] != ':') {
                return false;
            }
        } else if (!isHexDigit(mac[i])) {
            return false;
        }
    }
    return true;
}

inline const char* trimLeft(const char* value) {
    while (*value && std::isspace(static_cast<unsigned char>(*value))) {
        ++value;
    }
    return value;
}

inline size_t trimmedLength(const char* value) {
    const char* begin = trimLeft(value);
    const char* end = begin + std::strlen(begin);
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return static_cast<size_t>(end - begin);
}

}  // namespace ALARM_RULE_VALIDATION_DETAIL

inline bool alarmRuleNamesEqual(const char* left, const char* right) {
    using namespace ALARM_RULE_VALIDATION_DETAIL;
    const char* leftBegin = trimLeft(left);
    const char* rightBegin = trimLeft(right);
    const size_t leftLength = trimmedLength(leftBegin);
    const size_t rightLength = trimmedLength(rightBegin);
    if (leftLength != rightLength) {
        return false;
    }
    for (size_t i = 0; i < leftLength; ++i) {
        if (std::tolower(static_cast<unsigned char>(leftBegin[i])) !=
            std::tolower(static_cast<unsigned char>(rightBegin[i]))) {
            return false;
        }
    }
    return true;
}

inline bool isValidAlarmRuleDefinition(const AlarmRule& rule) {
    using namespace ALARM_RULE_VALIDATION_DETAIL;

    const size_t idLength = boundedLength(rule.id, kMaxIdLen);
    const size_t nameLength = boundedLength(rule.name, kMaxAlarmNameLen);
    if (idLength == 0 || idLength >= kMaxIdLen ||
        nameLength == 0 || nameLength >= kMaxAlarmNameLen ||
        !hasVisibleCharacter(rule.name, nameLength)) {
        return false;
    }

    if (static_cast<uint8_t>(rule.source) >
            static_cast<uint8_t>(AlarmSource::GpioDigital) ||
        static_cast<uint8_t>(rule.op) >
            static_cast<uint8_t>(AlarmOperator::Below) ||
        static_cast<uint8_t>(rule.severity) >
            static_cast<uint8_t>(AlarmSeverity::Critical)) {
        return false;
    }

    constexpr uint8_t kAllowedNotifyMask =
        static_cast<uint8_t>(NotifyChannel::Telegram) |
        static_cast<uint8_t>(NotifyChannel::Led) |
        static_cast<uint8_t>(NotifyChannel::Webhook) |
        static_cast<uint8_t>(NotifyChannel::Pushover);
    if ((static_cast<uint8_t>(rule.notifyChannels) & ~kAllowedNotifyMask) != 0) {
        return false;
    }

    if (!std::isfinite(rule.threshold) ||
        rule.threshold < LIMITS::ALARMS::MIN_THRESHOLD ||
        rule.threshold > LIMITS::ALARMS::MAX_THRESHOLD ||
        rule.cooldownSeconds < LIMITS::ALARMS::MIN_COOLDOWN_SEC ||
        rule.cooldownSeconds > LIMITS::ALARMS::MAX_COOLDOWN_SEC) {
        return false;
    }

    if (rule.isBleSource() && !isValidBleMac(rule.bleDeviceMac)) {
        return false;
    }
    if (rule.isGpioSource()) {
        const size_t gpioLength = boundedLength(rule.gpioId, kGpioIdLen);
        if (gpioLength == 0 || gpioLength >= kGpioIdLen ||
            !hasVisibleCharacter(rule.gpioId, gpioLength)) {
            return false;
        }
    }

    if (rule.shellyDeviceCount > kMaxShellyPerRule) {
        return false;
    }
    for (uint8_t i = 0; i < rule.shellyDeviceCount; ++i) {
        const size_t length = boundedLength(rule.shellyDeviceIds[i], kShellyIdLen);
        if (length == 0 || length >= kShellyIdLen) {
            return false;
        }
        for (uint8_t previous = 0; previous < i; ++previous) {
            if (std::strncmp(rule.shellyDeviceIds[previous],
                             rule.shellyDeviceIds[i],
                             kShellyIdLen) == 0) {
                return false;
            }
        }
    }

    return true;
}

}  // namespace ALARMS
