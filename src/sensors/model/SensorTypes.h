#pragma once

#include <Arduino.h>

// Sensor snapshot shared between task and API
struct SensorSnapshot {
    uint16_t co2 = 0;           // CO2 concentration in ppm
    float temp = 0;             // Temperature in °C
    float humid = 0;            // Relative humidity in %RH
    uint32_t timestamp_ms = 0;
    uint32_t seq = 0;
};

// Phase status for detailed logging
// Uses const char* instead of String to avoid heap fragmentation
struct PhaseStatus {
    bool ok = false;
    uint32_t start_ms = 0;
    uint32_t duration_ms = 0;
    const char* error_code = nullptr;
    const char* error_detail = nullptr;
};

struct ErrorInfo {
    const char* code = nullptr;
    uint32_t timestamp_ms = 0;
};

// Commands for task queue
enum SensorTaskCommand : uint8_t {
    CMD_NONE = 0,
    CMD_FORCE_LOG = 1,
    CMD_FORCE_READ_AND_LOG = 2
};

