/**
 * @file SystemDiagnosticsBuilder.cpp
 * @brief Implementation of system diagnostics builder
 */

#include "SystemDiagnosticsBuilder.h"
#include "../wifi/WifiHealthTracker.h"
#include "../runtime/RuntimeStatsCollector.h"
#include "../heap/HeapMonitor.h"
#include "../../logging/Logging.h"
#include "../../logging/LogRingBuffer.h"
#include "../../../system/rtc/RtcConfig.h"
#include "../../../notifications/telegram/client/TelegramClient.h"
#include "../../../notifications/telegram/runtime/TelegramWorker.h"

#undef LOG_TAG
#define LOG_TAG "SysDiag"

namespace SYSTEM {
namespace HEALTH {

SystemDiagnostics SystemDiagnosticsBuilder::build() {
    SystemDiagnostics diag = {};
    
    const auto& wifiHealth = WifiHealthTracker::getHealth();
    const auto& runtimeStats = RuntimeStatsCollector::getStats();
    const auto& rtcStats = RTC::runtimeStats;
    
    // Heap
    diag.heapFree = HeapMonitor::instance().getFreeHeap();
    diag.heapMin = HeapMonitor::instance().getMinFreeHeap();
    diag.heapLargest = HeapMonitor::instance().getLargestFreeBlock();
    diag.heapFragmentation = HeapMonitor::instance().getFragmentation();
    
    // WiFi
    if (wifiHealth.lastConnectMs > 0) {
        diag.wifiUptimeMs = millis() - wifiHealth.lastConnectMs;
    }
    diag.wifiReconnects = wifiHealth.reconnectCount;
    diag.wifiDisconnectReason = wifiHealth.lastDisconnectReason;
    diag.wifiRssi = wifiHealth.currentRssi;
    diag.wifiHealthy = WifiHealthTracker::isHealthy(RuntimeStatsCollector::getUptimeMs());
    
    // Runtime
    diag.uptimeMs = RuntimeStatsCollector::getUptimeMs();
    diag.loopCount = runtimeStats.loopCount;
    diag.slowLoops = runtimeStats.slowLoopCount;
    diag.maintenanceSleepCount = rtcStats.hygieneSleepCount;
    diag.maintenanceSleepActive = rtcStats.hygieneSleepActive;
    
    return diag;
}

void SystemDiagnosticsBuilder::logStatus() {
    SystemDiagnostics d = build();
    
    const auto& wifiHealth = WifiHealthTracker::getHealth();
    
    // Telegram client and worker now both report their explicit PSRAM-backed
    // scratch/runtime ownership after the Telegram cleanup removed hidden
    // transport globals and worker-global command state.
    // Target behavior is that diagnostics reflect real object ownership instead
    // of silently omitting memory that used to sit in file-static buffers.
    size_t psramLog = LOG::RingBuffer::getPsramMemoryUsage();
    size_t rtcTgParams = TELEGRAM::TelegramClient::getRtcMemoryUsage();
    size_t psramTgClient = TELEGRAM::TelegramClient::getPsramMemoryUsage();
    size_t psramTgWorker = TELEGRAM::TelegramWorker::getPsramMemoryUsage();
    
    size_t memTotal = psramLog + rtcTgParams + psramTgClient + psramTgWorker;

    LOGI("[Health] Up:%um Heap:%u(Min:%u Lrg:%u Frag:%u%%) WiFi:%s(%ddB RC:%u)",
         d.uptimeMs / 60000, d.heapFree, d.heapMin, d.heapLargest, d.heapFragmentation,
         wifiHealth.isConnected ? "Ok" : "No", d.wifiRssi, d.wifiReconnects);

    LOGI("[Health] Loops:%u(Slow:%u) Mem:%ub (LogPS:%u TgRTC:%u TgPS:%u+%u)",
         d.loopCount, d.slowLoops, memTotal, psramLog, rtcTgParams, psramTgClient, psramTgWorker);
}

}  // namespace HEALTH
}  // namespace SYSTEM
