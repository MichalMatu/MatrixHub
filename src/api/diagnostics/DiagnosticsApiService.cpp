#include "DiagnosticsApiService.h"

#include "../common/RequestQueryUtils.h"
#include "../system/snapshots/SystemApiSnapshots.h"
#include "../system/snapshots/SystemSnapshotJsonWriter.h"
#include "DiagnosticsJsonWriter.h"

#include "../../alarms/AlarmSettingsService.h"
#include "../../airmouse/AirMouseService.h"
#include "../../ble/BleService.h"
#include "../../ble/settings/BleSettingsService.h"
#include "../../compensation/CompensationSettingsService.h"
#include "../../keyboard/KeyboardService.h"
#include "../../keyboard/KeyboardSettingsService.h"
#include "../../macros/MacroService.h"
#include "../../system/boot/BootTracker.h"
#include "../../system/health/heartbeat/HeartbeatSettingsService.h"
#include "../../system/health/network/HttpServerHealthTracker.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../udp/UdpPusher.h"
#include "../../udp/UdpSettingsService.h"
#include "../../usb_terminal/UsbTerminalService.h"
#include "../../usb_terminal/UsbTerminalSettingsService.h"
#include "../../wifisensing/WifiSensingService.h"
#include "../../wifisensing/WifiSensingSettings.h"
#include "../../wifisensing/csi/core/CsiService.h"

#include <esp_system.h>

