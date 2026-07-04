#pragma once

#include <cstring>
#include "ShellyConfig.h"

namespace SHELLY {

// ============================================================================
// Device Data Structure
// ============================================================================

/**
 * Represents a Shelly relay device with configuration and runtime state.
 */
struct __attribute__((packed)) ShellyDevice {
    // Configuration (persisted)
    char id[kMaxShellyId];          // Unique ID
    char name[kMaxShellyName];      // Display name
    char ip[kMaxShellyIp];          // IP Address
    uint8_t relayIndex;             // Relay index (0-3)
    bool enabled;                   // Is active in config
    uint8_t generation = 2;         // 1 = Gen1 (Shelly1/2.5), 2 = Gen2/3 (Plus/Pro)
    
    // Runtime state (not persisted)
    bool isOn = false;              // Last known relay state
    bool isOnline = false;          // Is device reachable
    uint8_t failedPolls = 0;        // Transient error counter for debounce
    uint8_t zeroPowerCount = 0;     // Consecutive 0W readings debounce counter
    unsigned long lastUpdate = 0;   // Timestamp of last successful poll
    
    // Power metering
    float power = 0.0f;             // Watts
    float energy = 0.0f;            // Watt-minutes (from 'total')
    float voltage = 0.0f;           // Volts (if available)
    float current = 0.0f;           // Amps (calculated or from device)
    float temperature = 0.0f;       // Celsius
    uint16_t interval = 30000;  // Increased to 30s to reduce load and overheating
    uint8_t pollBackoff = 1;       // Per-device backoff multiplier
    int8_t rssi = 0;               // dBm

    ShellyDevice() {
        memset(id, 0, kMaxShellyId);
        memset(name, 0, kMaxShellyName);
        memset(ip, 0, kMaxShellyIp);
        relayIndex = 0;
        enabled = true;
        generation = 2;
        isOn = false;
        isOnline = false;
        lastUpdate = 0;
        power = 0.0f;
        energy = 0.0f;
        voltage = 0.0f;
        current = 0.0f;
        temperature = 0.0f;
        pollBackoff = 1;
        rssi = 0;
    }

    /**
     * Check if device has valid required fields
     */
    bool isValid() const {
        return strlen(id) > 0 && strlen(ip) > 0;
    }
};

// ============================================================================
// Command Structure for Async Queue
// ============================================================================

/**
 * Command structure for the async worker queue.
 * Supports future extension with different command types.
 */
struct ShellyCommand {
    enum Type { 
        SET_RELAY,      // Turn relay on/off
        // Future: REFRESH_STATUS, RESTART_DEVICE, etc.
    };
    
    Type type;
    char id[kMaxShellyId];
    bool value;  // For SET_RELAY: true = ON, false = OFF

    ShellyCommand() : type(SET_RELAY), value(false) {
        memset(id, 0, kMaxShellyId);
    }
};

/**
 * parsed device status (transient)
 */
struct ShellyStatus {
    bool isOn;
    bool hasPower;
    float power;    // Watts
    float energy;   // Watt-minutes
    float voltage;  // Volts
    float current;  // Amps
    float temperature; // Celsius
    int8_t rssi;      // dBm
    
    ShellyStatus() : isOn(false), hasPower(false), power(0), energy(0), voltage(0), current(0), temperature(0), rssi(0) {}
};

} // namespace SHELLY
