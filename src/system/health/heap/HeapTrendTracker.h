/**
 * @file HeapTrendTracker.h
 * @brief Track heap history over time (trend calculation removed)
 */

#pragma once

#include <Arduino.h>

namespace SYSTEM {
namespace HEALTH {

struct HeapSample {
    uint32_t timestampMs;
    uint32_t freeHeap;
    uint32_t largestBlock;
    uint8_t fragmentation;
};

constexpr size_t HEAP_HISTORY_SIZE = 12;  // 12 samples = 1 hour at 5min intervals

struct HeapHistory {
    HeapSample samples[HEAP_HISTORY_SIZE];
    uint8_t head;
    uint8_t count;
    uint32_t peakFree;       // Highest free heap seen
    uint32_t lowestFree;     // Lowest free heap seen
};

class HeapTrendTracker {
public:
    /**
     * Initialize heap tracking
     */
    static void begin();
    
    /**
     * Update min/max heap values
     */
    static void updateMinMax();
    
    /**
     * Take a heap sample and add to history
     */
    static void sampleHeap();
    
    /**
     * Get heap history (for diagnostics)
     */
    static const HeapHistory& getHistory();
    
    /**
     * Save last sample to RTC memory
     */
    static void saveLastSampleToRtc(HeapSample& outSample);
    
private:
    static HeapHistory _history;
};

}  // namespace HEALTH
}  // namespace SYSTEM
