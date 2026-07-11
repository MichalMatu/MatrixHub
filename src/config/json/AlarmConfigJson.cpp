#include "AlarmConfigJson.h"
#include "../App.h"
#include "../../alarms/AlarmRulesStore.h"
#include "../../alarms/core/AlarmRuleValidation.h"
#include "../System.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../alarms/serialization/AlarmEnumConverters.h"
#include "../../system/memory/SystemAllocator.h"
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>

namespace CONFIG {
namespace JSON {

namespace {

constexpr uint8_t kAllowedNotifyChannelsMask =
    static_cast<uint8_t>(ALARMS::NotifyChannel::Telegram) |
    static_cast<uint8_t>(ALARMS::NotifyChannel::Led) |
    static_cast<uint8_t>(ALARMS::NotifyChannel::Webhook) |
    static_cast<uint8_t>(ALARMS::NotifyChannel::Pushover);

bool alarmNamesEqual(const char* left, const char* right);

void saveAlarmRulesSnapshot(JsonObject& obj, const ALARMS::AlarmRulesSnapshot& rulesSnapshot) {
    JsonArray rules = obj[Keys::kRules].to<JsonArray>();
    for (uint8_t i = 0; i < rulesSnapshot.ruleCount; i++) {
        const auto& r = rulesSnapshot.rules[i];
        JsonObject rule = rules.add<JsonObject>();
        rule[Keys::kId].set(String(r.id));
        rule[Keys::kName].set(String(r.name));
        rule[Keys::kEnabled] = r.enabled;
        rule[Keys::kSource] = static_cast<int>(r.source);
        rule[Keys::kOperator] = static_cast<int>(r.op);
        rule[Keys::kThreshold] = r.threshold;
        rule[Keys::kSeverity] = static_cast<int>(r.severity);
        rule[Keys::kNotifyChannels] = static_cast<int>(r.notifyChannels);
        rule[Keys::kCooldownSeconds] = r.cooldownSeconds;
        rule[Keys::kCreatedAt] = r.createdAt;
        rule[Keys::kUpdatedAt] = r.updatedAt;
        rule[Keys::kBleDeviceMac].set(String(r.bleDeviceMac));
        rule[Keys::kGpioId].set(String(r.gpioId));

        if (r.shellyDeviceCount > 0) {
            JsonArray sdevs = rule[Keys::kShellyDeviceIds].to<JsonArray>();
            for (uint8_t j = 0; j < r.shellyDeviceCount; j++) {
                sdevs.add(String(r.shellyDeviceIds[j]));
            }
        }
    }
}

bool tryParseSource(JsonVariantConst value, ALARMS::AlarmSource& out) {
    if (value.is<int>()) {
        const int srcInt = value.as<int>();
        if (srcInt < static_cast<int>(ALARMS::AlarmSource::CO2) ||
            srcInt > static_cast<int>(ALARMS::AlarmSource::GpioDigital)) {
            return false;
        }
        out = static_cast<ALARMS::AlarmSource>(srcInt);
        return true;
    }

    const char* srcStr = value | static_cast<const char*>(nullptr);
    if (!srcStr) {
        return false;
    }

    if (strcmp(srcStr, "co2") == 0) {
        out = ALARMS::AlarmSource::CO2;
        return true;
    }
    if (strcmp(srcStr, "temperature") == 0) {
        out = ALARMS::AlarmSource::Temperature;
        return true;
    }
    if (strcmp(srcStr, "humidity") == 0) {
        out = ALARMS::AlarmSource::Humidity;
        return true;
    }
    if (strcmp(srcStr, "wifi_motion") == 0) {
        out = ALARMS::AlarmSource::WifiMotion;
        return true;
    }
    if (strcmp(srcStr, "ble_temperature") == 0) {
        out = ALARMS::AlarmSource::BleTemperature;
        return true;
    }
    if (strcmp(srcStr, "ble_humidity") == 0) {
        out = ALARMS::AlarmSource::BleHumidity;
        return true;
    }
    if (strcmp(srcStr, "ble_battery") == 0) {
        out = ALARMS::AlarmSource::BleBattery;
        return true;
    }
    if (strcmp(srcStr, "ble_rssi") == 0) {
        out = ALARMS::AlarmSource::BleRssi;
        return true;
    }
    if (strcmp(srcStr, "wifi_csi_motion") == 0) {
        out = ALARMS::AlarmSource::WifiCsiMotion;
        return true;
    }
    if (strcmp(srcStr, "imu_tamper") == 0) {
        out = ALARMS::AlarmSource::ImuTamper;
        return true;
    }
    if (strcmp(srcStr, "gpio_digital") == 0) {
        out = ALARMS::AlarmSource::GpioDigital;
        return true;
    }

    return false;
}

bool tryParseOperator(JsonVariantConst value, ALARMS::AlarmOperator& out) {
    if (value.is<int>()) {
        const int opInt = value.as<int>();
        if (opInt < static_cast<int>(ALARMS::AlarmOperator::Above) ||
            opInt > static_cast<int>(ALARMS::AlarmOperator::Below)) {
            return false;
        }
        out = static_cast<ALARMS::AlarmOperator>(opInt);
        return true;
    }

    const char* opStr = value | static_cast<const char*>(nullptr);
    if (!opStr) {
        return false;
    }

    if (strcmp(opStr, "above") == 0) {
        out = ALARMS::AlarmOperator::Above;
        return true;
    }
    if (strcmp(opStr, "below") == 0) {
        out = ALARMS::AlarmOperator::Below;
        return true;
    }

    return false;
}

bool tryParseSeverity(JsonVariantConst value, ALARMS::AlarmSeverity& out) {
    if (value.is<int>()) {
        const int sevInt = value.as<int>();
        if (sevInt < static_cast<int>(ALARMS::AlarmSeverity::Info) ||
            sevInt > static_cast<int>(ALARMS::AlarmSeverity::Critical)) {
            return false;
        }
        out = static_cast<ALARMS::AlarmSeverity>(sevInt);
        return true;
    }

    const char* sevStr = value | static_cast<const char*>(nullptr);
    if (!sevStr) {
        return false;
    }

    if (strcmp(sevStr, "info") == 0) {
        out = ALARMS::AlarmSeverity::Info;
        return true;
    }
    if (strcmp(sevStr, "warning") == 0) {
        out = ALARMS::AlarmSeverity::Warning;
        return true;
    }
    if (strcmp(sevStr, "critical") == 0) {
        out = ALARMS::AlarmSeverity::Critical;
        return true;
    }

    return false;
}

bool tryParseNotifyChannels(JsonVariantConst value, ALARMS::NotifyChannel& out) {
    if (value.is<int>()) {
        const int channelsInt = value.as<int>();
        if (channelsInt < 0 || channelsInt > UINT8_MAX ||
            (channelsInt & ~static_cast<int>(kAllowedNotifyChannelsMask)) != 0) {
            return false;
        }
        out = static_cast<ALARMS::NotifyChannel>(channelsInt);
        return true;
    }

    if (!value.is<JsonArrayConst>()) {
        return false;
    }

    ALARMS::NotifyChannel channels = ALARMS::NotifyChannel::None;
    for (JsonVariantConst v : value.as<JsonArrayConst>()) {
        const char* ch = v | static_cast<const char*>(nullptr);
        if (!ch) {
            return false;
        }

        if (strcmp(ch, "telegram") == 0) {
            channels = channels | ALARMS::NotifyChannel::Telegram;
        } else if (strcmp(ch, "led") == 0) {
            channels = channels | ALARMS::NotifyChannel::Led;
        } else if (strcmp(ch, "webhook") == 0) {
            channels = channels | ALARMS::NotifyChannel::Webhook;
        } else if (strcmp(ch, "pushover") == 0) {
            channels = channels | ALARMS::NotifyChannel::Pushover;
        } else {
            return false;
        }
    }

    out = channels;
    return true;
}

bool hasDuplicateRuleId(const ALARMS::AlarmRulesSnapshot& parsed, const char* id) {
    if (!id || !*id) {
        return true;
    }

    for (uint8_t i = 0; i < parsed.ruleCount; i++) {
        if (strncmp(parsed.rules[i].id, id, ALARMS::kMaxIdLen) == 0) {
            return true;
        }
    }

    return false;
}

bool hasDuplicateRuleName(const ALARMS::AlarmRulesSnapshot& parsed, const char* name) {
    if (!name || !*name) {
        return true;
    }

    for (uint8_t i = 0; i < parsed.ruleCount; i++) {
        if (alarmNamesEqual(parsed.rules[i].name, name)) {
            return true;
        }
    }

    return false;
}

bool hasVisibleCharacter(const char* value) {
    if (!value) {
        return false;
    }

    while (*value) {
        if (*value != ' ' && *value != '\t' && *value != '\r' && *value != '\n') {
            return true;
        }
        value++;
    }

    return false;
}

const char* trimLeft(const char* value) {
    if (!value) {
        return "";
    }

    while (*value && std::isspace(static_cast<unsigned char>(*value))) {
        value++;
    }
    return value;
}

size_t trimmedLength(const char* value) {
    const char* begin = trimLeft(value);
    const char* end = begin + strlen(begin);
    while (end > begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        end--;
    }
    return static_cast<size_t>(end - begin);
}

bool alarmNamesEqual(const char* left, const char* right) {
    const char* leftBegin = trimLeft(left);
    const char* rightBegin = trimLeft(right);
    const size_t leftLen = trimmedLength(leftBegin);
    const size_t rightLen = trimmedLength(rightBegin);

    if (leftLen != rightLen) {
        return false;
    }

    for (size_t i = 0; i < leftLen; i++) {
        const auto leftChar = static_cast<unsigned char>(leftBegin[i]);
        const auto rightChar = static_cast<unsigned char>(rightBegin[i]);
        if (std::tolower(leftChar) != std::tolower(rightChar)) {
            return false;
        }
    }

    return true;
}

}  // namespace

// Shared logic: Deserializes a single rule from JSON into the struct.
// Handles validation, defaults, and type conversion.
// Returns false if rule is completely malformed (e.g. ID too long or timestamp out of range)
bool deserializeAlarmRule(JsonObject& rule, ALARMS::AlarmRule& r) {
    // 1. Validate ID and Name lengths before copying
    if (const char* rawId = rule[Keys::kId] | (const char*)nullptr) {
        if (strlen(rawId) >= ALARMS::kMaxIdLen || strlen(rawId) == 0) return false;
        strlcpy(r.id, rawId, sizeof(r.id));
    } else {
        return false; // ID is mandatory for a valid rule
    }
    
    if (const char* rawName = rule[Keys::kName] | (const char*)nullptr) {
        if (strlen(rawName) >= ALARMS::kMaxAlarmNameLen ||
            strlen(rawName) == 0 ||
            !hasVisibleCharacter(rawName)) {
            return false;
        }
        strlcpy(r.name, rawName, sizeof(r.name));
    } else {
        return false; // Name is mandatory
    }

    if (!rule[Keys::kBleDeviceMac].isNull()) {
        const char* mac =
            rule[Keys::kBleDeviceMac] | static_cast<const char*>(nullptr);
        if (!mac || strlen(mac) >= ALARMS::kBleMacLen) return false;
        strlcpy(r.bleDeviceMac, mac, sizeof(r.bleDeviceMac));
    }

    if (!rule[Keys::kGpioId].isNull()) {
        const char* gpioId =
            rule[Keys::kGpioId] | static_cast<const char*>(nullptr);
        if (!gpioId || strlen(gpioId) >= ALARMS::kGpioIdLen) return false;
        strlcpy(r.gpioId, gpioId, sizeof(r.gpioId));
    }

    // 2. Boolean/Enum fields — use temp variables for packed struct safety
    if (rule[Keys::kEnabled].is<bool>()) {
        bool enabled = rule[Keys::kEnabled].as<bool>();
        r.enabled = enabled;
    } else if (!rule[Keys::kEnabled].isNull()) {
        return false;
    }

    if (!rule[Keys::kSource].isNull()) {
        ALARMS::AlarmSource source = r.source;
        if (!tryParseSource(rule[Keys::kSource], source)) {
            return false;
        }
        r.source = source;
    }

    if (!rule[Keys::kOperator].isNull()) {
        ALARMS::AlarmOperator op = r.op;
        if (!tryParseOperator(rule[Keys::kOperator], op)) {
            return false;
        }
        r.op = op;
    }

    // 3. Numeric values with bounds checking
    if (rule[Keys::kThreshold].is<float>() || rule[Keys::kThreshold].is<int>()) {
        float th = rule[Keys::kThreshold];
        if (!std::isfinite(th) ||
            th < LIMITS::ALARMS::MIN_THRESHOLD ||
            th > LIMITS::ALARMS::MAX_THRESHOLD) {
            return false;
        }
        r.threshold = th;
    } else if (!rule[Keys::kThreshold].isNull()) {
        return false;
    }

    if (!rule[Keys::kSeverity].isNull()) {
        ALARMS::AlarmSeverity severity = r.severity;
        if (!tryParseSeverity(rule[Keys::kSeverity], severity)) {
            return false;
        }
        r.severity = severity;
    }

    // Notify Channels: Can be int (bitmask) or array of strings
    if (!rule[Keys::kNotifyChannels].isNull()) {
        ALARMS::NotifyChannel channels = r.notifyChannels;
        if (!tryParseNotifyChannels(rule[Keys::kNotifyChannels], channels)) {
            return false;
        }
        r.notifyChannels = channels;
    }

    if (!rule[Keys::kCooldownSeconds].isNull()) {
        if (!rule[Keys::kCooldownSeconds].is<int64_t>() &&
            !rule[Keys::kCooldownSeconds].is<uint64_t>()) {
            return false;
        }
        const int64_t cooldown = rule[Keys::kCooldownSeconds].as<int64_t>();
        if (cooldown < static_cast<int64_t>(LIMITS::ALARMS::MIN_COOLDOWN_SEC) ||
            cooldown > static_cast<int64_t>(LIMITS::ALARMS::MAX_COOLDOWN_SEC)) {
            return false;
        }
        r.cooldownSeconds = static_cast<uint32_t>(cooldown);
    }

    // 4. Timestamps (temp variable for packed safety)
    if (rule[Keys::kCreatedAt].is<uint32_t>()) {
        uint32_t ts = rule[Keys::kCreatedAt].as<uint32_t>();
        r.createdAt = ts;
    } else if (rule[Keys::kCreatedAt].is<uint64_t>()) {
        // Prevent out-of-range timestamp (larger than uint32_t)
        uint64_t ts = rule[Keys::kCreatedAt].as<uint64_t>();
        if (ts > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) return false;
        r.createdAt = static_cast<uint32_t>(ts);
    } else if (!rule[Keys::kCreatedAt].isNull()) {
        return false;
    }

    if (rule[Keys::kUpdatedAt].is<uint32_t>()) {
        uint32_t ts = rule[Keys::kUpdatedAt].as<uint32_t>();
        r.updatedAt = ts;
    } else if (rule[Keys::kUpdatedAt].is<uint64_t>()) {
        uint64_t ts = rule[Keys::kUpdatedAt].as<uint64_t>();
        if (ts > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) return false;
        r.updatedAt = static_cast<uint32_t>(ts);
    } else if (!rule[Keys::kUpdatedAt].isNull()) {
        return false;
    }

    // 5. Shelly Device IDs
    if (rule[Keys::kShellyDeviceIds].is<JsonArray>()) {
        JsonArray shellyIds = rule[Keys::kShellyDeviceIds].as<JsonArray>();
        if (shellyIds.size() > ALARMS::kMaxShellyPerRule) {
            return false;
        }
        r.clearShellyDevices();
        for (JsonVariant sdev : shellyIds) {
            const char* deviceId = sdev | static_cast<const char*>(nullptr);
            if (!deviceId || deviceId[0] == '\0' ||
                strlen(deviceId) >= ALARMS::kShellyIdLen) {
                return false;
            }
            for (uint8_t existingIndex = 0;
                 existingIndex < r.shellyDeviceCount;
                 ++existingIndex) {
                if (strncmp(r.shellyDeviceIds[existingIndex],
                            deviceId,
                            ALARMS::kShellyIdLen) == 0) {
                    return false;
                }
            }
            if (!r.addShellyDevice(deviceId)) {
                return false;
            }
        }
    } else if (!rule[Keys::kShellyDeviceIds].isNull()) {
        return false;
    }

    r.normalizeBooleanSemantics();

    return ALARMS::isValidAlarmRuleDefinition(r);
}

bool deserializeAlarmRules(JsonArray& rules, ALARMS::AlarmRulesSnapshot& parsed, AlarmRulesParseError* error) {
    // Before: parsed = AlarmRulesData{} could materialize another full-sized
    // temporary snapshot. memset() resets the caller-owned buffer in place.
    memset(&parsed, 0, sizeof(parsed));
    if (error) {
        *error = AlarmRulesParseError::None;
    }

    if (rules.size() > RTC::kMaxAlarmRules) {
        if (error) {
            *error = AlarmRulesParseError::TooManyRules;
        }
        return false;
    }

    for (JsonObject ruleObj : rules) {
        ALARMS::AlarmRule rule;
        if (!deserializeAlarmRule(ruleObj, rule)) {
            if (error) {
                *error = AlarmRulesParseError::InvalidRuleData;
            }
            return false;
        }

        if (hasDuplicateRuleId(parsed, rule.id)) {
            if (error) {
                *error = AlarmRulesParseError::DuplicateRuleId;
            }
            return false;
        }

        if (hasDuplicateRuleName(parsed, rule.name)) {
            if (error) {
                *error = AlarmRulesParseError::DuplicateRuleName;
            }
            return false;
        }

        parsed.rules[parsed.ruleCount++] = rule;
    }

    return true;
}

bool loadAlarms(JsonObject& obj) {
    // Parsed rules are regular CPU-owned config data, so a temporary PSRAM
    // buffer is safe and avoids building a ~2 KB object on the current stack.
    // Before: loadAlarms() kept that parsed snapshot as a local stack object.
    auto parsed = SYSTEM::MEMORY::makeUniqueInPsram<ALARMS::AlarmRulesSnapshot>();
    if (!parsed) {
        return false;
    }

    // The caller requires the top-level alarms section and this loader requires
    // its rules snapshot to be explicit. Treating an incomplete `alarms: {}`
    // object as an empty set would silently disarm all configured rules after a
    // truncated or partial write.
    if (!obj[Keys::kRules].is<JsonArray>()) {
        return false;
    }
    JsonArray rules = obj[Keys::kRules].as<JsonArray>();
    if (!deserializeAlarmRules(rules, *parsed)) {
        // Never partially load a corrupt rule set. Keeping the previous
        // (or cold-boot default) snapshot is safer than silently dropping
        // one alarm and presenting the remaining subset as authoritative.
        return false;
    }

    if (!ALARMS::RULES_CONFIG::update([&](ALARMS::AlarmRulesSnapshot& alarms) {
            alarms = *parsed;
        })) {
        return false;
    }

    // Do not touch the RTC runtime summary here. On a warm boot it may still
    // represent the previous LittleFS commit order; AlarmRuleManager maps the
    // full retained set by runtime identity and then writes a compact summary.
    // Cold boots already start with an empty summary via RTC::initDefaults().
    return true;
}

void saveAlarms(JsonObject& obj) {
    // Before: saveAlarms(obj, RULES_CONFIG::copy()) created another full
    // AlarmRulesSnapshot return value on the caller stack. Now the snapshot is
    // copied directly into a PSRAM-owned temporary buffer.
    auto rulesSnapshot = SYSTEM::MEMORY::makeUniqueInPsram<ALARMS::AlarmRulesSnapshot>();
    if (!rulesSnapshot || !ALARMS::RULES_CONFIG::copyTo(*rulesSnapshot)) {
        obj[Keys::kRules].to<JsonArray>();
        return;
    }

    saveAlarms(obj, *rulesSnapshot);
}

void saveAlarms(JsonObject& obj, const ALARMS::AlarmRulesSnapshot& rules) {
    saveAlarmRulesSnapshot(obj, rules);
}

} // namespace JSON
} // namespace CONFIG
