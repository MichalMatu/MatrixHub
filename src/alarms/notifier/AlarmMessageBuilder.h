/**
 * @file AlarmMessageBuilder.h
 * @brief Builds human-readable alarm notification messages
 * 
 * Extracted from AlarmNotifier to keep modules small and focused.
 * Uses fixed-size buffers - no heap allocation.
 * 
 * Header-only implementation for testability (native unit tests).
 */

#pragma once

#include "../types/AlarmTypes.h"
#include "../engine/AlarmEvaluator.h"
#include "../serialization/AlarmEnumConverters.h"
#include <cstdio>

namespace ALARMS {

/**
 * Builds notification messages for alarm events.
 * 
 * Pure utility class with no state - all methods are static inline.
 */
class AlarmMessageBuilder {
public:
    /**
     * Build notification message for alarm trigger or clear event.
     * 
     * @param eval Evaluation result containing rule and current value
     * @param buffer Output buffer for the message
     * @param bufferSize Size of the output buffer
     * @param isCleared true if this is a "cleared" notification
     * @return Length of message written (excluding null terminator)
     */
    static inline size_t build(
        const EvaluationResult& eval, 
        char* buffer, 
        size_t bufferSize,
        bool isCleared = false
    ) {
        if (!eval.rule || bufferSize == 0) {
            return 0;
        }
        
        const AlarmRule& rule = *eval.rule;
        const char* emoji = severityEmoji(rule.severity);
        const char* source = sourceName(rule.source);
        const char* unit = sourceUnit(rule.source);
        
        int len;
        if (isCleared) {
            len = snprintf(buffer, bufferSize,
                "✅ *Alarm Cleared*\n"
                "Rule: %s\n"
                "%s: %.1f%s (threshold: %.1f%s)",
                rule.name,
                source,
                eval.currentValue, unit,
                rule.threshold, unit
            );
        } else {
            const char* opSymbol = (rule.op == AlarmOperator::Above) ? ">" : "<";
            len = snprintf(buffer, bufferSize,
                "%s *Alarm: %s*\n"
                "Severity: %s\n"
                "%s: %.1f%s %s %.1f%s",
                emoji,
                rule.name,
                severityToString(rule.severity),
                source,
                eval.currentValue, unit,
                opSymbol,
                rule.threshold, unit
            );
        }
        
        if (len < 0) {
            return 0;
        }
        
        return static_cast<size_t>(len) < bufferSize ? static_cast<size_t>(len) : bufferSize - 1;
    }

    /** Get emoji for severity level */
    static inline const char* severityEmoji(AlarmSeverity severity) {
        switch (severity) {
            case AlarmSeverity::Info:     return "ℹ️";
            case AlarmSeverity::Warning:  return "⚠️";
            case AlarmSeverity::Critical: return "🚨";
            default:                      return "🔔";
        }
    }
    
    /** Get human-readable source name */
    static inline const char* sourceName(AlarmSource source) {
        switch (source) {
            case AlarmSource::CO2:            return "CO₂";
            case AlarmSource::Temperature:    return "Temperature";
            case AlarmSource::Humidity:       return "Humidity";
            case AlarmSource::WifiMotion:     return "WiFi Motion";
            case AlarmSource::WifiCsiMotion:  return "CSI Motion";
            case AlarmSource::ImuTamper:      return "IMU Tamper";
            case AlarmSource::GpioDigital:    return "GPIO Digital";
            case AlarmSource::BleTemperature: return "BLE Temperature";
            case AlarmSource::BleHumidity:    return "BLE Humidity";
            case AlarmSource::BleBattery:     return "BLE Battery";
            case AlarmSource::BleRssi:        return "BLE RSSI";
            default:                          return "Unknown";
        }
    }
    
    /** Get unit suffix for source (e.g. " ppm", "°C") */
    static inline const char* sourceUnit(AlarmSource source) {
        switch (source) {
            case AlarmSource::CO2:            return " ppm";
            case AlarmSource::Temperature:    return "°C";
            case AlarmSource::Humidity:       return "%";
            case AlarmSource::WifiMotion:     return "";
            case AlarmSource::WifiCsiMotion:  return "";
            case AlarmSource::ImuTamper:      return "";
            case AlarmSource::GpioDigital:    return "";
            case AlarmSource::BleTemperature: return "°C";
            case AlarmSource::BleHumidity:    return "%";
            case AlarmSource::BleBattery:     return "%";
            case AlarmSource::BleRssi:        return " dBm";
            default:                          return "";
        }
    }

private:
    AlarmMessageBuilder() = delete; // Static-only class
};

}  // namespace ALARMS
