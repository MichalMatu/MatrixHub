#pragma once

#include <Arduino.h>
#include <cmath>

namespace DATALOG {

// Binary format constants
constexpr uint32_t BINARY_MAGIC = 0x504C4E54;  // "PLNT" in ASCII (little-endian)
constexpr uint8_t BINARY_VERSION = 3;
constexpr uint8_t BINARY_RECORD_SIZE = 10;

// File header (8 bytes total)
struct __attribute__((packed)) BinaryFileHeader {
    uint32_t magic;        // 0x504C4E54 ("PLNT")
    uint8_t version;       // Format version (1)
    uint8_t recordSize;    // Size of each record (18)
    uint16_t reserved;     // Reserved for future use (0)
};

// Single log record (10 bytes total - SCD4x without battery)
// All numeric fields are in little-endian (ESP32 native)
struct __attribute__((packed)) BinaryLogRecord {
    uint32_t timestamp;    // Unix timestamp (seconds since epoch) - 4 bytes
    uint16_t co2;          // CO2 concentration in ppm (0 to 65535) - 2 bytes
    int16_t temp_10x;      // Temperature × 10 (-200 to +600 = -20.0°C to +60.0°C) - 2 bytes
    uint16_t humid_10x;    // Humidity × 10 (0 to 1000 = 0.0% to 100.0%) - 2 bytes
    // Total: 10 bytes
};

// Compile-time size validation
static_assert(sizeof(BinaryFileHeader) == 8, "BinaryFileHeader must be 8 bytes");
static_assert(sizeof(BinaryLogRecord) == 10, "BinaryLogRecord must be 10 bytes");

// Helper functions for float → int16_t conversion
inline int16_t floatToInt16_10x(float value) {
    if (std::isnan(value)) {
        return INT16_MIN;  // Use minimum value as NaN marker
    }
    return static_cast<int16_t>(std::round(value * 10.0f));
}

inline uint16_t floatToUInt16_10x(float value) {
    if (std::isnan(value) || value < 0) {
        return 0;
    }
    return static_cast<uint16_t>(std::round(value * 10.0f));
}

inline uint16_t floatToMillivolts(float volts) {
    if (std::isnan(volts) || volts < 0) {
        return 0;
    }
    return static_cast<uint16_t>(std::round(volts * 1000.0f));
}

// Helper functions for int16_t → float conversion (for reading)
inline float int16ToFloat_10x(int16_t value) {
    if (value == INT16_MIN) {
        return NAN;
    }
    return static_cast<float>(value) / 10.0f;
}

inline float uint16ToFloat_10x(uint16_t value) {
    return static_cast<float>(value) / 10.0f;
}

inline float millivoltsToFloat(uint16_t millivolts) {
    return static_cast<float>(millivolts) / 1000.0f;
}

}  // namespace DATALOG
