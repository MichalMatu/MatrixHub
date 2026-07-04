#pragma once

#include <Arduino.h>
#include "../../../alarms/types/AlarmRule.h"
#include "../../../alarms/types/AlarmRuntimeState.h"

namespace ALARMS {

/**
 * Full alarm rules snapshot stored in PSRAM / serialized config.
 * The rule definitions are no longer retained in RTC.
 */
struct __attribute__((packed)) AlarmRulesSnapshot {
    AlarmRule rules[kMaxRules];
    uint8_t ruleCount = 0;
};

/**
 * Retained runtime summary stored in RTC.
 * Keeps cooldown/trigger state only, not the rule definitions themselves.
 */
struct __attribute__((packed)) AlarmRuntimeSummary {
    AlarmRuntimeState runtimeStates[kMaxRules];
    uint8_t ruleCount = 0;
    uint8_t enabledCount = 0;
    uint8_t _pad[2] = {0};
};

/**
 * Combined in-memory snapshot used by runtime consumers that need both the
 * PSRAM rule definitions and the retained RTC runtime state together.
 */
struct __attribute__((packed)) AlarmSnapshot {
    AlarmRule rules[kMaxRules];
    AlarmRuntimeState runtimeStates[kMaxRules];
    uint8_t ruleCount = 0;
};

}  // namespace ALARMS

namespace RTC {

/** Maximum number of alarm rules (legacy RTC-facing name kept for compatibility). */
constexpr uint8_t kMaxAlarmRules = ALARMS::kMaxRules;

/** Backward-compatible alias: full rule definitions live in PSRAM/config. */
using AlarmRulesData = ALARMS::AlarmRulesSnapshot;

/** Backward-compatible alias for the retained RTC runtime summary. */
using AlarmRuntimeData = ALARMS::AlarmRuntimeSummary;

/** Backward-compatible alias for the combined runtime snapshot. */
using AlarmData = ALARMS::AlarmSnapshot;

} // namespace RTC
