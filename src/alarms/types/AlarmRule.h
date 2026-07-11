/**
 * @file AlarmRule.h
 * @brief AlarmRule structure - single alarm rule definition
 * 
 * Fixed-size structure, no dynamic allocation.
 * Memory: ~80 bytes per rule.
 */

#pragma once

#include "AlarmConstants.h"
#include "AlarmEnums.h"
#include <cstring>

namespace ALARMS {

/**
 * Single alarm rule
 */
struct __attribute__((packed)) AlarmRule {
    char id[kMaxIdLen];           // Unique identifier
    char name[kMaxAlarmNameLen];  // Human-readable name
    bool enabled;                 // Is rule active
    AlarmSource source;           // What to monitor
    AlarmOperator op;             // Comparison type
    float threshold;              // Trigger value
    AlarmSeverity severity;       // Alert level
    NotifyChannel notifyChannels; // Bitmask of channels
    uint32_t cooldownSeconds;     // Min time between notifications
    uint32_t createdAt;           // Unix timestamp
    uint32_t updatedAt;           // Unix timestamp
    
    // BLE device MAC for all BLE-backed alarm sources
    char bleDeviceMac[kBleMacLen];  // "XX:XX:XX:XX:XX:XX\0", empty if not BLE source

    // GPIO channel id for GPIO-backed alarm sources
    char gpioId[kGpioIdLen];
    
    // Shelly device IDs to control when alarm triggers
    char shellyDeviceIds[kMaxShellyPerRule][kShellyIdLen];
    uint8_t shellyDeviceCount;
    
    /** Default constructor - initializes to safe defaults */
    AlarmRule() {
        memset(id, 0, kMaxIdLen);
        memset(name, 0, kMaxAlarmNameLen);
        enabled = false;
        source = AlarmSource::Temperature;
        op = AlarmOperator::Above;
        threshold = 0.0f;
        severity = AlarmSeverity::Warning;
        // Match the frontend DEFAULT_ALARM_RULE so new drafts and any firmware-side
        // fallback/default construction stay LED-only unless the user explicitly
        // enables remote channels later. If defaults ever diverge, debug both
        // this constructor and interface/src/lib/types/domain/alarms.ts.
        notifyChannels = NotifyChannel::Led;
        cooldownSeconds = 300;
        createdAt = 0;
        updatedAt = 0;
        memset(bleDeviceMac, 0, kBleMacLen);
        memset(gpioId, 0, kGpioIdLen);
        
        // Initialize Shelly fields
        shellyDeviceCount = 0;
        for (uint8_t i = 0; i < kMaxShellyPerRule; i++) {
            memset(shellyDeviceIds[i], 0, kShellyIdLen);
        }
    }
    
    /** Check if rule is valid (has ID and name) */
    bool isValid() const {
        return id[0] != '\0' && name[0] != '\0';
    }
    
    /** Check if this rule matches given source */
    bool matchesSource(AlarmSource s) const {
        return source == s;
    }
    
    /** Check if this rule uses a BLE source */
    bool isBleSource() const {
        return source == AlarmSource::BleTemperature ||
               source == AlarmSource::BleHumidity ||
               source == AlarmSource::BleBattery ||
               source == AlarmSource::BleRssi;
    }

    bool isGpioSource() const {
        return source == AlarmSource::GpioDigital;
    }

    bool isBooleanLikeSource() const {
        return source == AlarmSource::WifiCsiMotion ||
               source == AlarmSource::ImuTamper ||
               source == AlarmSource::GpioDigital;
    }

    // Boolean-like sources have one product meaning: true/detected is alarm.
    // Canonicalizing here prevents stale/API-authored `below 0.5` rules from
    // silently presenting as "motion detected" while behaving in reverse.
    void normalizeBooleanSemantics() {
        if (isBooleanLikeSource()) {
            op = AlarmOperator::Above;
            threshold = 0.5f;
        }
    }
    
    /** Check if rule has any Shelly devices configured */
    bool hasShellyDevices() const {
        return shellyDeviceCount > 0;
    }
    
    /** Add a Shelly device ID to the rule */
    bool addShellyDevice(const char* deviceId) {
        if (shellyDeviceCount >= kMaxShellyPerRule) {
            return false;
        }
        if (!deviceId) {
            return false;
        }
        size_t length = 0;
        while (length < kShellyIdLen && deviceId[length] != '\0') {
            ++length;
        }
        if (length == 0 || length >= kShellyIdLen) {
            return false;
        }
        for (uint8_t i = 0; i < shellyDeviceCount; ++i) {
            if (strncmp(shellyDeviceIds[i], deviceId, kShellyIdLen) == 0) {
                return false;
            }
        }
        memcpy(shellyDeviceIds[shellyDeviceCount], deviceId, length + 1);
        shellyDeviceCount++;
        return true;
    }
    
    /** Clear all Shelly devices */
    void clearShellyDevices() {
        shellyDeviceCount = 0;
        for (uint8_t i = 0; i < kMaxShellyPerRule; i++) {
            memset(shellyDeviceIds[i], 0, kShellyIdLen);
        }
    }
};

}  // namespace ALARMS
