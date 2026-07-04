#include "SystemSnapshots.h"

#include "../../SystemApiDependencies.h"
#include "../../../../config/App.h"
#include "../../../../config/System.h"
#include "../../../../sensors/SensorLoggingTask.h"
#include "../../../../sensors/logging/PsramLogBuffer.h"
#include "../../../../sensors/logging/SensorBinaryLogger.h"
#include "../../../../sensors/runtime/SensorSnapshotHealth.h"
#include "../../../../system/datalogger/BinaryFormat.h"
#include "../../../../system/logging/Logging.h"
#include "../../../../system/memory/PsramAllocator.h"
#include "../../../../system/rtc/RtcConfig.h"
#include "../../../../system/health/network/HttpServerHealthTracker.h"

#include <wifi/WiFiSettingsService.h>

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_wifi.h>

#include <cstring>

#ifndef APP_VERSION
#define APP_VERSION APP::VERSION
#endif
namespace API::SYSTEM_WS {

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

bool formatIpAddress(const IPAddress& ip, char* out, size_t outSize) {
    if (!out || outSize == 0 || !isSetIp(ip)) {
        return false;
    }

    return snprintf(out,
                    outSize,
                    "%u.%u.%u.%u",
                    static_cast<unsigned>(ip[0]),
                    static_cast<unsigned>(ip[1]),
                    static_cast<unsigned>(ip[2]),
                    static_cast<unsigned>(ip[3])) > 0;
}

void setIpField(JsonObject obj, const char* key, const IPAddress& ip) {
    char ipBuf[16];
    if (formatIpAddress(ip, ipBuf, sizeof(ipBuf))) {
        obj[key] = ipBuf;
    }
}

void formatMacAddress(const uint8_t* mac, char* out, size_t outSize) {
    if (!mac || !out || outSize == 0) {
        return;
    }

    snprintf(out,
             outSize,
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0],
             mac[1],
             mac[2],
             mac[3],
             mac[4],
             mac[5]);
}

}  // namespace

void sendTelemetrySnapshot(const SnapshotContext& ctx) {
    (void)ctx;

    SensorSnapshot snap = SensorLoggingTask::getSnapshot();
    PhaseStatus readStatus = SensorLoggingTask::getLastReadStatus();
    const uint32_t nowMs = millis();
    // JSON snapshots should not report an old retained sample as "healthy"
    // forever. They intentionally reuse the same freshness rule as the task
    // loop's STALE promotion and SensorState helpers.
    const bool snapshotFresh =
        SENSORS::isSnapshotFresh(snap.timestamp_ms, nowMs, SENSOR::SNAPSHOT_TIMEOUT_MS);
    const bool lastReadOk = readStatus.ok && snapshotFresh;

    SYSTEM::SpiRamJsonDocument doc(4096);
    doc["type"] = "snapshot";
    doc["channel"] = "telemetry";

    JsonObject data = doc["data"].to<JsonObject>();
    data[CONFIG::Keys::kCo2] = snap.co2;
    data[CONFIG::Keys::kTemp] = snap.temp;
    data[CONFIG::Keys::kHumid] = snap.humid;
    data["lastReadOk"] = lastReadOk;

    const size_t maxHistory = LIMITS::API::CHART_WS_HISTORY_POINTS;
    if (maxHistory > 0) {
        // Keep the temporary container in PSRAM too; the hot path already stores
        // sensor history there and snapshot generation should not pull that cost
        // back into internal heap during UI reconnects or refresh storms.
        SYSTEM::PsramVector<DATALOG::BinaryLogRecord> records;
        records.resize(maxHistory);
        const size_t count = SENSORS::PsramLogBuffer::copyLastRecords(records.data(), maxHistory);

        JsonObject history = data["history"].to<JsonObject>();
        JsonArray timestamps = history["timestamps"].to<JsonArray>();
        JsonArray co2Arr = history["co2"].to<JsonArray>();
        JsonArray tempArr = history["temp"].to<JsonArray>();
        JsonArray humidArr = history["humid"].to<JsonArray>();

        bool hasValidEpoch = false;
        for (size_t i = 0; i < count; i++) {
            if (records[i].timestamp >= SENSORS::SensorBinaryLogger::MIN_VALID_EPOCH) {
                hasValidEpoch = true;
                break;
            }
        }

        uint32_t lastTs = 0;
        bool hasLastTs = false;
        for (size_t i = 0; i < count; i++) {
            uint32_t ts = records[i].timestamp;
            if (hasValidEpoch && ts < SENSORS::SensorBinaryLogger::MIN_VALID_EPOCH) {
                continue;
            }
            if (hasLastTs && ts < lastTs) {
                ts = lastTs + 1;
            }

            timestamps.add(ts);
            co2Arr.add(records[i].co2);
            if (records[i].temp_10x == INT16_MIN) {
                tempArr.add(nullptr);
            } else {
                tempArr.add(DATALOG::int16ToFloat_10x(records[i].temp_10x));
            }
            humidArr.add(DATALOG::uint16ToFloat_10x(records[i].humid_10x));
            lastTs = ts;
            hasLastTs = true;
        }
    }

    sendSnapshotDoc(ctx.ws, ctx.fd, doc);
}

