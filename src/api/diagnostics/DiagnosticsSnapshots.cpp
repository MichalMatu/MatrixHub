#include "DiagnosticsSnapshots.h"

#include <esp_heap_caps.h>

namespace API {

const DiagnosticsEndpointEntry kDiagnosticsEndpoints[kDiagnosticsEndpointCount] = {
    {"/api/diagnostics/summary", "GET", "admin", "runtime summary, boot state, heap, HTTP/WS health"},
    {"/api/diagnostics/heap", "GET", "admin", "heap/PSRAM totals, minimums, largest block and fragmentation proxy"},
    {"/api/diagnostics/tasks", "GET", "admin", "FreeRTOS task count, watchdog state and optional task details"},
    {"/api/diagnostics/mutexes", "GET", "admin", "lock instrumentation coverage and known critical locks"},
    {"/api/diagnostics/endpoints", "GET", "admin", "diagnostics endpoint catalog and request metrics coverage"},
    {"/api/diagnostics/features", "GET", "admin", "feature configuration and runtime availability snapshot"},
};

uint8_t calculateFragmentationPercent(size_t freeBytes, size_t largestBlock) {
    if (freeBytes == 0 || largestBlock >= freeBytes) {
        return 0;
    }
    const size_t fragmented = freeBytes - largestBlock;
    const size_t percent = (fragmented * 100U + (freeBytes / 2U)) / freeBytes;
    return static_cast<uint8_t>(percent > 100U ? 100U : percent);
}

DiagnosticsHeapRegionSnapshot buildDiagnosticsHeapRegion(const char* name, uint32_t caps) {
    DiagnosticsHeapRegionSnapshot snapshot{};
    snapshot.name = name;
    snapshot.caps = caps;
    snapshot.total = heap_caps_get_total_size(caps);
    snapshot.free = heap_caps_get_free_size(caps);
    snapshot.minimumFree = heap_caps_get_minimum_free_size(caps);
    snapshot.largestBlock = heap_caps_get_largest_free_block(caps);
    if (snapshot.largestBlock > snapshot.free) {
        snapshot.largestBlock = snapshot.free;
    }
    snapshot.available = snapshot.total > 0 || snapshot.free > 0 || snapshot.largestBlock > 0;
    snapshot.fragmentationPercent =
        calculateFragmentationPercent(snapshot.free, snapshot.largestBlock);
    return snapshot;
}

DiagnosticsHeapSnapshot buildDiagnosticsHeapSnapshot() {
    DiagnosticsHeapSnapshot snapshot{};
    snapshot.defaultHeap = buildDiagnosticsHeapRegion("default_8bit", MALLOC_CAP_8BIT);
    snapshot.internal = buildDiagnosticsHeapRegion(
        "internal_8bit", MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    snapshot.psram = buildDiagnosticsHeapRegion("psram", MALLOC_CAP_SPIRAM);
    return snapshot;
}

bool addDiagnosticsFeature(DiagnosticsFeaturesSnapshot& snapshot, const DiagnosticsFeatureState& feature) {
    if (snapshot.count >= kMaxDiagnosticsFeatures) {
        return false;
    }
    snapshot.features[snapshot.count++] = feature;
    return true;
}

}  // namespace API
