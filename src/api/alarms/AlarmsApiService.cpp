/**
 * @file AlarmsApiService.cpp
 * @brief REST API routing for alarm rules (delegating facade)
 */

#include "AlarmsApiService.h"
#include "../../system/power/PowerManager.h"
#include "../../system/logging/Logging.h"

// Includes needed for endpoints
#include <vector>
#include "../../system/memory/PsramAllocator.h"
#include "utils/AlarmRulesSerializer.h"
#include "../common/RequestQueryUtils.h"
#include "../../alarms/types/AlarmTypes.h"
#include "../../alarms/AlarmService.h"
#include "../../alarms/AlarmSettingsService.h"
#include "../../alarms/engine/AlarmEvaluator.h"
#include "../../alarms/utils/BleDataProvider.h"
#include "../../gpio/GpioService.h"
#include "../../sensors/runtime/SensorState.h"
#include "../../wifisensing/WifiSensingService.h"
#include "../../system/health/heap/HeapMonitor.h"
#include "../../system/memory/SystemAllocator.h"
#include "../../system/utils/json/JsonResponseWriter.h"
#include "../../system/utils/ScopeLock.h"
#include "../../config/json/AlarmConfigJson.h"
#include "../../config/App.h"
#include <core/HttpEndpoint.h>
#include <utils/ResponseUtils.h>
#include <ArduinoJson.h>
#include <PsychicJson.h>
#include <limits>
#include <algorithm>
#include <cmath>

#undef LOG_TAG
#define LOG_TAG "AlarmsAPI"

namespace {
    constexpr const char* kAlarmRulesPath = "/api/alarms/rules";
    constexpr size_t kMaxAlarmRulesPayloadBytes = LIMITS::API::JSON_DOC::ALARMS_RULES;

    // Heap monitoring thresholds (bytes)
    constexpr int32_t kHeapWarnDeltaThreshold = 6000;
    constexpr int32_t kHeapLargestWarnThreshold = -4096;
    constexpr int32_t kHeapProbeThreshold = -2048;
}  // namespace

