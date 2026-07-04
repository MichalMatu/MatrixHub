#pragma once

#include <Arduino.h>
#include "../RtcDefaultValues.h"
#include "../../../ble/BleTypes.h"

namespace RTC {

/** Maximum number of BLE sensors */
constexpr uint8_t kMaxBleSensors = 8;

/** Maximum number of discovered BLE devices (not yet saved) */
constexpr uint8_t kMaxBleDiscovered = 8;

/**
 * BLE sensor reading (runtime data cached in RTC, survives deep sleep)
 * Size: 16 bytes per sensor = 128 bytes for 8 sensors
 */
struct __attribute__((packed)) BleSensorReading {
    uint32_t lastSeenTime = 0;  // millis() timestamp
    float temperature = -999.0f;
    float humidity = 0.0f;
    uint8_t battery = 0;        // 0-100%
    int8_t rssi = 0;
    uint8_t _pad[2] = {0};      // Align to 16 bytes
};

/**
 * BLE discovery entry (for devices found but not yet saved to whitelist)
 * Size: 18 + 16 = 34 bytes per device = 272 bytes for 8 devices
 */
struct __attribute__((packed)) BleDiscoveryEntry {
    char mac[18] = {0};           // "XX:XX:XX:XX:XX:XX\0"
    uint32_t lastSeenTime = 0;    // millis() timestamp
    float temperature = -999.0f;
    float humidity = 0.0f;
    uint8_t battery = 0;
    int8_t rssi = 0;

    bool isEmpty() const { return mac[0] == '\0'; }
    void clear() { mac[0] = '\0'; lastSeenTime = 0; temperature = -999.0f; }
};

/**
 * BLE settings (fixed-size array instead of vector)
 */
struct __attribute__((packed)) BleData {
    bool enabled = Defaults::Ble::Enabled;
    BLE::BleSensorConfig sensors[kMaxBleSensors];       // Config (whitelist)
    // Legacy in-config cache kept for layout compatibility; live scanner
    // readings are stored in RTC::runtimeStats.bleReadings.
    BleSensorReading readings[kMaxBleSensors];
    uint8_t sensorCount = 0;
};

} // namespace RTC
