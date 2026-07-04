/**
 * @file HeapMonitor.cpp
 * @brief Implementation of heap memory monitoring
 */

#include "HeapMonitor.h"
#include "../../logging/Logging.h"
#include "../../boot/BootTracker.h"
#include "../../rtc/RtcConfig.h"
#include "../../power/PowerManager.h"
#include <esp_heap_caps.h>
#include <esp_psram.h>

#undef LOG_TAG
#define LOG_TAG "HeapMon"

namespace SYSTEM {

void HeapMonitor::begin() {
    if (_initialized) {
        return;
    }
    
    _bootFreeHeap = getFreeHeap();
    _lastCheckMs = millis();
    _lowHeapWarnings = 0;
    _initialized = true;
    
    LOGD("Heap monitor initialized - boot free: %u bytes, largest block: %u bytes",
         _bootFreeHeap, getLargestFreeBlock());
}

bool HeapMonitor::check() {
    if (!_initialized) {
        begin();
    }
    
    uint32_t now = millis();
    uint32_t freeHeap = getFreeHeap();
    uint32_t minFree = getMinFreeHeap();
    uint32_t largestBlock = getLargestFreeBlock();
    uint8_t fragmentation = getFragmentation();
    
    _lastCheckMs = now;
    
    // Check for critical conditions
    bool healthy = true;
    
    if (freeHeap < kCriticalHeapBytes) {
        LOGE("CRITICAL: Free heap %u < %u bytes!", freeHeap, kCriticalHeapBytes);
        _lowHeapWarnings++;
        healthy = false;
    } else if (freeHeap < kWarningHeapBytes) {
        LOGW("Low heap: %u bytes (min ever: %u)", freeHeap, minFree);
        _lowHeapWarnings++;
    }
    
    if (largestBlock < kMinLargestBlock) {
        LOGW("Fragmented: largest block %u < %u bytes (TLS may fail)", 
             largestBlock, kMinLargestBlock);
        _lowHeapWarnings++;
        healthy = false;
    }
    
    if (fragmentation > kMaxFragmentationPct) {
        LOGW("High fragmentation: %u%% > %u%%", fragmentation, kMaxFragmentationPct);
        healthy = false;
    }

    
    // Check for PSRAM availability/health
    if (esp_psram_get_size() > 0) {
        size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        if (freePsram < 50000) {
            LOGW("Low PSRAM warning: %zu bytes free", freePsram);
        }
    }

    return healthy;
}

void HeapMonitor::poll() {
    if (!_deferredProbe.active) return;

    if (millis() >= _deferredProbe.triggerTime) {
        // Time to measure
        uint32_t freeNow = getFreeHeap();
        uint32_t largestNow = getLargestFreeBlock();
        
        int32_t diffFree = (int32_t)freeNow - (int32_t)_deferredProbe.freeBefore;
        int32_t diffLargest = (int32_t)largestNow - (int32_t)_deferredProbe.largestBefore;
        
        LOGD("[%s] Heap Check: Free %u->%u (%+d), Largest %u->%u (%+d)", 
             _deferredProbe.tag, 
             _deferredProbe.freeBefore, freeNow, diffFree,
             _deferredProbe.largestBefore, largestNow, diffLargest);
             
        // Disarm
        _deferredProbe.active = false;
    }
}

void HeapMonitor::armDeferredProbe(const char* tag, uint32_t delayMs, uint32_t freeBefore, uint32_t largestBefore) {
    if (_deferredProbe.active) return; // Busy
    
    _deferredProbe.tag = tag;
    _deferredProbe.armTime = millis();
    _deferredProbe.triggerTime = _deferredProbe.armTime + delayMs;
    _deferredProbe.freeBefore = freeBefore;
    _deferredProbe.largestBefore = largestBefore;
    _deferredProbe.active = true;
}

uint32_t HeapMonitor::getFreeHeap() const {
    // Return INTERNAL heap size to match what we monitor for fragmentation
    return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

uint32_t HeapMonitor::getMinFreeHeap() const {
    // Use INTERNAL scope to match getFreeHeap() and getLargestFreeBlock()
    return heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

uint32_t HeapMonitor::getLargestFreeBlock() const {
    return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

uint8_t HeapMonitor::getFragmentation() const {
    // [FIX] Use consistent caps for both metrics to ensure valid percentage
    // Default esp_get_free_heap_size() includes DRAM + PSRAM (if integrated)
    // trying to be specific to DRAM (Internal) for critical fragmentation
    
    uint32_t free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    uint32_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    
    if (free == 0) return 0;
    
    // Formula: (TotalFree - LargestBlock) / TotalFree * 100
    // If TotalFree == LargestBlock, fragmentation is 0%
    // If TotalFree >> LargestBlock, fragmentation approaches 100%
    
    float frag = 100.0f - ((float)largest * 100.0f) / (float)free;
    return (uint8_t)frag;
}

void HeapMonitor::logStatus() {
    uint32_t freeInternal = getFreeHeap(); // Now returns INTERNAL
    uint32_t totalFree = esp_get_free_heap_size(); // Classic total
    uint32_t largest = getLargestFreeBlock();
    uint32_t minFree = getMinFreeHeap();
    uint8_t frag = getFragmentation();
    
    LOGD_THROTTLED(TASK_MONITOR::STACK_LOG_INTERVAL_MS,
                   "Heap: IntFree=%u, TotFree=%u, LrgBlock=%u, Frag=%u%%",
                   freeInternal,
                   totalFree,
                   largest,
                   frag);
         
    if (esp_psram_get_size() > 0) {
        size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t totalPsram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
        size_t largestPsram = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
        LOGD_THROTTLED(TASK_MONITOR::STACK_LOG_INTERVAL_MS,
                       "PSRAM: Free=%zu/%zu, Largest=%zu",
                       freePsram,
                       totalPsram,
                       largestPsram);
    }
}

bool HeapMonitor::isCriticallyLow() const {
    return getFreeHeap() < kCriticalHeapBytes;
}

bool HeapMonitor::checkHygieneConditions() {
    uint32_t now = millis();
    
    // Safety check: Prevent boot loops by enforcing a minimum uptime before hygiene sleep
    if (now < kMinHygieneIntervalMs) {
        return false;
    }

    uint32_t largestBlock = getLargestFreeBlock();
    uint8_t frag = getFragmentation();
    
    // Condition 1: Unable to allocate standard TLS buffer
    bool criticalFragmentation = largestBlock < kHygieneThresholdBytes;
    
    // Condition 2: High fragmentation sustained over time
    bool highFragmentation = frag > kMaxFragmentationPct;
    
    if (highFragmentation) {
        if (_highFragStartMs == 0) {
            _highFragStartMs = now;
        }
    } else {
        _highFragStartMs = 0;
    }
    
    bool sustainedHighFrag = (_highFragStartMs > 0) && ((now - _highFragStartMs) > kHighFragDurationMs);

    if (criticalFragmentation || sustainedHighFrag) {
        LOGW("Hygiene sleep trigger: block=%uKB frag=%u%% sustained=%s",
             largestBlock / 1024, frag,
             sustainedHighFrag ? "YES" : "NO");
        
        // Very short sleep (100ms) - just to reset DRAM
        if (_powerManager) {
            _powerManager->setWakeInterval(100);
            RTC::markMaintenanceSleepPending(now);
            _powerManager->requestSleep("hygiene", 0);
        } else {
            LOGW("Hygiene sleep trigger skipped: PowerManager not wired");
        }
        
        return true;  // Triggered
    }
    
    return false;
}

}  // namespace SYSTEM