namespace API {

namespace {

const char* macroStatusName(MACROS::MacroStatus status) {
    switch (status) {
        case MACROS::MacroStatus::IDLE:
            return "idle";
        case MACROS::MacroStatus::RUNNING:
            return "running";
        case MACROS::MacroStatus::PAUSED:
            return "paused";
        case MACROS::MacroStatus::ERROR:
            return "error";
        case MACROS::MacroStatus::COMPLETED:
            return "completed";
        default:
            return "unknown";
    }
}

}  // namespace

DiagnosticsApiService::DiagnosticsApiService(
    PsychicHttpServer* server,
    SecurityManager* securityManager,
    POWER::PowerManager* powerManager,
    const DiagnosticsApiDeps& deps)
    : BaseApiService(server, securityManager, powerManager, "api/diagnostics")
    , _deps(deps) {}

void DiagnosticsApiService::begin() {
    _server->on("/api/diagnostics/summary", HTTP_GET,
                wrapAdmin([this](PsychicRequest* request) {
                    return handleSummary(request);
                }));
    _server->on("/api/diagnostics/heap", HTTP_GET,
                wrapAdmin([this](PsychicRequest* request) {
                    return handleHeap(request);
                }));
    _server->on("/api/diagnostics/tasks", HTTP_GET,
                wrapAdmin([this](PsychicRequest* request) {
                    return handleTasks(request);
                }));
    _server->on("/api/diagnostics/mutexes", HTTP_GET,
                wrapAdmin([this](PsychicRequest* request) {
                    return handleMutexes(request);
                }));
    _server->on("/api/diagnostics/endpoints", HTTP_GET,
                wrapAdmin([this](PsychicRequest* request) {
                    return handleEndpoints(request);
                }));
    _server->on("/api/diagnostics/features", HTTP_GET,
                wrapAdmin([this](PsychicRequest* request) {
                    return handleFeatures(request);
                }));
}

esp_err_t DiagnosticsApiService::handleSummary(PsychicRequest* request) {
    Utils::JsonResponseWriter writer(request->request());
    if (!writer.beginResponse()) {
        return ESP_FAIL;
    }
    DiagnosticsJsonWriter::writeSummary(writer, buildSummarySnapshot());
    writer.finish();
    return ESP_OK;
}

esp_err_t DiagnosticsApiService::handleHeap(PsychicRequest* request) {
    Utils::JsonResponseWriter writer(request->request());
    if (!writer.beginResponse()) {
        return ESP_FAIL;
    }
    DiagnosticsJsonWriter::writeHeap(writer, buildDiagnosticsHeapSnapshot());
    writer.finish();
    return ESP_OK;
}

esp_err_t DiagnosticsApiService::handleTasks(PsychicRequest* request) {
    Utils::JsonResponseWriter writer(request->request());
    if (!writer.beginResponse()) {
        return ESP_FAIL;
    }

    bool includeDetails = true;
    if (request->hasParam("details")) {
        includeDetails = RequestQuery::isTruthyParam(request->getParam("details")->value());
    }

    const SystemTasksSnapshot snapshot = buildSystemTasksSnapshot(_deps.tasks, includeDetails);
    SystemSnapshotJsonWriter::writeTasks(writer, snapshot);
    writer.finish();
    return snapshot.allocationFailed ? ESP_FAIL : ESP_OK;
}

esp_err_t DiagnosticsApiService::handleMutexes(PsychicRequest* request) {
    Utils::JsonResponseWriter writer(request->request());
    if (!writer.beginResponse()) {
        return ESP_FAIL;
    }
    DiagnosticsJsonWriter::writeMutexes(writer);
    writer.finish();
    return ESP_OK;
}

esp_err_t DiagnosticsApiService::handleEndpoints(PsychicRequest* request) {
    Utils::JsonResponseWriter writer(request->request());
    if (!writer.beginResponse()) {
        return ESP_FAIL;
    }
    DiagnosticsJsonWriter::writeEndpoints(writer);
    writer.finish();
    return ESP_OK;
}

esp_err_t DiagnosticsApiService::handleFeatures(PsychicRequest* request) {
    Utils::JsonResponseWriter writer(request->request());
    if (!writer.beginResponse()) {
        return ESP_FAIL;
    }
    DiagnosticsJsonWriter::writeFeatures(writer, buildFeaturesSnapshot());
    writer.finish();
    return ESP_OK;
}

DiagnosticsSummarySnapshot DiagnosticsApiService::buildSummarySnapshot() const {
    DiagnosticsSummarySnapshot snapshot{};
    const SystemInfoSnapshot info = buildSystemInfoSnapshot(_deps.info);

    snapshot.firmwareName = info.firmwareName;
    snapshot.firmwareVersion = info.firmwareVersion;
    snapshot.buildTarget = info.buildTarget;
    snapshot.uptimeSec = info.uptimeSec;
    snapshot.heap = buildDiagnosticsHeapSnapshot();
    snapshot.http = SYSTEM::HEALTH::HttpServerHealthTracker::getSnapshot();
    snapshot.watchdog.initialized =
        _deps.tasks.isWatchdogInitialized ? _deps.tasks.isWatchdogInitialized() : false;
    snapshot.watchdog.timeoutSec =
        _deps.tasks.getWatchdogTimeoutSec ? _deps.tasks.getWatchdogTimeoutSec() : 0;

    snapshot.boot.bootCount = SYSTEM::BootTracker::getBootCount();
    snapshot.boot.unexpectedRestarts = SYSTEM::BootTracker::getUnexpectedRestarts();
    snapshot.boot.lastBootUnexpected = SYSTEM::BootTracker::wasLastBootUnexpected();
    snapshot.boot.lastSessionUptimeMs = SYSTEM::BootTracker::getLastSessionUptimeMs();
    snapshot.boot.lastShutdownReason = SYSTEM::BootTracker::getLastShutdownReason();
    snapshot.boot.lastResetReason = SYSTEM::BootTracker::getLastResetReason();
    snapshot.boot.freeHeapAtShutdown = SYSTEM::BootTracker::getFreeHeapAtShutdown();
    snapshot.boot.currentResetReason = static_cast<int>(esp_reset_reason());

    DiagnosticsFeaturesSnapshot features = buildFeaturesSnapshot();
    snapshot.featureConfigRead = features.configRead;
    snapshot.featureCount = features.count;
    return snapshot;
}

DiagnosticsFeaturesSnapshot DiagnosticsApiService::buildFeaturesSnapshot() const {
    DiagnosticsFeaturesSnapshot snapshot{};

    bool wifiSensingEnabled = false;
    bool bleEnabled = false;
    bool udpEnabled = false;
    bool udpConfigured = false;
    bool macroEnabled = false;
    bool keyboardEnabled = false;
    bool usbTerminalEnabled = false;
    bool airMouseEnabled = false;
    bool compensationEnabled = false;
    bool notificationsConfigured = false;
    bool alarmsConfigured = false;
    bool matrixEnabled = false;

    RTC::withConfig([&](const RTC::ConfigStore& cfg) {
        snapshot.configRead = true;
        wifiSensingEnabled = cfg.wifiSensing.enabled;
        bleEnabled = cfg.ble.enabled;
        udpEnabled = cfg.udpPusher.enabled;
        udpConfigured = cfg.udpPusher.isValid();
        macroEnabled = cfg.macros.enabled;
        keyboardEnabled = cfg.keyboard.enabled;
        usbTerminalEnabled = cfg.usbTerminal.enabled;
        airMouseEnabled = cfg.airMouse.movementEnabled || cfg.airMouse.clickEnabled;
        compensationEnabled = cfg.compensation.enabled;
        notificationsConfigured = cfg.notification.isConfigured();
        alarmsConfigured = cfg.alarms.enabledCount > 0;
        matrixEnabled = cfg.matrix.effectEnabled || cfg.matrix.menu.enabled;
    });

    addDiagnosticsFeature(snapshot, {
        "wifi_sensing",
        _deps.wifiSensingService != nullptr,
        snapshot.configRead,
        wifiSensingEnabled,
        _deps.wifiSensingService != nullptr,
        _deps.wifiSensingService && _deps.wifiSensingService->isRunning(),
        nullptr,
    });

    addDiagnosticsFeature(snapshot, {
        "csi",
        _deps.csiService != nullptr,
        snapshot.configRead,
        wifiSensingEnabled,
        _deps.csiService != nullptr,
        _deps.csiService && _deps.csiService->isEnabled(),
        _deps.csiService && _deps.csiService->hasActiveConsumers()
            ? "active_consumers"
            : "no_active_consumers",
    });

    addDiagnosticsFeature(snapshot, {
        "ble",
        _deps.bleService != nullptr,
        snapshot.configRead,
        bleEnabled,
        _deps.bleService != nullptr,
        _deps.bleService && _deps.bleService->isRunning(),
        nullptr,
    });

    addDiagnosticsFeature(snapshot, {
        "alarms",
        _deps.alarmService != nullptr,
        snapshot.configRead,
        alarmsConfigured,
        true,
        alarmsConfigured,
        _deps.alarmSettings ? "rules_service_available" : "rules_service_missing",
    });

    addDiagnosticsFeature(snapshot, {
        "keyboard",
        _deps.keyboardService != nullptr,
        snapshot.configRead,
        keyboardEnabled,
        _deps.keyboardService != nullptr,
        _deps.keyboardService && _deps.keyboardService->isInitialized(),
        nullptr,
    });

    const bool macroRuntimeMeasured = _deps.macroService != nullptr;
    const char* macroDetail = nullptr;
    bool macroRuntimeActive = false;
    if (_deps.macroService) {
        const MACROS::MacroState state = _deps.macroService->getStatus();
        macroRuntimeActive = state.status == MACROS::MacroStatus::RUNNING;
        macroDetail = macroStatusName(state.status);
    }
    addDiagnosticsFeature(snapshot, {
        "macros",
        _deps.macroService != nullptr,
        snapshot.configRead,
        macroEnabled,
        macroRuntimeMeasured,
        macroRuntimeActive,
        macroDetail,
    });

    addDiagnosticsFeature(snapshot, {
        "airmouse",
        _deps.airMouseService != nullptr,
        snapshot.configRead,
        airMouseEnabled,
        _deps.airMouseService != nullptr,
        _deps.airMouseService && _deps.airMouseService->isRunning(),
        nullptr,
    });

    addDiagnosticsFeature(snapshot, {
        "usb_terminal",
        _deps.usbTerminalService != nullptr,
        snapshot.configRead,
        usbTerminalEnabled,
        false,
        false,
        _deps.usbTerminalService ? "runtime has no running-state getter" : "service_missing",
    });

    addDiagnosticsFeature(snapshot, {
        "udp_push",
        _deps.udpPusher != nullptr,
        snapshot.configRead,
        udpEnabled,
        false,
        false,
        udpConfigured ? "configured" : "not_configured",
    });

    addDiagnosticsFeature(snapshot, {
        "heartbeat",
        _deps.heartbeatSettings != nullptr,
        false,
        false,
        false,
        false,
        _deps.heartbeatSettings ? "settings_service_available" : "settings_service_missing",
    });

    addDiagnosticsFeature(snapshot, {
        "notifications",
        true,
        snapshot.configRead,
        notificationsConfigured,
        false,
        false,
        "summary_only_no_secret_fields",
    });

    addDiagnosticsFeature(snapshot, {
        "matrix",
        true,
        snapshot.configRead,
        matrixEnabled,
        false,
        false,
        "configuration_snapshot_only",
    });

    addDiagnosticsFeature(snapshot, {
        "compensation",
        _deps.compensationSettings != nullptr,
        snapshot.configRead,
        compensationEnabled,
        false,
        false,
        _deps.compensationSettings ? "settings_service_available" : "settings_service_missing",
    });

    return snapshot;
}

}  // namespace API
