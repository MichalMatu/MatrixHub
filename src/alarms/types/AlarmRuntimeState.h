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
 * Size: 20 bytes (4+4+4+4+1+1+2 padding)
 */
struct __attribute__((packed)) AlarmRuntimeState {
    uint32_t lastTriggeredMs = 0;  // millis() when last triggered
    float lastValue = 0.0f;        // Last sensor value that was checked
    // Boot-scoped transition observability. Sequence zero is reserved for
    // "no committed transition in this boot"; wrap continues at one.
    uint32_t transitionSeq = 0;
    uint32_t transitionDeviceMillis = 0;
    bool previouslyTriggered = false;
    bool initialized = false;
    uint8_t _pad[2] = {0};  // Keep the packed layout at 20 bytes

    void recordTransition(uint32_t nowMs) {
        transitionSeq = transitionSeq == UINT32_MAX ? 1U : transitionSeq + 1U;
        transitionDeviceMillis = nowMs;
    }

    void resetTransitionObservation() {
        transitionSeq = 0;
        transitionDeviceMillis = 0;
    }
    
    void reset() {
        lastTriggeredMs = 0;
        lastValue = 0.0f;
        resetTransitionObservation();
        previouslyTriggered = false;
        initialized = false;
    }
};

}  // namespace ALARMS