void sendSystemStatusSnapshot(const SnapshotContext& ctx) {
    if (!ctx.sysDeps) {
        return;
    }

    // This is the rich, cross-feature snapshot used by the dashboard, WiFi STA,
    // WiFi AP and log-settings UI. It intentionally carries more than the tiny
    // 1 Hz binary heartbeat because those views were moved away from separate
    // REST polling endpoints and now hydrate from one shared WS document.
    //
    // TODO: Keep this payload event-driven. If we later want "live" refreshes
    // without explicit snapshot requests, add dirty/debounce broadcasting for
    // meaningful state changes instead of turning this into a 1 Hz full JSON
    // stream.
    SYSTEM::HEALTH::SystemDiagnostics diagnostics = ctx.sysDeps->getDiagnostics();
    SYSTEM::SpiRamJsonDocument doc(8192);
    doc["type"] = "snapshot";
    doc["channel"] = "system_status";
    JsonObject data = doc["data"].to<JsonObject>();

    JsonObject info = data["system_info"].to<JsonObject>();
    esp_chip_info_t chipInfo;
    esp_chip_info(&chipInfo);

    uint32_t flashSize = 0;
    if (esp_flash_get_size(nullptr, &flashSize) != ESP_OK) {
        flashSize = 0;
    }

    uint8_t macAddr[6];
    WiFi.macAddress(macAddr);
    char macBuf[20];
    formatMacAddress(macAddr, macBuf, sizeof(macBuf));

    uint32_t internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    uint32_t internalTotal = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    const uint32_t psramSize = ESP.getPsramSize();
    const uint32_t psramFree = ESP.getFreePsram();
    const RTC::WifiSensingData wifiSensingCfg =
        RTC::copyConfigSection(&RTC::ConfigStore::wifiSensing);
    const RTC::BleData bleCfg = RTC::copyConfigSection(&RTC::ConfigStore::ble);
    const RTC::ShellySummaryData shellyCfg = RTC::copyConfigSection(&RTC::ConfigStore::shelly);
    const ALARMS::AlarmRuntimeSummary alarmsCfg =
        RTC::copyConfigSection(&RTC::ConfigStore::alarms);

    // system_status is a dashboard snapshot, not a full clone of
    // /api/system/info. Keep only the fields that current frontend cards
    // actually render so the websocket payload stays cheap to request and
    // inspect during debugging.
    info[CONFIG::Keys::kFirmwareVersion] = APP_VERSION;
    info[CONFIG::Keys::kCpuFreqMhz] = static_cast<unsigned int>(getCpuFrequencyMhz());
    info[CONFIG::Keys::kCpuType] = ESP.getChipModel();
    info[CONFIG::Keys::kCpuRev] = static_cast<int>(chipInfo.revision);
    info[CONFIG::Keys::kCpuCores] = static_cast<int>(chipInfo.cores);
    info[CONFIG::Keys::kFlashChipSize] = static_cast<unsigned long>(flashSize);
    info[CONFIG::Keys::kFlashChipSpeed] = static_cast<unsigned int>(ESP.getFlashChipSpeed());
    info[CONFIG::Keys::kSketchSize] = ESP.getSketchSize();
    info[CONFIG::Keys::kMacAddress] = macBuf;
    info[CONFIG::Keys::kSdkVersion] = ESP.getSdkVersion();
    info[CONFIG::Keys::kArduinoVersion] = ESP_ARDUINO_VERSION_STR;

    // Keep reset reason numeric end-to-end so the frontend does not need to
    // parse an integer back out of a stringified enum value.
    info[CONFIG::Keys::kCpuResetReason] = static_cast<int>(esp_reset_reason());

    info[CONFIG::Keys::kCoreTemp] = temperatureRead();
    info[CONFIG::Keys::kFreeHeap] = internalFree;
    info[CONFIG::Keys::kTotalHeap] = internalTotal;

    info[CONFIG::Keys::kPsramSize] = psramSize;
    info[CONFIG::Keys::kFreePsram] = psramFree;
    info[CONFIG::Keys::kUsedPsram] = psramSize - psramFree;

    info[CONFIG::Keys::kFsTotal] = static_cast<unsigned long>(ctx.sysDeps->getFsTotalBytes());
    info[CONFIG::Keys::kFsUsed] = static_cast<unsigned long>(ctx.sysDeps->getFsUsedBytes());
    info[CONFIG::Keys::kUptime] = static_cast<unsigned long>(millis() / 1000);
    info[CONFIG::Keys::kCompileDate] = __DATE__;
    info[CONFIG::Keys::kCompileTime] = __TIME__;

    JsonObject dashboardWidgets = data["dashboard_widgets"].to<JsonObject>();
    JsonObject bleWidget = dashboardWidgets["ble"].to<JsonObject>();
    bleWidget[CONFIG::Keys::kEnabled] = bleCfg.enabled;
    bleWidget["sensor_count"] = bleCfg.sensorCount;

    JsonObject shellyWidget = dashboardWidgets["shelly"].to<JsonObject>();
    shellyWidget["device_count"] = shellyCfg.deviceCount;

    JsonObject alarmsWidget = dashboardWidgets["alarms"].to<JsonObject>();
    alarmsWidget["rule_count"] = alarmsCfg.ruleCount;

    JsonObject sensingWidget = dashboardWidgets["wifi_sensing"].to<JsonObject>();
    sensingWidget[CONFIG::Keys::kEnabled] = wifiSensingCfg.enabled;

    JsonObject diag = data["diagnostics"].to<JsonObject>();
    // diagnostics is the contract consumed by most FE management/status views.
    // The frontend intentionally derives smaller screen-specific models from
    // this shared object instead of hitting many tiny status endpoints in parallel.
    JsonObject heap = diag["heap"].to<JsonObject>();
    // Keep only the metrics still consumed by the dashboard. Older/free-form
    // heap counters remain available elsewhere, but duplicating them here just
    // bloats every manual snapshot.
    heap["largest"] = static_cast<unsigned long>(diagnostics.heapLargest);
    heap["fragmentation"] = static_cast<int>(diagnostics.heapFragmentation);

    JsonObject wifi = diag["wifi"].to<JsonObject>();
    const bool wifiConnected = WiFi.isConnected();
    wifi["connected"] = wifiConnected;
    wifi["reconnects"] = diagnostics.wifiReconnects;
    wifi["lastDisconnectReason"] = static_cast<int>(diagnostics.wifiDisconnectReason);
    wifi["rssi"] = diagnostics.wifiRssi;
    wifi["healthy"] = diagnostics.wifiHealthy;
    wifi["mac"] = macBuf;

    // These link-layer/runtime fields mirror what older WiFi REST endpoints used
    // to expose separately. Keeping them here lets frontend cards share one WS
    // snapshot instead of polling /rest/wifiStatus and /rest/apStatus.
    if (wifiConnected) {
        wifi_ap_record_t apInfo{};
        if (esp_wifi_sta_get_ap_info(&apInfo) == ESP_OK) {
            char ssid[33] = {0};
            memcpy(ssid, apInfo.ssid, sizeof(apInfo.ssid));
            size_t len = strnlen(reinterpret_cast<const char*>(apInfo.ssid), 32);
            ssid[len] = '\0';
            wifi["ssid"] = ssid;

            char bssidBuf[20];
            formatMacAddress(apInfo.bssid, bssidBuf, sizeof(bssidBuf));
            wifi["bssid"] = bssidBuf;
        }

        setIpField(wifi, "ip", WiFi.localIP());
        setIpField(wifi, "gateway", WiFi.gatewayIP());
        setIpField(wifi, "subnet", WiFi.subnetMask());
        setIpField(wifi, "dns", WiFi.dnsIP(0));

        wifi["channel"] = static_cast<int>(WiFi.channel());
    }

    if (ctx.wifiSettingsService) {
        // WiFiSettingsService owns the configured mode, reconnect state,
        // forwarding readiness, stable/disconnected timestamps, and AP station count.
        // Folding that into system_status keeps transport simple: one snapshot,
        // one consumer contract on the frontend.
        const WiFiConnectivityDiagnostics connectivity = ctx.wifiSettingsService->getConnectivityDiagnostics();
        wifi["state"] = wifiConnectivityStateName(connectivity.state);
        wifi["configuredMode"] = wifiOperatingModeName(connectivity.configuredMode);
        wifi["mode"] = wifiModeName(connectivity.wifiMode);
        wifi["apActive"] = connectivity.apActive;
        wifi["lastRecoveryReason"] = connectivity.lastRecoveryReason;
        wifi["lastIpChangeMs"] = connectivity.lastIpChangeMs;
        wifi["disconnectedSinceMs"] = connectivity.disconnectedSinceMs;
        wifi["stableConnectedSinceMs"] = connectivity.stableConnectedSinceMs;
        setIpField(wifi, "savedStaticIp", connectivity.savedStaticIp);

        JsonObject ap = diag["ap"].to<JsonObject>();
        ap["active"] = connectivity.apActive;
        ap["stationNum"] = connectivity.apStationCount;
        uint8_t apMacAddr[6];
        if (esp_wifi_get_mac(WIFI_IF_AP, apMacAddr) == ESP_OK) {
            char apMacBuf[20];
            formatMacAddress(apMacAddr, apMacBuf, sizeof(apMacBuf));
            ap["mac"] = apMacBuf;
        }
        setIpField(ap, "ip", connectivity.apIp);

        JsonObject forwarding = diag["forwarding"].to<JsonObject>();
        forwarding["ready"] = connectivity.portForwardingReady;
        forwarding["requiresStaticIp"] = true;
        forwarding["savedStaticIpConfigured"] = connectivity.savedStaticIpConfigured;
        forwarding["savedStaticIpMatches"] = connectivity.savedStaticIpMatches;
        forwarding["httpsPort"] = 443;
    }

    const auto http = SYSTEM::HEALTH::HttpServerHealthTracker::getSnapshot();
    JsonObject httpInfo = diag["http"].to<JsonObject>();
    httpInfo["activeClients"] = http.activeClients;
    httpInfo["peakClients"] = http.peakClients;
    httpInfo["opens"] = http.openCount;
    httpInfo["closes"] = http.closeCount;
    httpInfo["lastOpenMs"] = http.lastOpenMs;
    httpInfo["lastCloseMs"] = http.lastCloseMs;
    httpInfo["wsActiveClients"] = http.wsActiveClients;
    httpInfo["wsPeakClients"] = http.wsPeakClients;
    httpInfo["wsOpens"] = http.wsOpenCount;
    httpInfo["wsCloses"] = http.wsCloseCount;
    httpInfo["lastWsOpenMs"] = http.lastWsOpenMs;
    httpInfo["lastWsCloseMs"] = http.lastWsCloseMs;
    httpInfo["wsForcedRemovals"] = http.wsForcedRemovals;
    httpInfo["wsQueueDrops"] = http.wsQueueDrops;
    httpInfo["lastWsQueueDropMs"] = http.lastWsQueueDropMs;
    httpInfo["lastWsQueueDropPayload"] = http.lastWsQueueDropPayload;
    httpInfo["wsHeapFallbacks"] = http.wsHeapFallbacks;
    httpInfo["lastWsHeapFallbackMs"] = http.lastWsHeapFallbackMs;
    httpInfo["lastWsHeapFallbackPayload"] = http.lastWsHeapFallbackPayload;
    httpInfo["maxWsHeapFallbackPayload"] = http.maxWsHeapFallbackPayload;

    JsonObject runtime = diag["runtime"].to<JsonObject>();
    // The dashboard uses uptimeMs for relative timestamps and
    // maintenanceSleeps for hygiene visibility. The removed runtime counters
    // were not read by the current frontend anymore.
    runtime["uptimeMs"] = diagnostics.uptimeMs;
    runtime["maintenanceSleeps"] = diagnostics.maintenanceSleepCount;

    // Keep the dashboard health verdict wired to the shared config thresholds
    // rather than duplicating literals here. Otherwise future heap tuning in
    // Network.h can silently drift away from the snapshot/UI contract.
    const bool healthy =
        diagnostics.wifiHealthy &&
        diagnostics.heapFree > LIMITS::API::HEALTH_HEAP_FREE_MIN &&
        diagnostics.heapLargest > LIMITS::API::HEALTH_HEAP_LARGEST_MIN;
    diag["healthy"] = healthy;

    if (ctx.wifiSettingsService) {
        const WiFiConnectivityDiagnostics connectivity =
            ctx.wifiSettingsService->getConnectivityDiagnostics();

        data["wifi_ap_mode"] = connectivity.apActive;
    }

    // LiveTail uses this to hydrate the current logging level without a second
    // config fetch on page entry.
    // TODO: If system_status becomes more chatty in the future, consider moving
    // mutable UI config like logging level into a smaller dedicated channel.
    JsonObject config = data["config"].to<JsonObject>();
    JsonObject logging = config[CONFIG::Keys::kLogging].to<JsonObject>();
    RTC::LoggingData loggingCfg = RTC::copyConfigSection(&RTC::ConfigStore::logging);
    logging[CONFIG::Keys::kLevel] = LOG::Logging::levelToString(loggingCfg.level);

    sendSnapshotDoc(ctx.ws, ctx.fd, doc);
}

}  // namespace API::SYSTEM_WS
