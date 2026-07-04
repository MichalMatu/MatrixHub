#include "SystemApiService.h"
#include "snapshots/SystemApiSnapshots.h"
#include "snapshots/SystemSnapshotJsonWriter.h"
#include "websocket/SystemWebsocketBroadcaster.h"
#include "../common/RequestQueryUtils.h"

#include <WiFi.h>
#include "../../system/utils/json/JsonResponseWriter.h"

namespace API {

SystemApiService::SystemApiService(
    PsychicHttpServer* server,
    SecurityManager* securityManager,
    POWER::PowerManager* powerManager,
    const SystemApiRouteDeps& routeDeps,
    const SystemApiBroadcastDeps& broadcastDeps)
    : BaseApiService(server, securityManager, powerManager, "api/system")
    , _routeDeps(routeDeps) {
    _broadcaster.reset(
        new SystemWebsocketBroadcaster(server, securityManager, broadcastDeps));
}

SystemApiService::~SystemApiService() = default;

void SystemApiService::begin() {
    _server->on("/api/system/tasks", HTTP_GET,
                wrapAuth([this](PsychicRequest* request) {
                    return this->handleTasks(request);
                }));

    _server->on("/api/system/wifi/recover", HTTP_POST,
                wrapAdmin([this](PsychicRequest* request) {
                    return this->handleWifiRecovery(request);
                }));

    _server->on("/api/system/info", HTTP_GET,
                wrapAuth([this](PsychicRequest* request) {
                    return this->handleSystemInfo(request);
                }));

    _server->on("/api/system/network", HTTP_GET,
                wrapAuth([this](PsychicRequest* request) {
                    return this->handleNetworkInfo(request);
                }));

    _broadcaster->begin();
}

void SystemApiService::sendShellyEvent(const void* devicePtr) {
    _broadcaster->sendShellyEvent(devicePtr);
}

void SystemApiService::cleanupClient(int fd) {
    _broadcaster->cleanupClient(fd);
}

esp_err_t SystemApiService::handleSystemInfo(PsychicRequest* request) {
    Utils::JsonResponseWriter writer(request->request());
    if (!writer.beginResponse()) {
        return ESP_FAIL;
    }
    const SystemInfoSnapshot snapshot = buildSystemInfoSnapshot(_routeDeps.info);
    SystemSnapshotJsonWriter::writeSystemInfo(writer, snapshot);
    writer.finish();
    return ESP_OK;
}

esp_err_t SystemApiService::handleTasks(PsychicRequest* request) {
    Utils::JsonResponseWriter writer(request->request());
    if (!writer.beginResponse()) {
        return ESP_FAIL;
    }

    const bool includeDetails =
        request->hasParam("details") &&
        // Keep query-flag semantics aligned with other APIs that accept
        // human-friendly booleans. If `/api/system/tasks?details=...` regresses
        // while alarms still work, inspect RequestQueryUtils first.
        RequestQuery::isTruthyParam(request->getParam("details")->value());
    const SystemTasksSnapshot snapshot =
        buildSystemTasksSnapshot(_routeDeps.tasks, includeDetails);
    SystemSnapshotJsonWriter::writeTasks(writer, snapshot);
    writer.finish();
    return snapshot.allocationFailed ? ESP_FAIL : ESP_OK;
}

esp_err_t SystemApiService::handleWifiRecovery(PsychicRequest* request) {
    // Recovery is coordinated asynchronously by the owner service. "accepted"
    // means the request was queued or deemed unnecessary because WiFi is
    // already up; it does not imply an immediate reconnect within this call.
    const bool accepted =
        _routeDeps.network.requestWiFiRecovery &&
        _routeDeps.network.requestWiFiRecovery("api/manual");
    const bool connected = WiFi.isConnected();
    
    Utils::JsonResponseWriter writer(request->request());
    if (!writer.beginResponse()) return ESP_FAIL;
    
    writer.raw("{");
    writer.key("success"); writer.value(accepted || connected); writer.raw(",");
    writer.key("accepted"); writer.value(accepted); writer.raw(",");
    writer.key("connected"); writer.value(connected);
    
    if (connected) {
        IPAddress ip = WiFi.localIP();
        char ipBuf[16];
        snprintf(ipBuf, sizeof(ipBuf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        writer.raw(","); writer.key("ip"); writer.string(ipBuf);
        writer.raw(","); writer.key("rssi"); writer.value(WiFi.RSSI());
    }
    
    writer.raw("}");
    writer.finish();
    return ESP_OK;
}

esp_err_t SystemApiService::handleNetworkInfo(PsychicRequest* request) {
    const SystemNetworkSnapshot snapshot =
        buildSystemNetworkSnapshot(_routeDeps.network);
    if (!snapshot.available || !snapshot.wifi) {
        return Response::error(request, 500, "service/wifi_unavailable");
    }

    Utils::JsonResponseWriter writer(request->request());
    if (!writer.beginResponse()) {
        return ESP_FAIL;
    }

    const StateHandlerResult writeResult =
        SystemSnapshotJsonWriter::writeNetwork(writer, snapshot);
    if (!writeResult.ok) {
        return Response::error(
            request,
            writeResult.httpStatus,
            writeResult.errorCode ? writeResult.errorCode : "service/wifi_unavailable");
    }
    writer.finish();
    return ESP_OK;
}

}  // namespace API
