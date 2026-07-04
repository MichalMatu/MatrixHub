#include "SensorsApiService.h"

#include "../../system/utils/json/JsonResponseWriter.h"
#include "../../config/App.h"
#include "../../sensors/SensorLoggingTask.h"
#include "../../sensors/runtime/SensorSnapshotHealth.h"
#include "../../system/power/PowerManager.h"
#include "../../system/health/heap/HeapMonitor.h"
#include "../../system/logging/Logging.h"
#include <utils/ResponseUtils.h>
#include <ArduinoJson.h>
#include <PsychicJson.h>
#include <PsychicStreamResponse.h>
#include <cmath>

#undef LOG_TAG
#define LOG_TAG "ApiSensors"

namespace API {

SensorsApiService::SensorsApiService(PsychicHttpServer* server, SecurityManager* securityManager, POWER::PowerManager* powerManager, SYSTEM::HeapMonitor* heapMonitor)
    : BaseApiService(server, securityManager, powerManager, "api/sensors"), _heapMonitor(heapMonitor) {}

void SensorsApiService::begin() {
    // Endpoint removed: /api/sensors is now provided via WebSocket telemetry snapshot
}

esp_err_t SensorsApiService::handleGetSensors(PsychicRequest* request) {
    uint32_t heapBefore = ESP.getFreeHeap();
    uint32_t largestBefore = ESP.getMaxAllocHeap();
    
    // Task continuously polls sensor every 2s - just return cached snapshot
    SensorSnapshot snap = SensorLoggingTask::getSnapshot();

    PhaseStatus readStatus = SensorLoggingTask::getLastReadStatus();
    SensorSnapshot lastGood = SensorLoggingTask::getLastGoodSnapshot();
    ErrorInfo lastError = SensorLoggingTask::getLastErrorInfo();

    Utils::JsonResponseWriter w(request->request());
    if (!w.beginResponse()) return ESP_FAIL;

    const uint32_t nowMs = millis();
    const uint32_t age_ms =
        (snap.timestamp_ms != 0 && nowMs >= snap.timestamp_ms) ? (nowMs - snap.timestamp_ms) : 0;
    // Keep the REST view aligned with WS/live semantics: old retained values are
    // still useful for diagnostics, but they should no longer look like a fresh
    // successful read once they age past the configured timeout.
    const bool snapshotFresh =
        SENSORS::isSnapshotFresh(snap.timestamp_ms, nowMs, SENSOR::SNAPSHOT_TIMEOUT_MS);
    const bool lastReadOk = readStatus.ok && snapshotFresh;
    
    w.raw("{");
    w.key(CONFIG::Keys::kCo2); w.value(snap.co2); w.raw(",");
    w.key(CONFIG::Keys::kTemp); w.value(snap.temp); w.raw(",");
    w.key(CONFIG::Keys::kHumid); w.value(snap.humid); w.raw(",");
    
    w.key("timestamp_ms"); w.value((unsigned long)snap.timestamp_ms); w.raw(",");
    w.key("seq"); w.value((unsigned int)snap.seq); w.raw(",");
    w.key("age_ms"); w.value((unsigned long)age_ms); w.raw(",");
    w.key("age_sec"); w.value((unsigned long)(age_ms / 1000)); w.raw(",");
    
    w.key("lastReadOk"); w.value(lastReadOk); w.raw(",");
    w.key("stale"); w.value(!snapshotFresh); w.raw(",");
    w.key("lastGoodSeq"); w.value((unsigned int)lastGood.seq); w.raw(",");
    w.key("lastGoodTimestamp_ms"); w.value((unsigned long)lastGood.timestamp_ms);

    if (readStatus.error_code) {
        w.raw(","); w.key("lastError"); w.string(readStatus.error_code);
    }

    if (lastError.code) {
        w.raw(","); w.key("lastErrorInfo"); w.raw("{");
        w.key("code"); w.string(lastError.code); w.raw(",");
        w.key("timestamp_ms"); w.value((unsigned long)lastError.timestamp_ms);
        w.raw("}");
    }

    w.raw("}");
    w.finish();

    uint32_t heapAfter = ESP.getFreeHeap();
    uint32_t largestAfter = ESP.getMaxAllocHeap();
    int32_t deltaFree = (int32_t)heapAfter - (int32_t)heapBefore;
    int32_t deltaLargest = (int32_t)largestAfter - (int32_t)largestBefore;
    
    if (abs(deltaFree) > 6000 || deltaLargest < -4096) {
        LOGW("[WebUI] /api/sensors: ΔFree=%+d ΔLargest=%+d (now: %u/%u)",
             deltaFree, deltaLargest, heapAfter, largestAfter);
    }

    if (deltaFree <= -2048 || deltaLargest <= -2048) {
        static uint32_t lastProbeMs = 0;
        const uint32_t nowMs = millis();
        if (nowMs - lastProbeMs >= 30000) {
            if (_heapMonitor) {
                _heapMonitor->armDeferredProbe("sensors", 250, heapAfter, largestAfter);
            }
            lastProbeMs = nowMs;
        }
    }

    return ESP_OK;
}

}  // namespace API
