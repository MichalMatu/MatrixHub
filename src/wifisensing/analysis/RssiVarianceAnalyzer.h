/**
 * @file RssiVarianceAnalyzer.h
 * @brief Statistical analysis of WiFi RSSI samples for motion detection
 * 
 * Calculates variance, min/max/avg from ring buffer samples.
 * High variance indicates environment changes (e.g., human movement).
 * 
 * Created: 2 Jan 2026 (extracted from WifiSensingService)
 */

#pragma once

#include "../sampling/RssiSampler.h"

namespace WIFISENSING {

// Use local type from RssiSampler
using RssiSample = WIFISENSING::RssiSample;

// Statistics calculated from samples
struct RssiStats {
    int8_t current;        // Most recent RSSI (raw)
    int8_t filtered;       // Median-filtered RSSI (for debugging)
    int8_t min;            // Minimum in window
    int8_t max;            // Maximum in window
    float avg;             // Average in window
    float variance;        // Variance (indicator of movement)
    uint16_t sampleCount;  // Number of samples analyzed
    uint32_t windowMs;     // Time span of analyzed samples
};

/**
 * @class RssiVarianceAnalyzer
 * @brief Analyzes RSSI samples to detect motion via variance
 * 
 * Calculates statistical measures from RssiSampler buffer.
 * Variance threshold (typically 3.0-5.0) indicates motion detected.
 */
class RssiVarianceAnalyzer {
public:
    /**
     * @brief Calculate statistics from raw ring buffer
     * 
     * Pure function - does NOT handle locking. Caller must ensure thread safety.
     * 
     * @param samples Pointer to ring buffer array
     * @param count Number of valid samples in buffer
     * @param head Current head index (where next sample goes)
     * @param bufferSize Total size of the ring buffer
     * @return RssiStats with calculated metrics
     */
    static RssiStats calculateStats(const RssiSample* samples, uint16_t count, uint16_t head, uint16_t bufferSize);
    
    // State for Spike Clipper
    static void resetState() {
        _lastValidRssi = -127;
        _spikeCount = 0;
    }

private:
    static int8_t _lastValidRssi;
    static uint8_t _spikeCount;
    
    RssiVarianceAnalyzer() = delete;  // Static utility class
};

}  // namespace WIFISENSING
