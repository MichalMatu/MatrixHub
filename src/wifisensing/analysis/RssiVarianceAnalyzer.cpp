/**
 * @file RssiVarianceAnalyzer.cpp
 * @brief Implementation of RSSI variance calculation for motion detection
 */

#include "RssiVarianceAnalyzer.h"

namespace WIFISENSING {



int8_t RssiVarianceAnalyzer::_lastValidRssi = -127;
uint8_t RssiVarianceAnalyzer::_spikeCount = 0;

RssiStats RssiVarianceAnalyzer::calculateStats(const RssiSample* samples, uint16_t count, uint16_t head, uint16_t bufferSize) {
    RssiStats stats = {};
    
    if (count == 0 || samples == nullptr) {
        return stats;
    }
    
    // CONFIGURATION
    constexpr uint16_t ANALYSIS_WINDOW_SIZE = 30; // Analyze last ~0.6 seconds (at 50Hz) or ~15 sec (at 2Hz) - reduced for stability
    constexpr uint8_t MEDIAN_WINDOW = 7;           // Filter window size (must be odd: 5, 7, 9...)
    
    // Determine how many samples to analyze
    uint16_t samplesToAnalyze = (count < ANALYSIS_WINDOW_SIZE) ? count : ANALYSIS_WINDOW_SIZE;
    
    // Not enough samples for filtering? Return basic current stats
    if (samplesToAnalyze < MEDIAN_WINDOW) {
        uint16_t currentIdx = (head > 0) ? (head - 1) : (bufferSize - 1);
        stats.current = samples[currentIdx].rssi;
        stats.filtered = stats.current;
        return stats;
    }

    // Temporary values
    int32_t sum = 0;
    int32_t sumSq = 0;
    int8_t minVal = 127;
    int8_t maxVal = -128;
    
    uint16_t filteredCount = 0;
    
    // Helper lambda to safe-read from circular buffer relative to Head.
    // relIndex 0 = Newest sample (at head-1).
    auto getRawEntry = [&](uint16_t relIndex) -> const RssiSample& {
         int32_t idx = (int32_t)head - 1 - relIndex;
         while (idx < 0) idx += bufferSize;
         return samples[idx];
    };

    auto getRawSample = [&](uint16_t relIndex) -> int8_t {
         return getRawEntry(relIndex).rssi;
    };
    
    // Filter Helper: Calculate median of 5 samples centered at relIndex
    auto getMedian = [&](uint16_t relIndex) -> int8_t {
        int8_t w[MEDIAN_WINDOW];
        // Collect samples centered at relIndex
        // i.e., relIndex-2, relIndex-1, relIndex, relIndex+1, relIndex+2
        for(int k=0; k<MEDIAN_WINDOW; k++) {
            // We clamp negative offset to 0 (Newest) to avoid looking into future
            // We ignore boundary check for Oldest because loop limits prevent it
            int32_t offset = (int32_t)relIndex - (MEDIAN_WINDOW/2) + k;
            if (offset < 0) offset = 0;
            w[k] = getRawSample(offset);
        }
        
        // Sorting Network for 7 elements (16 comparators, fastest)
        // Replaces slow O(N^2) Bubble Sort
        #define SORT_SWAP(a,b) if(w[a]>w[b]){int8_t t=w[a];w[a]=w[b];w[b]=t;}
        SORT_SWAP(0, 1); SORT_SWAP(2, 3); SORT_SWAP(4, 5);
        SORT_SWAP(0, 2); SORT_SWAP(1, 3); SORT_SWAP(4, 6);
        SORT_SWAP(1, 2); SORT_SWAP(5, 6);
        SORT_SWAP(0, 4); SORT_SWAP(1, 5); SORT_SWAP(2, 6);
        SORT_SWAP(1, 4); SORT_SWAP(3, 6);
        SORT_SWAP(2, 4); SORT_SWAP(3, 5);
        SORT_SWAP(3, 4);
        #undef SORT_SWAP
        return w[MEDIAN_WINDOW/2];
    };

    // 1. Calculate Statistics
    int8_t raw = getRawSample(0);

    // --- SPIKE CLIPPER (User Request: Cut > 15dBm jumps) ---
    // Init state if first run
    if (_lastValidRssi == -127) {
        _lastValidRssi = raw;
    }

    int16_t diff = (int16_t)raw - _lastValidRssi;
    // Abs diff
    if (diff < 0) diff = -diff;

    if (diff > 15) {
        // SPIKE DETECTED
        _spikeCount++;
        if (_spikeCount > 5) {
             // Sustained change (> 0.5s) -> Accept it
             _lastValidRssi = raw;
             _spikeCount = 0;
        } else {
             // Transient spike -> Reject it (Repeat last valid)
             raw = _lastValidRssi;
        }
    } else {
        // Normal change -> Accept
        _lastValidRssi = raw;
        _spikeCount = 0;
    }
    // --------------------------------------------------------

    stats.current = raw;
    stats.filtered = getMedian(0);
    
    // Iterate through analysis window
    // Start from MEDIAN_WINDOW/2 to avoid boundary at the beginning
    // End before the end of available samples
    uint16_t limit = samplesToAnalyze - (MEDIAN_WINDOW/2);
    
    for (uint16_t i = MEDIAN_WINDOW/2; i < limit; i++) {
        int8_t val = getMedian(i);
        filteredCount++;
        
        sum += val;
        sumSq += (int32_t)val * val;
        if (val < minVal) minVal = val;
        if (val > maxVal) maxVal = val;
    }
    
    // If we have data, calculate final stats
    if (filteredCount > 0) {
        stats.avg = (float)sum / filteredCount;
        stats.min = minVal;
        stats.max = maxVal;
        stats.sampleCount = filteredCount;
        const uint16_t newestAnalyzedRel = MEDIAN_WINDOW / 2;
        const uint16_t oldestAnalyzedRel = limit - 1;
        stats.windowMs =
            getRawEntry(newestAnalyzedRel).timestampMs -
            getRawEntry(oldestAnalyzedRel).timestampMs;
        
        // Fast 1-Pass Integer Variance Calculation to avoid FPU loops
        // Var(X) = (N * Sum(X^2) - Sum(X)^2) / N^2
        int32_t varianceScaled = (filteredCount * sumSq) - (sum * sum);
        stats.variance = (float)varianceScaled / ((int32_t)filteredCount * filteredCount);
    }
    
    return stats;
}

}  // namespace WIFISENSING
