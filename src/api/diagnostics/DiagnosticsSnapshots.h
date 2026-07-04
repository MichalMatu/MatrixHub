#pragma once

#include <cstddef>
#include <cstdint>

#include "../../system/health/network/HttpServerHealthTracker.h"

namespace API {

struct DiagnosticsHeapRegionSnapshot {
    const char* name = nullptr;
    uint32_t caps = 0;
    bool available = false;
    size_t total = 0;
    size_t free = 0;
    size_t minimumFree = 0;
    size_t largestBlock = 0;
    uint8_t fragmentationPercent = 0;
};

struct DiagnosticsHeapSnapshot {
    DiagnosticsHeapRegionSnapshot defaultHeap;
    DiagnosticsHeapRegionSnapshot internal;
    DiagnosticsHeapRegionSnapshot psram;
};

struct DiagnosticsBootSnapshot {
    uint32_t bootCount = 0;
    uint16_t unexpectedRestarts = 0;
    bool lastBootUnexpected = false;
    uint32_t lastSessionUptimeMs = 0;
    uint8_t lastShutdownReason = 0;
    uint8_t lastResetReason = 0;
    uint32_t freeHeapAtShutdown = 0;
    int currentResetReason = 0;
};

struct DiagnosticsWatchdogSnapshot {
    bool initialized = false;
    uint32_t timeoutSec = 0;
};

struct DiagnosticsSummarySnapshot {
    const char* schema = "diagnostics.v1";
    const char* firmwareName = nullptr;
    const char* firmwareVersion = nullptr;
    const char* buildTarget = nullptr;
    uint32_t uptimeSec = 0;
    DiagnosticsBootSnapshot boot;
    DiagnosticsWatchdogSnapshot watchdog;
    DiagnosticsHeapSnapshot heap;
    SYSTEM::HEALTH::HttpServerHealthSnapshot http;
    size_t featureCount = 0;
    bool featureConfigRead = false;
};

struct DiagnosticsFeatureState {
    const char* key = nullptr;
    bool serviceAvailable = false;
    bool configKnown = false;
    bool configuredEnabled = false;
    bool runtimeMeasured = false;
    bool runtimeActive = false;
    const char* detail = nullptr;
};

static constexpr size_t kMaxDiagnosticsFeatures = 16;

struct DiagnosticsFeaturesSnapshot {
    bool configRead = false;
    DiagnosticsFeatureState features[kMaxDiagnosticsFeatures];
    size_t count = 0;
};

struct DiagnosticsEndpointEntry {
    const char* path = nullptr;
    const char* method = nullptr;
    const char* auth = nullptr;
    const char* description = nullptr;
};

static constexpr size_t kDiagnosticsEndpointCount = 6;

extern const DiagnosticsEndpointEntry kDiagnosticsEndpoints[kDiagnosticsEndpointCount];

uint8_t calculateFragmentationPercent(size_t freeBytes, size_t largestBlock);
DiagnosticsHeapRegionSnapshot buildDiagnosticsHeapRegion(const char* name, uint32_t caps);
DiagnosticsHeapSnapshot buildDiagnosticsHeapSnapshot();
bool addDiagnosticsFeature(DiagnosticsFeaturesSnapshot& snapshot, const DiagnosticsFeatureState& feature);

}  // namespace API
