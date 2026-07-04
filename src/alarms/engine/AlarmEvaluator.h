/**
 * @file AlarmEvaluator.h
 * @brief Stateless alarm condition evaluator
 * 
 * Evaluates sensor values against alarm rules.
 * No heap allocation - all operations on stack/static memory.
 * 
 * Design notes:
 * - Stateless evaluation logic
 * - Runtime state passed in/out as parameters
 * - Cooldown/trigger tracking is retained by the manager and committed separately
 *   on significant state transitions (not on every sample)
 */

#pragma once

#include "../types/AlarmTypes.h"
#include <cmath>

namespace ALARMS {

/**
 * Result of evaluating a single alarm rule
 */
struct EvaluationResult {
    bool triggered;           // Condition is met now
    bool shouldNotify;        // Should send notification (respects cooldown)
    bool stateChanged;        // Transition from cleared->triggered or triggered->cleared
    float currentValue;       // The value that was evaluated
    const AlarmRule* rule;    // Pointer to the rule that was evaluated
};

/**
 * Stateless alarm evaluator
 */
class AlarmEvaluator {
public:
    /**
     * Evaluate a single alarm rule against current sensor values
     * 
     * @param rule The alarm rule to evaluate
     * @param sensors Current sensor readings
     * @param state Runtime state for this rule (will be updated)
     * @param currentTimeMs Current time in milliseconds (millis())
     * @return Evaluation result
     */
    static inline EvaluationResult evaluate(
        const AlarmRule& rule,
        const AlarmInputData& sensors,
        AlarmRuntimeState& state,
        uint32_t currentTimeMs
    ) {
        EvaluationResult result;
        result.triggered = false;
        result.shouldNotify = false;
        result.stateChanged = false;
        result.currentValue = NAN;
        result.rule = &rule;
        
        // Skip disabled rules
        if (!rule.enabled) {
            return result;
        }
        
        // Get sensor value for this rule's source
        float value = getSensorValue(sensors, rule.source);
        result.currentValue = value;
        
        // Skip if sensor value is unavailable or non-finite. Infinite values
        // are sensor/runtime faults, not valid "extreme" readings to alarm on.
        if (!std::isfinite(value)) {
            return result;
        }
        
        // Check if condition is met
        bool conditionMet = checkCondition(value, rule.threshold, rule.op);
        result.triggered = conditionMet;
        
        // Detect state change
        result.stateChanged = (conditionMet != state.previouslyTriggered);
        
        // Update previous state
        state.previouslyTriggered = conditionMet;
        state.lastValue = value;  // Store value for BLE reporting
        
        // Determine if we should notify
        if (conditionMet) {
            // Notify on first trigger, and then periodically while still triggered
            // once the cooldown elapses (reminder behavior).
            if (state.lastTriggeredMs == 0 ||
                isCooldownElapsed(state.lastTriggeredMs, rule.cooldownSeconds, currentTimeMs)) {
                result.shouldNotify = true;
                state.lastTriggeredMs = currentTimeMs;
            }
        } else {
            // Reset cooldown tracking when alarm clears so next trigger is immediate
            state.lastTriggeredMs = 0;
        }
        
        return result;
    }
    
    /**
     * Get the sensor value for a given source
     * 
     * @param sensors Sensor snapshot
     * @param source Which sensor to read
     * @return Value or NAN if not available
     */
    static inline float getSensorValue(const AlarmInputData& sensors, AlarmSource source) {
        switch (source) {
            case AlarmSource::CO2:
                // Filter invalid readings (SCD4x error state 0xFFFF or initialization 0)
                if (sensors.co2 >= 65535.0f || sensors.co2 <= 0.0f) {
                    return NAN;
                }
                return sensors.co2;
            case AlarmSource::Temperature:
                return sensors.temperature;
            case AlarmSource::Humidity:
                return sensors.humidity;
            case AlarmSource::WifiMotion:
                return sensors.wifiVariance;
            case AlarmSource::WifiCsiMotion:
                return sensors.wifiCsiMotion;
            case AlarmSource::ImuTamper:
                return sensors.imuTamper;
            case AlarmSource::GpioDigital:
                return sensors.gpioDigital;
            case AlarmSource::BleTemperature:
                return sensors.bleTemp;
            case AlarmSource::BleHumidity:
                return sensors.bleHumid;
            case AlarmSource::BleBattery:
                return sensors.bleBattery;
            case AlarmSource::BleRssi:
                return sensors.bleRssi;
            default:
                return NAN;
        }
    }
    
    /**
     * Check if a value meets the alarm condition
     * 
     * @param value Current sensor value
     * @param threshold Alarm threshold
     * @param op Comparison operator
     * @return true if condition is met (alarm should trigger)
     */
    static inline bool checkCondition(float value, float threshold, AlarmOperator op) {
        switch (op) {
            case AlarmOperator::Above:
                return value > threshold;
            case AlarmOperator::Below:
                return value < threshold;
            default:
                return false;
        }
    }
    
    /**
     * Check if cooldown period has elapsed
     * 
     * @param lastTriggeredMs When alarm was last triggered
     * @param cooldownSeconds Cooldown period
     * @param currentTimeMs Current time
     * @return true if cooldown has passed
     */
    static inline bool isCooldownElapsed(uint32_t lastTriggeredMs, uint16_t cooldownSeconds, uint32_t currentTimeMs) {
        // Handle millis() overflow
        uint32_t elapsed;
        if (currentTimeMs >= lastTriggeredMs) {
            elapsed = currentTimeMs - lastTriggeredMs;
        } else {
            // Overflow occurred
            elapsed = (0xFFFFFFFF - lastTriggeredMs) + currentTimeMs + 1;
        }
        
        // Convert cooldown to milliseconds
        uint32_t cooldownMs = static_cast<uint32_t>(cooldownSeconds) * 1000;
        
        return elapsed >= cooldownMs;
    }

private:
    AlarmEvaluator() = delete; // Static-only class
};

}  // namespace ALARMS
