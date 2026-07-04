#include "SystemSnapshotJsonWriter.h"

#include <wifi/WiFiSettingsService.h>

#include "../../../config/json/ConfigKeys.h"

namespace API {

namespace {

const char* wifiModeName(wifi_mode_t mode) {
    switch (mode) {
        case WIFI_OFF:
            return "off";
        case WIFI_STA:
            return "sta";
        case WIFI_AP:
            return "ap";
        case WIFI_AP_STA:
            return "ap_sta";
        default:
            return "unknown";
    }
}

bool isSetIp(const IPAddress& ip) {
    return static_cast<uint32_t>(ip) != 0;
}

void writeIpIfSet(
    Utils::JsonResponseWriter& writer,
    const char* key,
    const IPAddress& ip) {
    if (!isSetIp(ip)) {
        return;
    }
    writer.key(key);
    writer.string(ip.toString().c_str());
}

void writeMemoryRegion(
    Utils::JsonResponseWriter& writer,
    const SystemMemoryRegionSnapshot& snapshot) {
    writer.raw("{");
    writer.key("available"); writer.value(snapshot.available); writer.raw(",");
    writer.key("total"); writer.value(static_cast<unsigned long>(snapshot.total)); writer.raw(",");
    writer.key("free"); writer.value(static_cast<unsigned long>(snapshot.free)); writer.raw(",");
    writer.key("used"); writer.value(static_cast<unsigned long>(snapshot.used)); writer.raw(",");
    writer.key("minimumFree"); writer.value(static_cast<unsigned long>(snapshot.minimumFree)); writer.raw(",");
    writer.key("largestBlock"); writer.value(static_cast<unsigned long>(snapshot.largestBlock)); writer.raw(",");
    writer.key("fragmentationPercent"); writer.value(static_cast<unsigned long>(snapshot.fragmentationPercent));
    writer.raw("}");
}

const char* taskStateName(eTaskState state) {
    switch (state) {
        case eRunning:
            return "running";
        case eReady:
            return "ready";
        case eBlocked:
            return "blocked";
        case eSuspended:
            return "suspended";
        case eDeleted:
            return "deleted";
        default:
            return "unknown";
    }
}

}  // namespace

void SystemSnapshotJsonWriter::writeSystemInfo(
    Utils::JsonResponseWriter& writer,
    const SystemInfoSnapshot& snapshot) {
    writer.raw("{");
    writer.key(CONFIG::Keys::kEspPlatform); writer.string(snapshot.chipModel); writer.raw(",");
    writer.key(CONFIG::Keys::kFirmwareVersion); writer.string(snapshot.firmwareVersion); writer.raw(",");
    writer.key(CONFIG::Keys::kFirmwareName); writer.string(snapshot.firmwareName); writer.raw(",");
    writer.key(CONFIG::Keys::kFirmwareBuiltTarget); writer.string(snapshot.buildTarget); writer.raw(",");
    writer.key(CONFIG::Keys::kCpuFreqMhz); writer.value(snapshot.cpuFreqMhz); writer.raw(",");
    writer.key(CONFIG::Keys::kCpuType); writer.string(snapshot.chipModel); writer.raw(",");
    writer.key(CONFIG::Keys::kCpuRev); writer.value(snapshot.cpuRevision); writer.raw(",");
    writer.key(CONFIG::Keys::kCpuCores); writer.value(snapshot.cpuCores); writer.raw(",");
    writer.key(CONFIG::Keys::kFlashChipSize); writer.value(static_cast<unsigned long>(snapshot.flashSize)); writer.raw(",");
    writer.key(CONFIG::Keys::kFlashChipSpeed); writer.value(snapshot.flashChipSpeed); writer.raw(",");
    writer.key(CONFIG::Keys::kSketchSize); writer.value(snapshot.sketchSize); writer.raw(",");
    writer.key(CONFIG::Keys::kFreeSketchSpace); writer.value(snapshot.freeSketchSpace); writer.raw(",");
    writer.key(CONFIG::Keys::kMacAddress); writer.string(snapshot.macAddress); writer.raw(",");
    writer.key(CONFIG::Keys::kSdkVersion); writer.string(snapshot.sdkVersion); writer.raw(",");
    writer.key(CONFIG::Keys::kArduinoVersion); writer.string(snapshot.arduinoVersion); writer.raw(",");
    writer.key(CONFIG::Keys::kCpuResetReason); writer.value(snapshot.resetReason); writer.raw(",");
    writer.key(CONFIG::Keys::kCoreTemp); writer.value(snapshot.coreTemp); writer.raw(",");
    writer.key(CONFIG::Keys::kFreeHeap); writer.value(snapshot.internalFree); writer.raw(",");
    writer.key(CONFIG::Keys::kTotalHeap); writer.value(snapshot.internalTotal); writer.raw(",");
    writer.key(CONFIG::Keys::kUsedHeap); writer.value(snapshot.internalTotal - snapshot.internalFree); writer.raw(",");
    writer.key(CONFIG::Keys::kMinFreeHeap); writer.value(snapshot.internalMinFree); writer.raw(",");
    writer.key(CONFIG::Keys::kMaxAllocHeap); writer.value(snapshot.internalLargest); writer.raw(",");
    writer.key(CONFIG::Keys::kPsramSize); writer.value(snapshot.psramSize); writer.raw(",");
    writer.key(CONFIG::Keys::kFreePsram); writer.value(snapshot.psramFree); writer.raw(",");
    writer.key(CONFIG::Keys::kUsedPsram); writer.value(snapshot.psramSize - snapshot.psramFree); writer.raw(",");
    writer.key(CONFIG::Keys::kFsTotal); writer.value(static_cast<unsigned long>(snapshot.fsTotalBytes)); writer.raw(",");
    writer.key(CONFIG::Keys::kFsUsed); writer.value(static_cast<unsigned long>(snapshot.fsUsedBytes)); writer.raw(",");
    writer.key(CONFIG::Keys::kUptime); writer.value(static_cast<unsigned long>(snapshot.uptimeSec)); writer.raw(",");
    writer.key(CONFIG::Keys::kCompileDate); writer.string(__DATE__); writer.raw(",");
    writer.key(CONFIG::Keys::kCompileTime); writer.string(__TIME__);
    writer.raw("}");
}

void SystemSnapshotJsonWriter::writeTasks(
    Utils::JsonResponseWriter& writer,
    const SystemTasksSnapshot& snapshot) {
    const TaskStatus_t* worstTask = nullptr;
    if (snapshot.detailsIncluded && snapshot.tasks) {
        for (UBaseType_t i = 0; i < snapshot.actualCount; i++) {
            const TaskStatus_t& task = snapshot.tasks[i];
            if (!worstTask || task.usStackHighWaterMark < worstTask->usStackHighWaterMark) {
                worstTask = &task;
            }
        }
    }

    writer.raw("{");
    writer.key("watchdog"); writer.raw("{");
    writer.key("initialized"); writer.value(snapshot.watchdogInitialized); writer.raw(",");
    writer.key("timeoutSec"); writer.value(static_cast<unsigned long>(snapshot.watchdogTimeoutSec));
    writer.raw("},");

    writer.key("taskCount"); writer.value(static_cast<unsigned long>(snapshot.taskCount)); writer.raw(",");
    writer.key("detailsIncluded"); writer.value(snapshot.detailsIncluded);
    if (snapshot.allocationFailed) {
        writer.raw(",");
        writer.key("error"); writer.string("PSRAM alloc failed");
    } else if (snapshot.detailsIncluded && snapshot.tasks) {
        writer.raw(",");
        writer.key("tasks"); writer.raw("[");
        for (UBaseType_t i = 0; i < snapshot.actualCount; i++) {
            if (i > 0) {
                writer.raw(",");
            }

            const TaskStatus_t& task = snapshot.tasks[i];
            writer.raw("{");
            writer.key("name"); writer.string(task.pcTaskName); writer.raw(",");
            writer.key("priority"); writer.value(static_cast<unsigned long>(task.uxCurrentPriority)); writer.raw(",");
            writer.key("stackHighWaterMark"); writer.value(static_cast<unsigned long>(task.usStackHighWaterMark)); writer.raw(",");
            writer.key("state"); writer.string(taskStateName(task.eCurrentState)); writer.raw(",");
#if configNUM_CORES > 1
            writer.key("coreId"); writer.value((task.xCoreID == tskNO_AFFINITY) ? -1 : static_cast<int>(task.xCoreID));
#else
            writer.key("coreId"); writer.value(0);
#endif
            writer.raw("}");
        }
        writer.raw("]");
    }

    writer.raw(",");
    writer.key("stack"); writer.raw("{");
    writer.key("detailsAvailable"); writer.value(worstTask != nullptr);
    if (worstTask) {
        writer.raw(",");
        writer.key("worstTask"); writer.string(worstTask->pcTaskName); writer.raw(",");
        writer.key("worstHighWaterMark"); writer.value(static_cast<unsigned long>(worstTask->usStackHighWaterMark));
    }
    writer.raw("},");
    writer.key("memory"); writer.raw("{");
    writer.key("freeHeap"); writer.value(static_cast<unsigned long>(snapshot.freeHeap)); writer.raw(",");
    writer.key("minFreeHeap"); writer.value(static_cast<unsigned long>(snapshot.minFreeHeap)); writer.raw(",");
    writer.key("freePsram"); writer.value(static_cast<unsigned long>(snapshot.freePsram)); writer.raw(",");
    writer.key("default"); writeMemoryRegion(writer, snapshot.defaultHeap); writer.raw(",");
    writer.key("internal"); writeMemoryRegion(writer, snapshot.internalHeap); writer.raw(",");
    writer.key("psram"); writeMemoryRegion(writer, snapshot.psram);
    writer.raw("}");
    writer.raw("}");
}

StateHandlerResult SystemSnapshotJsonWriter::writeNetwork(
    Utils::JsonResponseWriter& writer,
    const SystemNetworkSnapshot& snapshot) {
    if (!snapshot.available || !snapshot.wifi) {
        return StateHandlerResult::failure("service/wifi_unavailable", 500);
    }

    const WiFiConnectivityDiagnostics& wifi = *snapshot.wifi;
    writer.raw("{");

    writer.key("wifi"); writer.raw("{");
    writer.key("state"); writer.string(wifiConnectivityStateName(wifi.state)); writer.raw(",");
    writer.key("configured_mode"); writer.string(wifiOperatingModeName(wifi.configuredMode)); writer.raw(",");
    writer.key("mode"); writer.string(wifiModeName(wifi.wifiMode)); writer.raw(",");
    writer.key("mode_id"); writer.value(static_cast<int>(wifi.wifiMode)); writer.raw(",");
    writer.key("sta_connected"); writer.value(wifi.staConnected); writer.raw(",");
    writer.key("ap_active"); writer.value(wifi.apActive); writer.raw(",");
    writer.key("last_disconnect_reason"); writer.value(static_cast<unsigned long>(wifi.lastDisconnectReason)); writer.raw(",");
    writer.key("last_ip_change_ms"); writer.value(static_cast<unsigned long>(wifi.lastIpChangeMs)); writer.raw(",");
    writer.key("disconnected_since_ms"); writer.value(static_cast<unsigned long>(wifi.disconnectedSinceMs)); writer.raw(",");
    writer.key("stable_connected_since_ms"); writer.value(static_cast<unsigned long>(wifi.stableConnectedSinceMs)); writer.raw(",");
    writer.key("last_recovery_reason"); writer.string(wifi.lastRecoveryReason);
    if (isSetIp(wifi.staIp)) {
        writer.raw(",");
        writeIpIfSet(writer, "sta_ip", wifi.staIp);
    }
    if (isSetIp(wifi.savedStaticIp)) {
        writer.raw(",");
        writeIpIfSet(writer, "saved_static_ip", wifi.savedStaticIp);
    }
    writer.raw("},");

    writer.key("ap"); writer.raw("{");
    writer.key("active"); writer.value(wifi.apActive); writer.raw(",");
    writer.key("station_num"); writer.value(static_cast<unsigned long>(wifi.apStationCount));
    if (isSetIp(wifi.apIp)) {
        writer.raw(",");
        writeIpIfSet(writer, "ip", wifi.apIp);
    }
    writer.raw("},");

    writer.key("http"); writer.raw("{");
    writer.key("active_clients"); writer.value(static_cast<unsigned long>(snapshot.http.activeClients)); writer.raw(",");
    writer.key("peak_clients"); writer.value(static_cast<unsigned long>(snapshot.http.peakClients)); writer.raw(",");
    writer.key("opens"); writer.value(static_cast<unsigned long>(snapshot.http.openCount)); writer.raw(",");
    writer.key("closes"); writer.value(static_cast<unsigned long>(snapshot.http.closeCount)); writer.raw(",");
    writer.key("last_open_ms"); writer.value(static_cast<unsigned long>(snapshot.http.lastOpenMs)); writer.raw(",");
    writer.key("last_close_ms"); writer.value(static_cast<unsigned long>(snapshot.http.lastCloseMs)); writer.raw(",");
    writer.key("ws_active_clients"); writer.value(static_cast<unsigned long>(snapshot.http.wsActiveClients)); writer.raw(",");
    writer.key("ws_peak_clients"); writer.value(static_cast<unsigned long>(snapshot.http.wsPeakClients)); writer.raw(",");
    writer.key("ws_opens"); writer.value(static_cast<unsigned long>(snapshot.http.wsOpenCount)); writer.raw(",");
    writer.key("ws_closes"); writer.value(static_cast<unsigned long>(snapshot.http.wsCloseCount)); writer.raw(",");
    writer.key("last_ws_open_ms"); writer.value(static_cast<unsigned long>(snapshot.http.lastWsOpenMs)); writer.raw(",");
    writer.key("last_ws_close_ms"); writer.value(static_cast<unsigned long>(snapshot.http.lastWsCloseMs)); writer.raw(",");
    writer.key("ws_forced_removals"); writer.value(static_cast<unsigned long>(snapshot.http.wsForcedRemovals)); writer.raw(",");
    writer.key("ws_queue_drops"); writer.value(static_cast<unsigned long>(snapshot.http.wsQueueDrops)); writer.raw(",");
    writer.key("last_ws_queue_drop_ms"); writer.value(static_cast<unsigned long>(snapshot.http.lastWsQueueDropMs)); writer.raw(",");
    writer.key("last_ws_queue_drop_payload"); writer.value(static_cast<unsigned long>(snapshot.http.lastWsQueueDropPayload)); writer.raw(",");
    writer.key("ws_heap_fallbacks"); writer.value(static_cast<unsigned long>(snapshot.http.wsHeapFallbacks)); writer.raw(",");
    writer.key("last_ws_heap_fallback_ms"); writer.value(static_cast<unsigned long>(snapshot.http.lastWsHeapFallbackMs)); writer.raw(",");
    writer.key("last_ws_heap_fallback_payload"); writer.value(static_cast<unsigned long>(snapshot.http.lastWsHeapFallbackPayload)); writer.raw(",");
    writer.key("max_ws_heap_fallback_payload"); writer.value(static_cast<unsigned long>(snapshot.http.maxWsHeapFallbackPayload));
    writer.raw("},");

    writer.key("forwarding"); writer.raw("{");
    writer.key("ready"); writer.value(wifi.portForwardingReady); writer.raw(",");
    writer.key("requires_static_ip"); writer.value(true); writer.raw(",");
    writer.key("saved_static_ip_configured"); writer.value(wifi.savedStaticIpConfigured); writer.raw(",");
    writer.key("saved_static_ip_matches"); writer.value(wifi.savedStaticIpMatches); writer.raw(",");
    writer.key("https_port"); writer.value(443);
    writer.raw("}");

    writer.raw("}");
    return StateHandlerResult::success();
}

}  // namespace API
