#pragma once

/**
 * BTHome v2 BLE Advertisement Parser
 * 
 * Parses BTHome v2 format used by PVVX/ATC firmware on Xiaomi thermometers (LYWSD03MMC, etc.)
 * 
 * Format specification: https://bthome.io/format/
 * 
 * BTHome v2 uses Service Data with UUID 0xFCD2.
 * First byte after UUID is device info byte (version in bits 5-7, should be 0b010 for v2).
 * Following bytes are measurements in format: [Object ID][Value bytes...]
 * 
 * Common Object IDs:
 * - 0x01: Battery (1 byte, uint8, %)
 * - 0x02: Temperature (2 bytes, int16 LE, factor 0.01, °C)
 * - 0x03: Humidity (2 bytes, uint16 LE, factor 0.01, %)
 * - 0x45: Temperature (2 bytes, int16 LE, factor 0.1, °C) - precise
 * - 0x46: Humidity (2 bytes, uint16 LE, factor 0.1, %) - precise
 */

#include <cstdint>
#include <cstddef>

namespace BLE {

// BTHome Service UUID (16-bit): 0xFCD2
static constexpr uint16_t BTHOME_SERVICE_UUID = 0xFCD2;

// Little-endian byte helpers (inline, no overhead)
static inline int16_t readInt16LE(const uint8_t* p) {
    return static_cast<int16_t>(p[0] | (p[1] << 8));
}

static inline uint16_t readUint16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

struct BtHomeData {
    float temperature;
    float humidity;
    uint8_t battery;       // 0-100%
    uint16_t voltage_mv;   // Battery voltage in mV (optional)
    bool hasTemperature;
    bool hasHumidity;
    bool hasBattery;
    bool hasVoltage;
    bool valid;            // True if at least one measurement was parsed
    bool encrypted;        // True if packet is encrypted (we can't decode without key)
};

class BtHomeParser {
public:
    /**
     * Check if this Service Data belongs to BTHome v2
     * @param uuid 16-bit UUID from service data
     * @return true if UUID is 0xFCD2
     */
    static bool isBtHome(uint16_t uuid) {
        return uuid == BTHOME_SERVICE_UUID;
    }

    /**
     * Parse BTHome v2 Service Data payload
     * @param data Service Data after UUID (starts with device info byte)
     * @param length Length of data
     * @return Parsed BtHomeData structure
     */
    static BtHomeData parse(const uint8_t* data, size_t length) {
        BtHomeData result = {};
        
        if (!data || length < 2) {
            return result;
        }

        // First byte is device info:
        // Bits 0: Encryption flag (0 = no encryption)
        // Bit 1: Reserved
        // Bit 2: Trigger based device flag
        // Bits 5-7: BTHome Version (010 = v2)
        uint8_t deviceInfo = data[0];
        
        // Check if encrypted
        result.encrypted = (deviceInfo & 0x01) != 0;
        if (result.encrypted) {
            // Cannot decode encrypted packets without key
            // User should disable encryption in PVVX settings for ESP32 integration
            return result;
        }

        // Check version (bits 5-7 should be 0b010 for v2)
        uint8_t version = (deviceInfo >> 5) & 0x07;
        if (version != 2) {
            // Not BTHome v2, might be v1 or unsupported
            return result;
        }

        // Parse measurements starting from byte 1
        size_t pos = 1;
        bool parsing = true;
        
        while (parsing && pos < length) {
            uint8_t objectId = data[pos++];
            
            switch (objectId) {
                case 0x01: // Battery (1 byte, uint8, %)
                    if (pos + 1 <= length) {
                        result.battery = data[pos++];
                        result.hasBattery = true;
                    } else {
                        parsing = false;
                    }
                    break;
                    
                case 0x02: // Temperature (2 bytes, int16 LE, factor 0.01)
                    if (pos + 2 <= length) {
                        result.temperature = readInt16LE(&data[pos]) * 0.01f;
                        result.hasTemperature = true;
                        pos += 2;
                    } else {
                        parsing = false;
                    }
                    break;
                    
                case 0x03: // Humidity (2 bytes, uint16 LE, factor 0.01)
                    if (pos + 2 <= length) {
                        result.humidity = readUint16LE(&data[pos]) * 0.01f;
                        result.hasHumidity = true;
                        pos += 2;
                    } else {
                        parsing = false;
                    }
                    break;
                    
                case 0x0C: // Voltage (2 bytes, uint16 LE, mV)
                    if (pos + 2 <= length) {
                        result.voltage_mv = readUint16LE(&data[pos]);
                        result.hasVoltage = true;
                        pos += 2;
                    } else {
                        parsing = false;
                    }
                    break;
                    
                case 0x45: // Temperature precise (2 bytes, int16 LE, factor 0.1)
                    if (pos + 2 <= length) {
                        result.temperature = readInt16LE(&data[pos]) * 0.1f;
                        result.hasTemperature = true;
                        pos += 2;
                    } else {
                        parsing = false;
                    }
                    break;
                    
                case 0x46: // Humidity precise (2 bytes, uint16 LE, factor 0.1)
                    if (pos + 2 <= length) {
                        result.humidity = readUint16LE(&data[pos]) * 0.1f;
                        result.hasHumidity = true;
                        pos += 2;
                    } else {
                        parsing = false;
                    }
                    break;
                    
                default: {
                    // Unknown object ID - skip based on known sizes
                    size_t skipBytes = getObjectSize(objectId);
                    if (skipBytes == 0 || pos + skipBytes > length) {
                        // Unknown size or would exceed buffer - stop parsing
                        parsing = false;
                    } else {
                        pos += skipBytes;
                    }
                    break;
                }
            }
        }
        
        // Valid if we got at least temperature or humidity
        result.valid = result.hasTemperature || result.hasHumidity;
        
        // Sanity checks
        if (result.hasTemperature && (result.temperature < -50.0f || result.temperature > 100.0f)) {
            result.valid = false;
        }
        if (result.hasHumidity && result.humidity > 100.0f) {
            result.valid = false;
        }
        
        return result;
    }

private:
    /**
     * Get the data size for a given BTHome object ID
     * Returns 0 for unknown objects
     */
    static size_t getObjectSize(uint8_t objectId) {
        // Common BTHome v2 object sizes
        switch (objectId) {
            // 1-byte values
            case 0x00: // Packet ID
            case 0x01: // Battery
            case 0x2E: // Humidity (1 byte)
            case 0x3A: // Button
            case 0x3C: // Dimmer
                return 1;
                
            // 2-byte values
            case 0x02: // Temperature
            case 0x03: // Humidity
            case 0x0C: // Voltage
            case 0x0D: // Power
            case 0x43: // Current
            case 0x45: // Temperature (0.1)
            case 0x46: // Humidity (0.1)
            case 0x4A: // Voltage (0.1)
                return 2;
                
            // 3-byte values
            case 0x04: // Pressure
            case 0x05: // Illuminance
            case 0x06: // Mass (kg)
            case 0x07: // Mass (lb)
            case 0x08: // Dewpoint
            case 0x09: // Count
            case 0x0A: // Energy
            case 0x0B: // Power
            case 0x40: // Distance (mm)
            case 0x41: // Distance (m)
            case 0x42: // Duration
                return 3;
                
            // 4-byte values
            case 0x4D: // Energy (4 byte)
            case 0x4E: // Volume
            case 0x4F: // Water
                return 4;
                
            default:
                return 0; // Unknown
        }
    }
};

} // namespace BLE
