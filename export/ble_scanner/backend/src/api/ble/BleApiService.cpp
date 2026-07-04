/**
 * @file BleApiService.cpp
 * @brief REST API implementation for BLE scanner module
 */

#include "BleApiService.h"

#include "../../ble/BleLastSeenTime.h"
#include "../../ble/BleService.h"
#include "../../ble/settings/BleSettingsService.h"
#include "../../system/power/PowerManager.h"
#include "../../system/utils/json/JsonResponseWriter.h"
#include <utils/ResponseUtils.h>

#include <Arduino.h>
#include <time.h>

#undef LOG_TAG
#define LOG_TAG "ApiBle"

namespace API {

namespace {
constexpr uint32_t kDiscoveryTimeoutDefaultMs = 30000;
constexpr uint32_t kDiscoveryTimeoutMinMs = 1000;
constexpr uint32_t kDiscoveryTimeoutMaxMs = 300000;

bool isAdminRequest(SecurityManager* securityManager, PsychicRequest* request) {
    if (!securityManager || !request) {
        return false;
    }

    Authentication authentication = securityManager->authenticateRequest(request);
    return AuthenticationPredicates::IS_ADMIN(authentication);
}
} // namespace

BleApiService::BleApiService(PsychicHttpServer* server, SecurityManager* securityManager, POWER::PowerManager* powerManager,
                             BLE::BleSettingsService* settings, BLE::BleService* service)
    : BaseApiService(server, securityManager, powerManager, "api/ble"),
      _settings(settings),
      _bleService(service) {}

void BleApiService::begin() {
    _server->on("/api/ble/status", HTTP_GET,
        wrapAuth([this](PsychicRequest* request) {
            return handleGetStatus(request);
        })
    );

    _server->on("/api/ble/scan", HTTP_POST,
        wrapAdmin([this](PsychicRequest* request) {
            return handleStartScan(request);
        })
    );

    _server->on("/api/ble/scan", HTTP_DELETE,
        wrapAdmin([this](PsychicRequest* request) {
            return handleStopScan(request);
        })
    );

    LOGI("BLE API endpoints registered");
}

esp_err_t BleApiService::handleGetStatus(PsychicRequest* request) {
    if (!_bleService) return Response::error(request, 500, "service/unavailable");

    Utils::JsonResponseWriter w(request->request());
    if (!w.beginResponse()) return ESP_FAIL;

    const bool isAdmin = isAdminRequest(_securityManager, request);
    auto status = _bleService->getStatus();
    using namespace CONFIG::Keys;

    w.raw("{");
    w.key(kEnabled); w.value(_settings && _settings->isEnabled()); w.raw(",");
    w.key(kRunning); w.value(_bleService->isRunning()); w.raw(",");
    w.key(kScannerActive); w.value(status.scannerActive);
    w.raw(",");

    w.key(kDevices); w.raw("[");
    const uint32_t nowMs = millis();
    const time_t wallClockNow = time(nullptr);
    const uint64_t nowEpochMs = static_cast<uint64_t>(wallClockNow) * 1000ULL;
    // BLE readings themselves are relative to millis(); this gate only decides
    // whether we are allowed to project them onto real wall-clock time for API clients.
    const bool wallClockValid = BLE::LastSeenTime::isWallClockValid(wallClockNow);
    bool firstDevice = true;

    auto addDeviceToStream = [&](const char* mac, float temp, float humid, uint8_t batt, int8_t rssi, uint32_t lastSeenMs, bool saved) {
        if (!mac || mac[0] == '\0') return;

        if (!firstDevice) w.raw(",");
        firstDevice = false;

        w.raw("{");
        w.key(kMac); w.string(mac); w.raw(",");
        w.key(kTemp); w.value(temp, 1); w.raw(",");
        w.key(kHumid); w.value(humid, 0); w.raw(",");
        w.key(kBatt); w.value((int)batt); w.raw(",");
        w.key(kRssi); w.value((int)rssi); w.raw(",");

        // BLE cache stores relative millis(). If wall-clock time is not trusted yet
        // (AP/offline boot before NTP/manual sync), publish 0 instead of inventing a
        // fake epoch that the browser would render as a wildly stale reading.
        const uint64_t lastSeenEpochMs = BLE::LastSeenTime::toEpochMsOrZero(
            nowMs, lastSeenMs, nowEpochMs, wallClockValid);
        w.key(kLastSeen); w.value((unsigned long long)lastSeenEpochMs); w.raw(",");
        w.key(kSaved); w.value(saved);
        w.raw("}");
    };

    for (size_t slot = 0; slot < _bleService->getCachedDeviceSlots(); slot++) {
        const char* mac = nullptr;
        float temp = 0.0f; float humid = 0.0f; uint8_t batt = 0; int8_t rssi = 0; uint32_t lastSeenMs = 0;
        if (_bleService->getCachedDeviceDataAt(slot, mac, temp, humid, batt, rssi, lastSeenMs)) {
            addDeviceToStream(mac, temp, humid, batt, rssi, lastSeenMs, true);
        }
    }

    if (isAdmin) {
        for (size_t slot = 0; slot < _bleService->getDiscoverySlotCount(); slot++) {
            const char* mac = nullptr;
            float temp = 0.0f; float humid = 0.0f; uint8_t batt = 0; int8_t rssi = 0; uint32_t lastSeenMs = 0;
            if (_bleService->getDiscoveryEntryAt(slot, mac, temp, humid, batt, rssi, lastSeenMs)) {
                addDeviceToStream(mac, temp, humid, batt, rssi, lastSeenMs, false);
            }
        }
    }

    w.raw("]}");
    w.finish();
    return ESP_OK;
}

esp_err_t BleApiService::handleStartScan(PsychicRequest* request) {
    if (!_bleService) return Response::error(request, 500, "service/unavailable");

    uint32_t timeout = kDiscoveryTimeoutDefaultMs;

    if (request->hasParam("timeout")) {
        const long requestedTimeout = request->getParam("timeout")->value().toInt();
        if (requestedTimeout < static_cast<long>(kDiscoveryTimeoutMinMs) ||
            requestedTimeout > static_cast<long>(kDiscoveryTimeoutMaxMs)) {
            return Response::error(request, 400, "scan/invalid_timeout");
        }
        timeout = static_cast<uint32_t>(requestedTimeout);
    }

    if (!_bleService->startDiscovery(timeout)) {
        return Response::error(request, 503, "scan/start_failed");
    }

    Utils::JsonResponseWriter w(request->request());
    if (!w.beginResponse()) return ESP_FAIL;
    w.raw("{");
    w.key("ok"); w.value(true);
    w.raw(","); w.key("status"); w.string("scan_started");
    w.raw("}");
    w.finish();

    return ESP_OK;
}

esp_err_t BleApiService::handleStopScan(PsychicRequest* request) {
    if (!_bleService) return Response::error(request, 500, "service/unavailable");

    _bleService->stopDiscovery();

    Utils::JsonResponseWriter w(request->request());
    if (!w.beginResponse()) return ESP_FAIL;
    w.raw("{");
    w.key("ok"); w.value(true);
    w.raw(","); w.key("status"); w.string("scan_stopped");
    w.raw("}");
    w.finish();

    return ESP_OK;
}

}  // namespace API
