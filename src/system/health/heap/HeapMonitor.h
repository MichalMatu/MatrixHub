/**
 * @file HeapMonitor.h
 * @brief Heap memory monitoring for long-running stability
 * 
 * Provides periodic heap monitoring, fragmentation detection, and 
 * low memory warnings to aid in debugging memory issues during
 * long-term operation.
 */

#pragma once

#include <Arduino.h>

namespace POWER {
class PowerManager;
}

namespace SYSTEM {

class HeapMonitor {
public:
    static HeapMonitor& instance() {
        // Intentional singleton: heap health is a whole-process concern and the
        // monitor acts as one global observer over shared memory pools. DI
        // would add plumbing, but would not create meaningful independent
        // instances or ownership boundaries.
        //
        // Leave this alone unless heap accounting is ever split into separate
        // allocators with independent policy/telemetry. In the current design
        // one monitor per device/process is the clearest representation.
        static HeapMonitor _instance;
        return _instance;
    }

    /**
     * Initialize heap monitor
     * Call once during application setup
     */
    void begin();

    /**
     * Inject PowerManager for optional hygiene sleep requests.
     */
    void setPowerManager(POWER::PowerManager* powerManager) { _powerManager = powerManager; }
    
    /**
     * Check heap status and log warnings if needed
     * Call periodically (e.g., every 30 seconds)
     * 
     * @return true if heap is healthy, false if critically low
     */
    bool check();

    /**
     * Lightweight poll hook (safe to call frequently).
     * Processes any deferred probes scheduled via armDeferredProbe().
     */
    void poll();

    /**
     * Schedule a one-shot heap probe for (now + delayMs).
     * Intended for debugging short-lived heap dips caused by networking buffers.
     *
     * @param tag A short literal tag (e.g., "charts", "download").
     * @param delayMs Delay before measuring (typical: 1000-2000ms).
     * @param freeBefore Baseline free heap (captured earlier).
     * @param largestBefore Baseline largest block (captured earlier).
     */
    void armDeferredProbe(const char* tag, uint32_t delayMs, uint32_t freeBefore, uint32_t largestBefore);
    
    /**
     * Get current heap statistics
     */
    uint32_t getFreeHeap() const;
    uint32_t getMinFreeHeap() const;
    uint32_t getLargestFreeBlock() const;
    uint8_t getFragmentation() const;
    
    /**
     * Log detailed heap status
     */
    void logStatus();
    
    /**
     * Check if heap is critically low
     */
    bool isCriticallyLow() const;
    
    /**
     * Check if hygiene sleep should be triggered for heap defragmentation.
     * Call periodically (e.g., every 30 seconds).
     * Will trigger deep sleep if conditions are met (largest block < 30KB for TLS
     * or fragmentation > 50% for > 5 minutes).
     * 
     * @return true if hygiene sleep was triggered
     */
    bool checkHygieneConditions();
    
    // Configuration
    static constexpr uint32_t kCriticalHeapBytes = 15000;   // 15KB - panic level
    static constexpr uint32_t kWarningHeapBytes = 25000;    // 25KB - warning level
    static constexpr uint32_t kMinLargestBlock = 8192;      // 8KB - min for TLS
    static constexpr uint8_t kMaxFragmentationPct = 70;      // 70% - generous with PSRAM (baseline ~50%)
#if defined(BOARD_HAS_PSRAM)
    static constexpr uint32_t kHygieneThresholdBytes = 10240; // 10KB sufficient for Internal RAM when PSRAM handles large blocks
#else
    static constexpr uint32_t kHygieneThresholdBytes = 30720; // 30KB needed for SSL buffers on non-PSRAM devices
#endif
    static constexpr uint32_t kHighFragDurationMs = 300000;   // 5 minutes sustained high frag
    static constexpr uint32_t kMinHygieneIntervalMs = 600000; // 10 minutes min between sleeps
    
protected:
    HeapMonitor() = default;
    HeapMonitor(const HeapMonitor&) = delete;
    HeapMonitor& operator=(const HeapMonitor&) = delete;

    uint32_t _bootFreeHeap = 0;
    uint32_t _lastCheckMs = 0;
    uint32_t _lowHeapWarnings = 0;
    uint32_t _highFragStartMs = 0;  // When high fragmentation started
    bool _initialized = false;
    POWER::PowerManager* _powerManager = nullptr;

    // Deferred probe state
    struct DeferredProbe {
        const char* tag = nullptr;
        uint32_t armTime = 0;
        uint32_t triggerTime = 0;
        uint32_t freeBefore = 0;
        uint32_t largestBefore = 0;
        bool active = false;
    } _deferredProbe;
};

}  // namespace SYSTEM
