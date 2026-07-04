/**
 * @file HeapTrendTracker.cpp
 * @brief Implementation of heap history tracking
 * 
 * Uses RTC memory (RTC::heapHistory) to survive deep sleep.
 */

#include "HeapTrendTracker.h"
#include "HeapMonitor.h"
#include "../../logging/Logging.h"
#include "../../../system/rtc/RtcConfig.h"
#include <cstring>

#undef LOG_TAG
#define LOG_TAG "HeapHistory"

namespace SYSTEM {
namespace HEALTH {

// Use RTC memory directly via reference
static RTC::RtcHeapHistory& rtcHistory = RTC::heapHistory;

// Local cache for fast access (synced with RTC)
HeapHistory HeapTrendTracker::_history = {};

void HeapTrendTracker::begin() {
    // Try to restore from RTC (if valid data exists from before deep sleep)
    if (rtcHistory.count > 0 && rtcHistory.peakFree > 0) {
        // Restore from RTC
        _history.head = rtcHistory.head;
        _history.count = rtcHistory.count;
        _history.peakFree = rtcHistory.peakFree;
        _history.lowestFree = rtcHistory.lowestFree;
        
        // Copy samples
        for (size_t i = 0; i < HEAP_HISTORY_SIZE && i < RTC::kHeapHistorySize; i++) {
            _history.samples[i].timestampMs = rtcHistory.samples[i].timestampMs;
            _history.samples[i].freeHeap = rtcHistory.samples[i].freeHeap;
            _history.samples[i].largestBlock = rtcHistory.samples[i].largestBlock;
            _history.samples[i].fragmentation = rtcHistory.samples[i].fragmentation;
        }
        
        LOGI("Restored heap history from RTC (%u samples)", _history.count);
    } else {
        // Fresh start
        memset(&_history, 0, sizeof(_history));
        memset(&rtcHistory, 0, sizeof(rtcHistory));
        _history.peakFree = HeapMonitor::instance().getFreeHeap();
        _history.lowestFree = _history.peakFree;
        
        LOGD("Heap history tracking initialized (fresh)");
    }
    
    // Take initial sample
    sampleHeap();
}

void HeapTrendTracker::updateMinMax() {
    uint32_t currentHeap = HeapMonitor::instance().getFreeHeap();
    
    if (currentHeap > _history.peakFree) {
        _history.peakFree = currentHeap;
        rtcHistory.peakFree = currentHeap;
    }
    if (currentHeap < _history.lowestFree) {
        _history.lowestFree = currentHeap;
        rtcHistory.lowestFree = currentHeap;
    }
}

void HeapTrendTracker::sampleHeap() {
    HeapSample sample;
    sample.timestampMs = millis();
    sample.freeHeap = HeapMonitor::instance().getFreeHeap();
    sample.largestBlock = HeapMonitor::instance().getLargestFreeBlock();
    sample.fragmentation = HeapMonitor::instance().getFragmentation();
    
    // Add to local ring buffer
    _history.samples[_history.head] = sample;
    _history.head = (_history.head + 1) % HEAP_HISTORY_SIZE;
    if (_history.count < HEAP_HISTORY_SIZE) {
        _history.count++;
    }
    
    // Sync to RTC (survives deep sleep)
    uint8_t rtcHead = rtcHistory.head;
    rtcHistory.samples[rtcHead].timestampMs = sample.timestampMs;
    rtcHistory.samples[rtcHead].freeHeap = sample.freeHeap;
    rtcHistory.samples[rtcHead].largestBlock = sample.largestBlock;
    rtcHistory.samples[rtcHead].fragmentation = sample.fragmentation;
    rtcHistory.head = (rtcHead + 1) % RTC::kHeapHistorySize;
    if (rtcHistory.count < RTC::kHeapHistorySize) {
        rtcHistory.count++;
    }
    
    LOGD_THROTTLED(TASK_MONITOR::STACK_LOG_INTERVAL_MS,
                   "Heap sample: free=%u, largest=%u, frag=%u%%",
                   sample.freeHeap,
                   sample.largestBlock,
                   sample.fragmentation);
}

const HeapHistory& HeapTrendTracker::getHistory() {
    return _history;
}

void HeapTrendTracker::saveLastSampleToRtc(HeapSample& outSample) {
    if (_history.count > 0) {
        uint8_t lastIdx = (_history.head + HEAP_HISTORY_SIZE - 1) % HEAP_HISTORY_SIZE;
        outSample = _history.samples[lastIdx];
    }
}

}  // namespace HEALTH
}  // namespace SYSTEM
