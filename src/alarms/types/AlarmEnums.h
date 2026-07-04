/**
 * @file AlarmEnums.h
 * @brief Enum types for the Alarms module
 */

#pragma once

#include <cstdint>

namespace ALARMS {

/** Signal source for alarm condition */
enum class AlarmSource : uint8_t {
    CO2 = 0,
    Temperature = 1,
    Humidity = 2,
    WifiMotion = 3,
    BleTemperature = 4,  // BLE thermometer temperature
    BleHumidity = 5,     // BLE thermometer humidity
    BleBattery = 6,      // BLE sensor battery percentage
    BleRssi = 7,         // BLE sensor signal strength
    WifiCsiMotion = 8,   // WiFi CSI motion boolean
    ImuTamper = 9,       // IMU tilt/shock tamper detector boolean
    GpioDigital = 10     // GPIO digital boolean channel
};

/** Comparison operator */
enum class AlarmOperator : uint8_t {
    Above = 0,
    Below = 1
};

/** Severity level */
enum class AlarmSeverity : uint8_t {
    Info = 0,
    Warning = 1,
    Critical = 2
};

/** Notification channel flags (bitmask) */
enum class NotifyChannel : uint8_t {
    None = 0,
    Telegram = 1 << 0,
    Led = 1 << 1,
    Webhook = 1 << 2,
    Pushover = 1 << 3
};

/** Alarm state */
enum class AlarmState : uint8_t {
    Cleared = 0,
    Triggered = 1
};

// ============================================================================
// Bitmask operators for NotifyChannel
// ============================================================================

inline NotifyChannel operator|(NotifyChannel a, NotifyChannel b) {
    return static_cast<NotifyChannel>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline NotifyChannel operator&(NotifyChannel a, NotifyChannel b) {
    return static_cast<NotifyChannel>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline bool hasChannel(NotifyChannel mask, NotifyChannel channel) {
    return (static_cast<uint8_t>(mask) & static_cast<uint8_t>(channel)) != 0;
}

}  // namespace ALARMS
