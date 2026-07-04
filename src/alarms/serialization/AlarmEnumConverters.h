/**
 * @file AlarmEnumConverters.h
 * @brief String <-> Enum conversion helpers for JSON serialization
 */

#pragma once

#include "../types/AlarmEnums.h"
#include <cstring>

namespace ALARMS {

// ============================================================================
// AlarmSource conversions
// ============================================================================

inline const char* sourceToString(AlarmSource source) {
    switch (source) {
        case AlarmSource::CO2: return "co2";
        case AlarmSource::Temperature: return "temperature";
        case AlarmSource::Humidity: return "humidity";
        case AlarmSource::WifiMotion: return "wifi_motion";
        case AlarmSource::BleTemperature: return "ble_temperature";
        case AlarmSource::BleHumidity: return "ble_humidity";
        case AlarmSource::BleBattery: return "ble_battery";
        case AlarmSource::BleRssi: return "ble_rssi";
        case AlarmSource::WifiCsiMotion: return "wifi_csi_motion";
        case AlarmSource::ImuTamper: return "imu_tamper";
        case AlarmSource::GpioDigital: return "gpio_digital";
        default: return "unknown";
    }
}

inline AlarmSource stringToSource(const char* str) {
    if (!str) return AlarmSource::Temperature;
    if (strcmp(str, "co2") == 0) return AlarmSource::CO2;
    if (strcmp(str, "temperature") == 0) return AlarmSource::Temperature;
    if (strcmp(str, "humidity") == 0) return AlarmSource::Humidity;
    if (strcmp(str, "wifi_motion") == 0) return AlarmSource::WifiMotion;
    if (strcmp(str, "ble_temperature") == 0) return AlarmSource::BleTemperature;
    if (strcmp(str, "ble_humidity") == 0) return AlarmSource::BleHumidity;
    if (strcmp(str, "ble_battery") == 0) return AlarmSource::BleBattery;
    if (strcmp(str, "ble_rssi") == 0) return AlarmSource::BleRssi;
    if (strcmp(str, "wifi_csi_motion") == 0) return AlarmSource::WifiCsiMotion;
    if (strcmp(str, "imu_tamper") == 0) return AlarmSource::ImuTamper;
    if (strcmp(str, "gpio_digital") == 0) return AlarmSource::GpioDigital;
    return AlarmSource::Temperature; // Default
}

// ============================================================================
// AlarmOperator conversions
// ============================================================================

inline const char* operatorToString(AlarmOperator op) {
    switch (op) {
        case AlarmOperator::Above: return "above";
        case AlarmOperator::Below: return "below";
        default: return "above";
    }
}

inline AlarmOperator stringToOperator(const char* str) {
    if (!str) return AlarmOperator::Above;
    if (strcmp(str, "below") == 0) return AlarmOperator::Below;
    return AlarmOperator::Above;
}

// ============================================================================
// AlarmSeverity conversions
// ============================================================================

inline const char* severityToString(AlarmSeverity sev) {
    switch (sev) {
        case AlarmSeverity::Info: return "info";
        case AlarmSeverity::Warning: return "warning";
        case AlarmSeverity::Critical: return "critical";
        default: return "warning";
    }
}

inline AlarmSeverity stringToSeverity(const char* str) {
    if (!str) return AlarmSeverity::Warning;
    if (strcmp(str, "info") == 0) return AlarmSeverity::Info;
    if (strcmp(str, "warning") == 0) return AlarmSeverity::Warning;
    if (strcmp(str, "critical") == 0) return AlarmSeverity::Critical;
    return AlarmSeverity::Warning;
}

// ============================================================================
// NotifyChannel conversions
// ============================================================================

/**
 * Parse notification channels from string array
 * @param channelStrings Array of channel name strings
 * @param count Number of strings in array
 * @return Bitmask of NotifyChannel
 */
inline NotifyChannel parseNotifyChannelsFromStrings(const char* const* channelStrings, size_t count) {
    NotifyChannel result = NotifyChannel::None;
    for (size_t i = 0; i < count; i++) {
        const char* ch = channelStrings[i];
        if (!ch) continue;
        if (strcmp(ch, "telegram") == 0) {
            result = result | NotifyChannel::Telegram;
        } else if (strcmp(ch, "led") == 0) {
            result = result | NotifyChannel::Led;
        } else if (strcmp(ch, "webhook") == 0) {
            result = result | NotifyChannel::Webhook;
        } else if (strcmp(ch, "pushover") == 0) {
            result = result | NotifyChannel::Pushover;
        }
    }
    return result;
}

}  // namespace ALARMS
