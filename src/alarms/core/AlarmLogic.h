/**
 * @file AlarmLogic.h
 * @brief Pure business logic for alarm actions and state aggregation
 * 
 * Decides WHAT to do (actions) based on alarm evaluation results.
 * Decoupled from hardware/IO for easier testing.
 */

#pragma once

#include "../types/AlarmRule.h"
#include "../types/AlarmRuntimeState.h"
#include "../engine/AlarmEvaluator.h" // For EvaluationResult
#include <cstring>

namespace ALARMS {

inline constexpr size_t kAlarmNameBufferLen = kMaxAlarmNameLen;
inline constexpr size_t kAlarmNameMaxChars = kAlarmNameBufferLen - 1;

/**
 * @brief Safely copy alarm name with guaranteed null termination
 * @param dest Destination buffer (must be at least kAlarmNameBufferLen bytes)
 * @param src Source string
 */
inline void safeCopyAlarmName(char* dest, const char* src) {
    if (!dest) {
        return;
    }
    if (!src) {
        dest[0] = '\0';
        return;
    }

    strlcpy(dest, src, kAlarmNameBufferLen);
}

/**
 * atomic action to be executed by the imperative shell (Coordinator)
 */
struct AlarmAction {
    bool triggerShelly;      // Action: Change Shelly State
    bool shellyState;        // Target Shelly State (ON/OFF)
    bool sendNotify;         // Action: Send Alarm Notification
    bool sendClear;          // Action: Send Cleared Notification
    
    // Helper to check if any action is required
    bool hasAction() const {
        return triggerShelly || sendNotify || sendClear;
    }
    
    void reset() {
        triggerShelly = false;
        shellyState = false;
        sendNotify = false;
        sendClear = false;
    }
};

/**
 * Result of global state aggregation (e.g. LED latching)
 */
struct AlarmAggregateState {
    bool active;
    AlarmSeverity maxSeverity;
    char alarmName[kAlarmNameBufferLen];  // Name of highest-severity active alarm
    
    void reset() {
        active = false;
        maxSeverity = AlarmSeverity::Info;
        alarmName[0] = '\0';
    }
};

class AlarmLogic {
public:
    /**
     * @brief Determine actions for a single rule based on evaluation
     * 
     * @param rule The alarm rule configuration
     * @param result The evaluation result (triggered, etc.)
     * @param wasUninitialized True if this is the first run for this rule (affects Shelly sync)
     * @return AlarmAction struct describing what needs to be done
     */
    static AlarmAction determineAction(
        const AlarmRule& rule, 
        const EvaluationResult& result, 
        bool wasUninitialized
    );

    /**
     * @brief Update running aggregate state for LED latching
     * 
     * @param rule The alarm rule
     * @param state The runtime state (to check previouslyTriggered)
     * @param currentAggregate The current running aggregate to update
     */
    static void updateAggregate(
        const AlarmRule& rule,
        const AlarmRuntimeState& state,
        AlarmAggregateState& currentAggregate
    );
};

} // namespace ALARMS
