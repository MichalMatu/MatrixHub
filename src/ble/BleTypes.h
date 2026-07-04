/**
 * @file BleTypes.h
 * @brief BLE scanner data structures
 *
 * All structures use fixed-size arrays to avoid heap allocation.
 * This prevents memory fragmentation during BLE operations.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include "../config/Network.h"

namespace BLE {

/**
 * @brief Configuration Value Object for scanner-only BLE.
 */
struct BleConfig {
    bool enabled = false;
};

// ============================================================================
// Sensor Payload Types
// ============================================================================

struct TpData {
    float temperature;
    float humidity;
    uint8_t battery; // 0-100%
    bool valid;
};

// Callback type for when valid sensor data is found
using TpDataCallback = std::function<void(const char* mac, const TpData& data, int rssi)>;

/** Maximum length for device name */
constexpr size_t kMaxDeviceNameLen = 24;

/** Maximum length for MAC address string */
constexpr size_t kMaxMacAddressLen = 18;  // "XX:XX:XX:XX:XX:XX\0"

/**
 * Configuration for a monitored BLE sensor (whitelist)
 */
struct __attribute__((packed)) BleSensorConfig {
    char mac[kMaxMacAddressLen] = {0};
    char alias[kMaxDeviceNameLen] = {0};

    bool operator==(const BleSensorConfig& other) const {
        return strncmp(mac, other.mac, kMaxMacAddressLen) == 0;
    }

    bool operator==(const char* otherMac) const {
        return strncmp(mac, otherMac, kMaxMacAddressLen) == 0;
    }

    bool isEmpty() const { return mac[0] == '\0'; }
};

/**
 * BLE service status for scanner-only monitoring/API.
 */
struct BleStatus {
    bool scannerActive = false;
};

}  // namespace BLE
