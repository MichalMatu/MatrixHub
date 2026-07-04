#include "DiagnosticsJsonWriter.h"
#include "../../system/utils/LockDiagnostics.h"

namespace API {

namespace {

unsigned long ticksToMs(uint32_t ticks) {
    return static_cast<unsigned long>(ticks) * static_cast<unsigned long>(portTICK_PERIOD_MS);
}

void writeLockCounter(
    Utils::JsonResponseWriter& writer,
    const SYSTEM::LOCK_DIAGNOSTICS::LockCounterSnapshot& snapshot) {
    writer.raw("{");
    writer.key("attempts"); writer.value(static_cast<unsigned long>(snapshot.attempts)); writer.raw(",");
    writer.key("successes"); writer.value(static_cast<unsigned long>(snapshot.successes)); writer.raw(",");
    writer.key("timeouts"); writer.value(static_cast<unsigned long>(snapshot.timeouts)); writer.raw(",");
    writer.key("slowAcquires"); writer.value(static_cast<unsigned long>(snapshot.slowAcquires)); writer.raw(",");
    writer.key("unlimitedWaits"); writer.value(static_cast<unsigned long>(snapshot.unlimitedWaits)); writer.raw(",");
    writer.key("maxWaitTicks"); writer.value(static_cast<unsigned long>(snapshot.maxWaitTicks)); writer.raw(",");
    writer.key("maxWaitMs"); writer.value(ticksToMs(snapshot.maxWaitTicks));
    writer.raw("}");
}

}  // namespace

void DiagnosticsJsonWriter::writeHeapRegion(
    Utils::JsonResponseWriter& writer,
    const DiagnosticsHeapRegionSnapshot& region) {
    writer.raw("{");
    writer.key("name"); writer.string(region.name ? region.name : "unknown"); writer.raw(",");
    writer.key("available"); writer.value(region.available); writer.raw(",");
    writer.key("caps"); writer.value(static_cast<unsigned long>(region.caps)); writer.raw(",");
    writer.key("total"); writer.value(static_cast<unsigned long>(region.total)); writer.raw(",");
    writer.key("free"); writer.value(static_cast<unsigned long>(region.free)); writer.raw(",");
    writer.key("minimumFree"); writer.value(static_cast<unsigned long>(region.minimumFree)); writer.raw(",");
    writer.key("largestBlock"); writer.value(static_cast<unsigned long>(region.largestBlock)); writer.raw(",");
    writer.key("fragmentationPercent"); writer.value(static_cast<unsigned long>(region.fragmentationPercent));
    writer.raw("}");
}

void DiagnosticsJsonWriter::writeHttpHealth(
    Utils::JsonResponseWriter& writer,
    const SYSTEM::HEALTH::HttpServerHealthSnapshot& snapshot) {
    writer.raw("{");
    writer.key("activeClients"); writer.value(static_cast<unsigned long>(snapshot.activeClients)); writer.raw(",");
    writer.key("peakClients"); writer.value(static_cast<unsigned long>(snapshot.peakClients)); writer.raw(",");
    writer.key("opens"); writer.value(static_cast<unsigned long>(snapshot.openCount)); writer.raw(",");
    writer.key("closes"); writer.value(static_cast<unsigned long>(snapshot.closeCount)); writer.raw(",");
    writer.key("lastOpenMs"); writer.value(static_cast<unsigned long>(snapshot.lastOpenMs)); writer.raw(",");
    writer.key("lastCloseMs"); writer.value(static_cast<unsigned long>(snapshot.lastCloseMs)); writer.raw(",");
    writer.key("wsActiveClients"); writer.value(static_cast<unsigned long>(snapshot.wsActiveClients)); writer.raw(",");
    writer.key("wsPeakClients"); writer.value(static_cast<unsigned long>(snapshot.wsPeakClients)); writer.raw(",");
    writer.key("wsOpens"); writer.value(static_cast<unsigned long>(snapshot.wsOpenCount)); writer.raw(",");
    writer.key("wsCloses"); writer.value(static_cast<unsigned long>(snapshot.wsCloseCount)); writer.raw(",");
    writer.key("lastWsOpenMs"); writer.value(static_cast<unsigned long>(snapshot.lastWsOpenMs)); writer.raw(",");
    writer.key("lastWsCloseMs"); writer.value(static_cast<unsigned long>(snapshot.lastWsCloseMs)); writer.raw(",");
    writer.key("wsForcedRemovals"); writer.value(static_cast<unsigned long>(snapshot.wsForcedRemovals)); writer.raw(",");
    writer.key("wsQueueDrops"); writer.value(static_cast<unsigned long>(snapshot.wsQueueDrops)); writer.raw(",");
    writer.key("lastWsQueueDropMs"); writer.value(static_cast<unsigned long>(snapshot.lastWsQueueDropMs)); writer.raw(",");
    writer.key("lastWsQueueDropPayload"); writer.value(static_cast<unsigned long>(snapshot.lastWsQueueDropPayload)); writer.raw(",");
    writer.key("wsHeapFallbacks"); writer.value(static_cast<unsigned long>(snapshot.wsHeapFallbacks)); writer.raw(",");
    writer.key("lastWsHeapFallbackMs"); writer.value(static_cast<unsigned long>(snapshot.lastWsHeapFallbackMs)); writer.raw(",");
    writer.key("lastWsHeapFallbackPayload"); writer.value(static_cast<unsigned long>(snapshot.lastWsHeapFallbackPayload)); writer.raw(",");
    writer.key("maxWsHeapFallbackPayload"); writer.value(static_cast<unsigned long>(snapshot.maxWsHeapFallbackPayload));
    writer.raw("}");
}

void DiagnosticsJsonWriter::writeHeap(
    Utils::JsonResponseWriter& writer,
    const DiagnosticsHeapSnapshot& snapshot) {
    writer.raw("{");
    writer.key("schema"); writer.string("diagnostics.heap.v1"); writer.raw(",");
    writer.key("regions"); writer.raw("{");
    writer.key("default"); writeHeapRegion(writer, snapshot.defaultHeap); writer.raw(",");
    writer.key("internal"); writeHeapRegion(writer, snapshot.internal); writer.raw(",");
    writer.key("psram"); writeHeapRegion(writer, snapshot.psram);
    writer.raw("}");
    writer.raw("}");
}

void DiagnosticsJsonWriter::writeSummary(
    Utils::JsonResponseWriter& writer,
    const DiagnosticsSummarySnapshot& snapshot) {
    writer.raw("{");
    writer.key("schema"); writer.string(snapshot.schema ? snapshot.schema : "diagnostics.v1"); writer.raw(",");
    writer.key("firmware"); writer.raw("{");
    writer.key("name"); writer.string(snapshot.firmwareName ? snapshot.firmwareName : "unknown"); writer.raw(",");
    writer.key("version"); writer.string(snapshot.firmwareVersion ? snapshot.firmwareVersion : "unknown"); writer.raw(",");
    writer.key("buildTarget"); writer.string(snapshot.buildTarget ? snapshot.buildTarget : "unknown");
    writer.raw("},");

    writer.key("uptimeSec"); writer.value(static_cast<unsigned long>(snapshot.uptimeSec)); writer.raw(",");

    writer.key("boot"); writer.raw("{");
    writer.key("bootCount"); writer.value(static_cast<unsigned long>(snapshot.boot.bootCount)); writer.raw(",");
    writer.key("unexpectedRestarts"); writer.value(static_cast<unsigned long>(snapshot.boot.unexpectedRestarts)); writer.raw(",");
    writer.key("lastBootUnexpected"); writer.value(snapshot.boot.lastBootUnexpected); writer.raw(",");
    writer.key("lastSessionUptimeMs"); writer.value(static_cast<unsigned long>(snapshot.boot.lastSessionUptimeMs)); writer.raw(",");
    writer.key("lastShutdownReason"); writer.value(static_cast<unsigned long>(snapshot.boot.lastShutdownReason)); writer.raw(",");
    writer.key("lastResetReason"); writer.value(static_cast<unsigned long>(snapshot.boot.lastResetReason)); writer.raw(",");
    writer.key("currentResetReason"); writer.value(snapshot.boot.currentResetReason); writer.raw(",");
    writer.key("freeHeapAtShutdown"); writer.value(static_cast<unsigned long>(snapshot.boot.freeHeapAtShutdown));
    writer.raw("},");

    writer.key("watchdog"); writer.raw("{");
    writer.key("initialized"); writer.value(snapshot.watchdog.initialized); writer.raw(",");
    writer.key("timeoutSec"); writer.value(static_cast<unsigned long>(snapshot.watchdog.timeoutSec));
    writer.raw("},");

    writer.key("heap"); writer.raw("{");
    writer.key("internalFree"); writer.value(static_cast<unsigned long>(snapshot.heap.internal.free)); writer.raw(",");
    writer.key("internalMinimumFree"); writer.value(static_cast<unsigned long>(snapshot.heap.internal.minimumFree)); writer.raw(",");
    writer.key("internalLargestBlock"); writer.value(static_cast<unsigned long>(snapshot.heap.internal.largestBlock)); writer.raw(",");
    writer.key("internalFragmentationPercent"); writer.value(static_cast<unsigned long>(snapshot.heap.internal.fragmentationPercent)); writer.raw(",");
    writer.key("psramFree"); writer.value(static_cast<unsigned long>(snapshot.heap.psram.free)); writer.raw(",");
    writer.key("psramMinimumFree"); writer.value(static_cast<unsigned long>(snapshot.heap.psram.minimumFree)); writer.raw(",");
    writer.key("psramLargestBlock"); writer.value(static_cast<unsigned long>(snapshot.heap.psram.largestBlock)); writer.raw(",");
    writer.key("psramFragmentationPercent"); writer.value(static_cast<unsigned long>(snapshot.heap.psram.fragmentationPercent));
    writer.raw("},");

    writer.key("http"); writeHttpHealth(writer, snapshot.http); writer.raw(",");

    writer.key("features"); writer.raw("{");
    writer.key("configRead"); writer.value(snapshot.featureConfigRead); writer.raw(",");
    writer.key("count"); writer.value(static_cast<unsigned long>(snapshot.featureCount));
    writer.raw("}");
    writer.raw("}");
}

void DiagnosticsJsonWriter::writeFeatures(
    Utils::JsonResponseWriter& writer,
    const DiagnosticsFeaturesSnapshot& snapshot) {
    writer.raw("{");
    writer.key("schema"); writer.string("diagnostics.features.v1"); writer.raw(",");
    writer.key("configRead"); writer.value(snapshot.configRead); writer.raw(",");
    writer.key("features"); writer.raw("[");
    for (size_t i = 0; i < snapshot.count; ++i) {
        if (i > 0) {
            writer.raw(",");
        }
        const DiagnosticsFeatureState& feature = snapshot.features[i];
        writer.raw("{");
        writer.key("key"); writer.string(feature.key ? feature.key : "unknown"); writer.raw(",");
        writer.key("serviceAvailable"); writer.value(feature.serviceAvailable); writer.raw(",");
        writer.key("configKnown"); writer.value(feature.configKnown); writer.raw(",");
        writer.key("configuredEnabled"); writer.value(feature.configuredEnabled); writer.raw(",");
        writer.key("runtimeMeasured"); writer.value(feature.runtimeMeasured); writer.raw(",");
        writer.key("runtimeActive"); writer.value(feature.runtimeActive);
        if (feature.detail) {
            writer.raw(",");
            writer.key("detail"); writer.string(feature.detail);
        }
        writer.raw("}");
    }
    writer.raw("]");
    writer.raw("}");
}

void DiagnosticsJsonWriter::writeMutexes(Utils::JsonResponseWriter& writer) {
    const auto snapshot = SYSTEM::LOCK_DIAGNOSTICS::snapshot();
    writer.raw("{");
    writer.key("schema"); writer.string("diagnostics.mutexes.v1"); writer.raw(",");
    writer.key("instrumented"); writer.value(true); writer.raw(",");
    writer.key("coverage"); writer.raw("{");
    writer.key("contentionCounters"); writer.value(true); writer.raw(",");
    writer.key("holdTimeBuckets"); writer.value(false); writer.raw(",");
    writer.key("timeoutCounters"); writer.value(true); writer.raw(",");
    writer.key("slowAcquireCounters"); writer.value(true); writer.raw(",");
    writer.key("perLockNames"); writer.value(false);
    writer.raw("},");
    writer.key("runtime"); writer.raw("{");
    writer.key("slowThresholdTicks"); writer.value(static_cast<unsigned long>(snapshot.slowThresholdTicks)); writer.raw(",");
    writer.key("slowThresholdMs"); writer.value(ticksToMs(snapshot.slowThresholdTicks)); writer.raw(",");
    writer.key("standard"); writeLockCounter(writer, snapshot.standard); writer.raw(",");
    writer.key("recursive"); writeLockCounter(writer, snapshot.recursive);
    writer.raw("},");
    writer.key("reason"); writer.string("ScopeLock/RecursiveScopeLock acquisition counters are global; per-lock hold-time buckets are not instrumented yet"); writer.raw(",");
    writer.key("criticalLocks"); writer.raw("[");
    const char* locks[] = {
        "rtc_config",
        "fs_mutex",
        "hid_keyboard",
        "csi_state",
        "csi_callback",
        "udp_state",
        "websocket_payload_pool",
        "websocket_client_manager",
        "alarm_settings",
    };
    for (size_t i = 0; i < sizeof(locks) / sizeof(locks[0]); ++i) {
        if (i > 0) {
            writer.raw(",");
        }
        writer.raw("{");
        writer.key("name"); writer.string(locks[i]); writer.raw(",");
        writer.key("instrumented"); writer.value(true); writer.raw(",");
        writer.key("counterScope"); writer.string("global");
        writer.raw("}");
    }
    writer.raw("]");
    writer.raw("}");
}

void DiagnosticsJsonWriter::writeEndpoints(Utils::JsonResponseWriter& writer) {
    writer.raw("{");
    writer.key("schema"); writer.string("diagnostics.endpoints.v1"); writer.raw(",");
    writer.key("metrics"); writer.raw("{");
    writer.key("requestCounts"); writer.value(false); writer.raw(",");
    writer.key("errorCounts"); writer.value(false); writer.raw(",");
    writer.key("latencyBuckets"); writer.value(false); writer.raw(",");
    writer.key("httpTransportCounters"); writer.value(true);
    writer.raw("},");
    writer.key("diagnostics"); writer.raw("[");
    for (size_t i = 0; i < kDiagnosticsEndpointCount; ++i) {
        if (i > 0) {
            writer.raw(",");
        }
        const DiagnosticsEndpointEntry& endpoint = kDiagnosticsEndpoints[i];
        writer.raw("{");
        writer.key("path"); writer.string(endpoint.path); writer.raw(",");
        writer.key("method"); writer.string(endpoint.method); writer.raw(",");
        writer.key("auth"); writer.string(endpoint.auth); writer.raw(",");
        writer.key("description"); writer.string(endpoint.description);
        writer.raw("}");
    }
    writer.raw("]");
    writer.raw("}");
}

}  // namespace API
