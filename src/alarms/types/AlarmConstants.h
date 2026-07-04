/**
 * @file AlarmConstants.h
 * @brief Compile-time constants for the Alarms module
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace ALARMS {

/** Maximum number of alarm rules */
constexpr uint8_t kMaxRules = 8;

/** Timeout for Mutex operations (milliseconds) */
constexpr uint32_t kAlarmMutexTimeoutMs = 5000;

/**
 * Short timeout for alarm evaluation on hot runtime paths (sensor/WiFi sensing).
 * Runtime producers should skip one evaluation pass under contention rather than
 * stall the sensor loop for hundreds or thousands of milliseconds.
 */
constexpr uint32_t kAlarmEvalMutexTimeoutMs = 25;

/** Maximum length of alarm name (including null terminator) */
constexpr size_t kMaxAlarmNameLen = 64;

/** Maximum length of rule ID */
constexpr uint8_t kMaxIdLen = 32;

/** Maximum length of BLE device MAC address (e.g., "A4:C1:38:XX:XX:XX\0") */
constexpr uint8_t kBleMacLen = 18;

/** Maximum length of GPIO channel ID */
constexpr uint8_t kGpioIdLen = 16;

/** Timeout for BLE readings to be considered stale (milliseconds) */
constexpr uint32_t kBleStaleTimeoutMs = 60 * 1000;  // 1 minute

/** Ring buffer size for alarm events history */
constexpr uint8_t kMaxEvents = 16;

/** Maximum Shelly devices that can be triggered per alarm rule */
constexpr uint8_t kMaxShellyPerRule = 4;

/** Maximum length of Shelly device ID (must match SHELLY::kMaxShellyId) */
constexpr uint8_t kShellyIdLen = 32;

}  // namespace ALARMS
