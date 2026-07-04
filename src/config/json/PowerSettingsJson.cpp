#include "PowerSettingsJson.h"
#include "../../config/App.h"
#include "../../config/Hardware.h"
#include "../../system/rtc/RtcConfig.h"

#include <algorithm>

namespace CONFIG {
namespace JSON {

namespace {
uint32_t clampU32(uint32_t value, uint32_t minValue, uint32_t maxValue) {
    return std::max(minValue, std::min(value, maxValue));
}

uint8_t clampU8(uint32_t value, uint8_t minValue, uint8_t maxValue) {
    return static_cast<uint8_t>(std::max<uint32_t>(minValue, std::min<uint32_t>(value, maxValue)));
}

void ensureSafeWakeSources(RTC::PowerData& data) {
    if (!data.sleepEnabled) {
        return;
    }
    if (data.wakeTimerEnabled || data.wakeButtonEnabled || data.wakeTouchEnabled) {
        return;
    }

    // Persisted configs can outlive firmware versions. Prefer a timer wake
    // over accepting a sleep-enabled config that has no path back to runtime.
    data.wakeTimerEnabled = true;
}
}  // namespace

// ---------------------------------------------------------------------------
// deserializePower  —  shared by loadPower (file) AND API endpoint.
// Updates only fields present in JSON (partial-update safe).
// ---------------------------------------------------------------------------
void deserializePower(JsonObject& obj, RTC::PowerData& data) {
    if (obj[Keys::kSleepEnabled].is<bool>()) {
        data.sleepEnabled = obj[Keys::kSleepEnabled].as<bool>();
    }
    if (obj[Keys::kInactivityTimeoutMs].is<uint32_t>()) {
        uint32_t v = obj[Keys::kInactivityTimeoutMs].as<uint32_t>();
        v = clampU32(v, POWER::INACTIVITY_TIMEOUT_MIN_MS, POWER::INACTIVITY_TIMEOUT_MAX_MS);
        data.inactivityTimeoutMs = v;
    }
    if (obj[Keys::kGraceAfterBootMs].is<uint32_t>()) {
        uint32_t v = obj[Keys::kGraceAfterBootMs].as<uint32_t>();
        v = clampU32(v, POWER::GRACE_MIN_MS, POWER::GRACE_MAX_MS);
        data.graceAfterBootMs = v;
    }
    if (obj[Keys::kWakeTimerEnabled].is<bool>()) {
        data.wakeTimerEnabled = obj[Keys::kWakeTimerEnabled].as<bool>();
    }
    if (obj[Keys::kWakeButtonEnabled].is<bool>()) {
        data.wakeButtonEnabled = obj[Keys::kWakeButtonEnabled].as<bool>();
    }
    if (obj[Keys::kWakeTouchEnabled].is<bool>()) {
        data.wakeTouchEnabled = obj[Keys::kWakeTouchEnabled].as<bool>();
    }
    if (obj[Keys::kWakeIntervalMs].is<uint32_t>()) {
        uint32_t v = obj[Keys::kWakeIntervalMs].as<uint32_t>();
        data.wakeIntervalMs = clampU32(v, POWER::WAKE_INTERVAL_MIN_MS, POWER::WAKE_INTERVAL_MAX_MS);
    }
    if (obj[Keys::kTimerWakeAwakeMs].is<uint32_t>()) {
        uint32_t v = obj[Keys::kTimerWakeAwakeMs].as<uint32_t>();
        data.timerWakeAwakeMs = clampU32(v, POWER::TIMER_WAKE_AWAKE_MIN_MS, POWER::TIMER_WAKE_AWAKE_MAX_MS);
    }
    if (obj[Keys::kButtonWakeAwakeMs].is<uint32_t>()) {
        uint32_t v = obj[Keys::kButtonWakeAwakeMs].as<uint32_t>();
        data.buttonWakeAwakeMs = clampU32(v, POWER::BUTTON_WAKE_AWAKE_MIN_MS, POWER::BUTTON_WAKE_AWAKE_MAX_MS);
    }
    if (obj[Keys::kWakeTouchGpio].is<uint32_t>()) {
        uint32_t v = obj[Keys::kWakeTouchGpio].as<uint32_t>();
        data.wakeTouchGpio = clampU8(v, POWER::TOUCH_WAKE_GPIO_MIN, POWER::TOUCH_WAKE_GPIO_MAX);
    }
    if (obj[Keys::kWakeTouchThreshold].is<uint32_t>()) {
        uint32_t v = obj[Keys::kWakeTouchThreshold].as<uint32_t>();
        data.wakeTouchThreshold = clampU32(
            v,
            POWER::TOUCH_WAKE_THRESHOLD_MIN,
            POWER::TOUCH_WAKE_THRESHOLD_MAX);
    }
}

void loadPower(JsonObject& obj) {
    RTC::updateConfigSection(&RTC::ConfigStore::power, [&](RTC::PowerData& power) {
        power = RTC::PowerData{};
        deserializePower(obj, power);
        ensureSafeWakeSources(power);
    });
}

void savePower(JsonObject& obj) {
    RTC::PowerData p{};
    RTC::withConfig([&](const RTC::ConfigStore& cfg) {
        p = cfg.power;
    });
    obj[Keys::kInactivityTimeoutMs] = p.inactivityTimeoutMs;
    obj[Keys::kGraceAfterBootMs] = p.graceAfterBootMs;
    obj[Keys::kSleepEnabled] = p.sleepEnabled;
    obj[Keys::kWakeTimerEnabled] = p.wakeTimerEnabled;
    obj[Keys::kWakeButtonEnabled] = p.wakeButtonEnabled;
    obj[Keys::kWakeTouchEnabled] = p.wakeTouchEnabled;
    obj[Keys::kWakeIntervalMs] = p.wakeIntervalMs;
    obj[Keys::kTimerWakeAwakeMs] = p.timerWakeAwakeMs;
    obj[Keys::kButtonWakeAwakeMs] = p.buttonWakeAwakeMs;
    obj[Keys::kWakeTouchGpio] = p.wakeTouchGpio;
    obj[Keys::kWakeTouchThreshold] = p.wakeTouchThreshold;
}

} // namespace JSON
} // namespace CONFIG
