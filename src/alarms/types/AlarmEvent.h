/**
 * @file AlarmEvent.h
 * @brief AlarmEvent structure - triggered alarm record
 * 
 * Stored in ring buffer for history tracking.
 * Fixed-size, no dynamic allocation.
 */

#pragma once

#include "AlarmConstants.h"
#include "AlarmEnums.h"
#include <cstring>
#include <cstdint>

namespace ALARMS {

/**
 * Alarm event (triggered alarm record)
 */
struct AlarmEvent {
    uint32_t timestamp;           // Unix timestamp when triggered
    char ruleId[kMaxIdLen];       // Which rule triggered
    AlarmSeverity severity;       // Severity at time of trigger
    AlarmState state;             // Triggered or Cleared
    float value;                  // Actual sensor value that triggered
    float threshold;              // Threshold that was crossed
    bool telegramSent;            // Was Telegram notification sent
    
    AlarmEvent() {
        timestamp = 0;
        memset(ruleId, 0, kMaxIdLen);
        severity = AlarmSeverity::Info;
        state = AlarmState::Cleared;
        value = 0.0f;
        threshold = 0.0f;
        telegramSent = false;
    }
};

}  // namespace ALARMS