namespace API {

AlarmsApiService::AlarmsApiService(PsychicHttpServer* server,
                                   SecurityManager* securityManager,
                                   POWER::PowerManager* powerManager,
                                   ALARMS::AlarmService* service,
                                   ALARMS::AlarmSettingsService* settings,
                                   GPIO::GpioService* gpioService,
                                   WIFISENSING::WifiSensingService* wifiSensing,
                                   BLE::BleService* bleService,
                                   SYSTEM::HeapMonitor* heapMonitor)
    : BaseApiService(server, securityManager, powerManager, "api/alarms")
    , _service(service)
    , _wifiSensing(wifiSensing) 
    , _bleService(bleService)
    , _gpioService(gpioService)
    , _heapMonitor(heapMonitor)
    // Registry owns the transactional alarm rules state; this API keeps the
    // custom GET/POST/status serialization around that shared settings object.
    , _settings(settings) {}

void AlarmsApiService::begin() {
    _server->on(kAlarmRulesPath, HTTP_GET,
        wrapAuth([this](PsychicRequest* request) { 
            return this->handleGetRules(request);
        })
    );

    _server->on(
        kAlarmRulesPath,
        HTTP_POST,
        // Route alarm writes through the BaseApiService admin wrapper so auth,
        // PowerManager activity tracking and future admin-only behavior stay
        // consistent with the rest of the API surface. If POST requests stop
        // waking the device or start bypassing admin checks, inspect wrapAdmin.
        wrapAdmin([this](PsychicRequest* request) {
            return this->handleWriteRules(request);
        }));

    LOGI("API endpoints registered");
}

esp_err_t AlarmsApiService::handleGetRules(PsychicRequest* request) {
    uint32_t heapBefore = ESP.getFreeHeap();
    uint32_t largestBefore = ESP.getMaxAllocHeap();
    
    const bool includeStatus = request->hasParam("includeStatus") &&
        // Reuse the shared query parser so `includeStatus=1/true/yes` behaves
        // exactly like similar flags elsewhere. A regression here should be
        // compared against RequestQueryUtils before touching this endpoint.
        RequestQuery::isTruthyParam(request->getParam("includeStatus")->value());

    auto& manager = _service->getManager();

    // ── Single atomic snapshot under one lock ──────────────────────────
    std::vector<ALARMS::AlarmRule, SYSTEM::PsramAllocator<ALARMS::AlarmRule>> rulesCopy;
    std::vector<ALARMS::AlarmRuntimeState, SYSTEM::PsramAllocator<ALARMS::AlarmRuntimeState>> statesCopy;
    {
        SYSTEM::ScopeLock managerLock(manager.getMutex(), pdMS_TO_TICKS(50));
        if (!managerLock.isLocked()) {
            return Response::error(request, 503, "BUSY");
        }

        const ALARMS::AlarmRule* rules = manager.getRules();
        const uint8_t count = std::min(manager.getCount(), ALARMS::kMaxRules);
        rulesCopy.assign(rules, rules + count);

        if (includeStatus) {
            ALARMS::AlarmRuntimeState* states = manager.getStates();
            statesCopy.assign(states, states + count);
        }
    } // mutex released — engine is free

    // ── Build status data outside lock (BLE reads, sensor lookups) ─────
    std::vector<RuleStatus, SYSTEM::PsramAllocator<RuleStatus>> statuses;

    if (includeStatus && !statesCopy.empty()) {
        statuses.resize(rulesCopy.size());

        SensorSnapshot snap = SENSORS::SensorState::getSnapshot();
        auto wifiStats = _wifiSensing ? _wifiSensing->getStats() : WIFISENSING::RssiStats{};

        ALARMS::AlarmInputData inputData;
        inputData.co2 = static_cast<float>(snap.co2);
        inputData.temperature = snap.temp;
        inputData.humidity = snap.humid;
        inputData.wifiVariance = wifiStats.variance;
        inputData.imuTamper = _service ? _service->getLastImuTamperValue() : NAN;

        const uint32_t nowMs = millis();
        const size_t statusLimit = std::min(rulesCopy.size(), statesCopy.size());
        for (size_t i = 0; i < statusLimit; i++) {
            if (!rulesCopy[i].isValid()) continue;
            
            RuleStatus& item = statuses[i];
            item.triggered = statesCopy[i].previouslyTriggered;
            item.lastTriggered = statesCopy[i].lastTriggeredMs;
            if (rulesCopy[i].isBleSource()) {
                item.currentValue = ALARMS::getBleValue(
                    rulesCopy[i].source,
                    rulesCopy[i].bleDeviceMac,
                    nowMs,
                    _bleService
                );
            } else if (rulesCopy[i].isGpioSource()) {
                bool logicalValue = false;
                item.currentValue =
                    _gpioService && _gpioService->getLogicalValue(rulesCopy[i].gpioId, logicalValue)
                        ? (logicalValue ? 1.0f : 0.0f)
                        : NAN;
            } else {
                item.currentValue = ALARMS::AlarmEvaluator::getSensorValue(inputData, rulesCopy[i].source);
            }
            item.valid = true;
        }
    }

    // ── Streaming JSON response ───────────────────────────────────────
    httpd_req_t* req = request->request();
    Utils::JsonResponseWriter w(req);
    if (!w.beginResponse()) {
        return ESP_FAIL;
    }

    bool ok = AlarmRulesSerializer::serialize(
        w, 
        rulesCopy.data(), 
        static_cast<uint8_t>(rulesCopy.size()), 
        statuses.empty() ? nullptr : statuses.data(), 
        includeStatus
    );
    if (ok) {
        w.finish();
    }

    esp_err_t result = ok ? ESP_OK : ESP_FAIL;
    
    // ── Heap diagnostics ──────────────────────────────────────────────
    uint32_t heapAfter = ESP.getFreeHeap();
    uint32_t largestAfter = ESP.getMaxAllocHeap();
    int32_t deltaFree = static_cast<int32_t>(heapAfter) - static_cast<int32_t>(heapBefore);
    int32_t deltaLargest = static_cast<int32_t>(largestAfter) - static_cast<int32_t>(largestBefore);
    
    if (abs(deltaFree) > kHeapWarnDeltaThreshold || deltaLargest < kHeapLargestWarnThreshold) {
        LOGW("/api/alarms/rules: ΔFree=%+d ΔLargest=%+d (now: %u/%u)",
             deltaFree, deltaLargest, heapAfter, largestAfter);
    }

    if (deltaFree <= kHeapProbeThreshold || deltaLargest <= kHeapProbeThreshold) {
        static uint32_t lastProbeMs = 0;
        const uint32_t nowMs = millis();
        if (nowMs - lastProbeMs >= 30000) {
            if (_heapMonitor) {
                _heapMonitor->armDeferredProbe("alarms_rules", 250, heapAfter, largestAfter);
            }
            lastProbeMs = nowMs;
        }
    }
    
    return result;
}

esp_err_t AlarmsApiService::handleWriteRules(PsychicRequest* request) {
    // Alarm rules payloads can be large (up to 8 KB), so writes now go through
    // BaseApiService::parseJsonBody() to keep parsing on the common PSRAM-backed
    // path and to standardize `400 invalid_json` / `413 payload_too_large`.
    //
    // If alarm POSTs regress while other JSON APIs still work, compare
    // `kMaxAlarmRulesPayloadBytes` with BaseApiService::parseJsonBody() first.
    return parseJsonBody(
        request,
        kMaxAlarmRulesPayloadBytes,
        [this, request](JsonDocument& jsonDocument) -> esp_err_t {
            if (!jsonDocument.is<JsonObject>()) {
                return Response::invalidJson(request);
            }

            if (!_settings) {
                return Response::error(request, 500, "internal/update_failed");
            }

            JsonObject jsonObject = jsonDocument.as<JsonObject>();
            const StateHandlerResult validation =
                ALARMS::AlarmSettingsService::validateStateUpdate(jsonObject);
            if (!validation.ok) {
                return Response::error(
                    request,
                    validation.httpStatus,
                    validation.errorCode ? validation.errorCode : "internal/update_failed");
            }

            const StateTransactionResult txResult = _settings->updateAndPropagate(
                jsonObject,
                ALARMS::AlarmSettingsService::updateState,
                HTTP_ENDPOINT_ORIGIN_ID);
            if (txResult.outcome == StateUpdateResult::ERROR) {
                return Response::error(
                    request,
                    txResult.httpStatus,
                    txResult.errorCode ? txResult.errorCode : "internal/update_failed");
            }

            return this->handleGetRules(request);
        },
        kMaxAlarmRulesPayloadBytes);
}

}  // namespace API
