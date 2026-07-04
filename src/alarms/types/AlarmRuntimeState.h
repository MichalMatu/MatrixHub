/**
 * @file AlarmRuntimeState.h
 * @brief Runtime state for alarm rules (cooldown, trigger tracking)
 * 
 * NOTE: This struct is also defined identically in RTC::AlarmRuntimeState
 * for RTC memory storage. Keep both in sync!
 */

#pragma once

#include <cstdint>

namespace ALARMS {

/**
 * Runtime state for a single alarm rule
 * 
 * Tracks cooldown and previous state.
 * Now stored in RTC memory to survive deep sleep.
 * Size: 12 bytes (4+4+1+1+2 padding)
 */
struct __attribute__((packed)) AlarmRuntimeState {
    uint32_t lastTriggeredMs = 0;  // millis() when last triggered
    float lastValue = 0.0f;        // Last sensor value that was checked
    bool previouslyTriggered = false;
    bool initialized = false;
    uint8_t _pad[2] = {0};  // Align to 12 bytes
    
    void reset() {
        lastTriggeredMs = 0;
        lastValue = 0.0f;
        previouslyTriggered = false;
        initialized = false;
    }
};

}  // namespace ALARMS
