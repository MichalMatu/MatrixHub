#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#ifdef NATIVE_BUILD
#include "BleTypes.h"
#else
#include "../../BleTypes.h"
#endif

namespace BLE {

class TpParser {
public:
    /**
     * Parsuje dane Manufacturer Data w poszukiwaniu formatu ThermoPro TP357S.
     * 
     * Format (Reverse Engineered):
     * Manufacturer ID: 2 bytes (Little Endian)
     * Payload:
     * Byte 0: ?
     * Byte 1: Temperature (LSB)
     * Byte 2: Temperature (MSB) -> int16_t / 10.0
     * Byte 3: Humidity (%)
     * Byte 4: Battery (%)
     */
    static TpData parse(const uint8_t* payload, size_t length) {
        TpData result = {0, 0, 0, false};
        
        // ThermoPro payload is usually short. 
        // We need at least 5-6 bytes of payload AFTER the manufacturer ID.
        // Payload from NimBLE usually includes the Manufacturer ID (2 bytes) at the start?
        // Let's assume input is valid Manufacturer Data payload.
        
        if (length < 6) return result;

        // Simple validation (heuristic), constant byte checking could be added if they exist.
        
        // Temperature: Little Endian short, div 10
        int16_t tempRaw = (int16_t)(payload[1] | (payload[2] << 8));
        result.temperature = tempRaw / 10.0f;
        
        // Humidity: Byte 3
        result.humidity = (float)payload[3];
        
        // Battery: Byte 4? Sometimes Byte 0? 
        // TP357S specifc:
        // Byte 4 is often battery level in %.
        result.battery = payload[4];
        
        // Sane range checks to filter garbage
        if (result.humidity > 100) return result;
        if (result.temperature < -50 || result.temperature > 100) return result;
        
        result.valid = true;
        return result;
    }
    
    /**
     * Checks if packet looks like ThermoPro based on Manufacturer ID or Name
     * TP357S often doesn't use a specific Manufacturer ID but broadcasts name "TP357S"
     * and data in specific format.
     */
    static bool isThermoPro(const char* name) {
        if (!name) return false;
        return (strncmp(name, "TP357", 5) == 0); 
    }
};

} // namespace BLE
