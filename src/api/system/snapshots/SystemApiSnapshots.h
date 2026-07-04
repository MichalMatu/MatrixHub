#pragma once

#include <PsychicHttpServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../../../system/health/network/HttpServerHealthTracker.h"
#include "../SystemApiFacets.h"

struct WiFiConnectivityDiagnostics;

namespace API {

struct SystemInfoSnapshot {
    const char* firmwareVersion = nullptr;
    const char* firmwareName = nullptr;
    const char* buildTarget = nullptr;
    const char* chipModel = nullptr;
    const char* sdkVersion = nullptr;
    const char* arduinoVersion = nullptr;
    uint32_t cpuFreqMhz = 0;
    int cpuRevision = 0;
    int cpuCores = 0;
    uint32_t flashSize = 0;
    uint32_t flashChipSpeed = 0;
    uint32_t sketchSize = 0;
    uint32_t freeSketchSpace = 0;
    float coreTemp = 0.0f;
    uint32_t internalFree = 0;
    uint32_t internalTotal = 0;
    uint32_t internalLargest = 0;
    uint32_t internalMinFree = 0;
    uint32_t psramSize = 0;
    uint32_t psramFree = 0;
    size_t fsTotalBytes = 0;
    size_t fsUsedBytes = 0;
    uint32_t uptimeSec = 0;
    char macAddress[20]{0};
    // Keep the raw ESP reset enum numeric end-to-end. Older code stringified
    // this value and the frontend immediately parsed it back to an integer.
    int resetReason = 0;
};

struct SystemMemoryRegionSnapshot {
    bool available = false;
    uint32_t total = 0;
    uint32_t free = 0;
    uint32_t used = 0;
    uint32_t minimumFree = 0;
    uint32_t largestBlock = 0;
    uint8_t fragmentationPercent = 0;
};

struct SystemTasksSnapshot {
    TaskStatus_t* tasks = nullptr;
    bool watchdogInitialized = false;
    uint32_t watchdogTimeoutSec = 0;
    uint32_t taskCount = 0;
    uint32_t totalRunTime = 0;
    uint32_t freeHeap = 0;
    uint32_t minFreeHeap = 0;
    uint32_t freePsram = 0;
    SystemMemoryRegionSnapshot defaultHeap;
    SystemMemoryRegionSnapshot internalHeap;
    SystemMemoryRegionSnapshot psram;
    UBaseType_t actualCount = 0;
    bool allocationFailed = false;
    bool detailsIncluded = false;

    SystemTasksSnapshot() = default;
    ~SystemTasksSnapshot();

    SystemTasksSnapshot(const SystemTasksSnapshot&) = delete;
    SystemTasksSnapshot& operator=(const SystemTasksSnapshot&) = delete;
    SystemTasksSnapshot(SystemTasksSnapshot&& other) noexcept;
    SystemTasksSnapshot& operator=(SystemTasksSnapshot&& other) noexcept;
};

struct SystemNetworkSnapshot {
    bool available = false;
    WiFiConnectivityDiagnostics* wifi = nullptr;
    SYSTEM::HEALTH::HttpServerHealthSnapshot http{};

    SystemNetworkSnapshot();
    ~SystemNetworkSnapshot();

    SystemNetworkSnapshot(const SystemNetworkSnapshot&) = delete;
    SystemNetworkSnapshot& operator=(const SystemNetworkSnapshot&) = delete;
    SystemNetworkSnapshot(SystemNetworkSnapshot&& other) noexcept;
    SystemNetworkSnapshot& operator=(SystemNetworkSnapshot&& other) noexcept;
};

SystemInfoSnapshot buildSystemInfoSnapshot(const SystemApiInfoDeps& deps);
SystemTasksSnapshot buildSystemTasksSnapshot(const SystemApiTaskDeps& deps, bool includeDetails);
SystemNetworkSnapshot buildSystemNetworkSnapshot(const SystemApiNetworkDeps& deps);

}  // namespace API
