/**
 * @file AlarmLogic.cpp
 * @brief Implementation of pure business logic for alarms
 */

#include "AlarmLogic.h"

namespace ALARMS {

AlarmAction AlarmLogic::determineAction(
    const AlarmRule& rule, 
    const EvaluationResult& result, 
    bool wasUninitialized
) {
    AlarmAction action;
    action.reset();

    // Logic 1: Notification (Triggered & Cooldown elapsed)
    if (result.shouldNotify) {
        action.sendNotify = true;
    }

    // Logic 2: Shelly Control
    // Trigger if:
    // a) State changed (ON->OFF or OFF->ON)
    // b) OR this is the first run (wasUninitialized) -> Force sync
    if (rule.hasShellyDevices()) {
        if (result.stateChanged || wasUninitialized) {
            action.triggerShelly = true;
            // Always set target state to matched condition
            // If triggered=true -> Turn ON. If triggered=false -> Turn OFF.
            action.shellyState = result.triggered;
        }
    }

    // Logic 3: Cleared Notification (Transition Triggered -> Safe)
    if (result.stateChanged && !result.triggered) {
        action.sendClear = true;
    }

    return action;
}

void AlarmLogic::updateAggregate(
    const AlarmRule& rule,
    const AlarmRuntimeState& state,
    AlarmAggregateState& currentAggregate
) {
    // Only consider rules that have LED channel enabled
    if (!hasChannel(rule.notifyChannels, NotifyChannel::Led)) {
        return;
    }

    if (state.previouslyTriggered) {
        currentAggregate.active = true;
        
        // Severity aggregation (Critical > Warning > Info)
        // Also track the name of the highest-severity alarm
        bool updateName = false;
        
        if (rule.severity == AlarmSeverity::Critical) {
            if (currentAggregate.maxSeverity != AlarmSeverity::Critical) {
                updateName = true;
            }
            currentAggregate.maxSeverity = AlarmSeverity::Critical;
        } else if (rule.severity == AlarmSeverity::Warning && currentAggregate.maxSeverity != AlarmSeverity::Critical) {
            if (currentAggregate.maxSeverity != AlarmSeverity::Warning) {
                updateName = true;
            }
            currentAggregate.maxSeverity = AlarmSeverity::Warning;
        } else if (currentAggregate.alarmName[0] == '\0') {
            // First active alarm if no higher severity yet
            updateName = true;
        }
        
        if (updateName) {
            safeCopyAlarmName(currentAggregate.alarmName, rule.name);
        }
    }
}

} // namespace ALARMS
