#include "WifiSensingApiService.h"
#include "CsiWireFormat.h"

#include <esp_http_server.h>

#include "../../wifisensing/WifiSensingService.h"
#include "../../wifisensing/WifiSensingSettings.h"
#include "../../wifisensing/csi/core/CsiService.h"
#include "../../config/System.h"
#include "../../config/json/WifiSensingConfigJson.h"
#include "core/config/ConfigManager.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../system/power/PowerManager.h"
#include "../../system/logging/Logging.h"
#include "../../system/utils/ScopeLock.h"
#include "../../system/utils/json/JsonResponseWriter.h"
#include <ArduinoJson.h>
#include <PsychicJson.h>
#include <utils/ResponseUtils.h>
#include <LittleFS.h>
#include <services/RestartService.h>
#include "../../system/errors/ErrorCodes.h"

#undef LOG_TAG
#define LOG_TAG "ApiWifiSensing"

namespace API {
namespace {
constexpr const char* kWifiSensingConfigPath = "/api/wifisensing/config";
constexpr const char* kCsiCalibratePath = "/api/wifisensing/csi/calibrate";
constexpr size_t kCsiWsQueueDepth = 4;
constexpr uint32_t kCsiWsQueueStackBytes = 4096;
} // namespace

// Constructor
WifiSensingApiService::WifiSensingApiService(PsychicHttpServer* server, SecurityManager* securityManager, POWER::PowerManager* powerManager)
    : BaseApiService(server, securityManager, powerManager, "api/wifisensing"),
      _csiAuthenticator(securityManager),
      _csiEndpoint(server, CONFIG::Keys::kWsCsi, CONFIG::Keys::kCsiBroadcastName, &_csiAuthenticator, [this](bool start) {
          handleCsiFrontendStateChange(start);
      }, 50) { // <-- 50ms send timeout specifically for real-time CSI data streaming
    _csiFrontendLifecycleMutex = xSemaphoreCreateMutex();
    if (!_csiFrontendLifecycleMutex) {
        LOGE("Failed to create CSI frontend lifecycle mutex");
    }
    _csiEndpoint.setRequestCallback([this]() {
        if (_powerManager) {
            _powerManager->notifyActivity("ws/csi");
        }
    });
}

WifiSensingApiService::~WifiSensingApiService() {
    cancelPendingCsiFrontendStop();
    const TickType_t waitStep = pdMS_TO_TICKS(10);
    TickType_t waited = 0;
    while (_csiFrontendStopTaskRunning.load(std::memory_order_acquire) &&
           waited < pdMS_TO_TICKS(1000)) {
        vTaskDelay(waitStep);
        waited += waitStep;
    }
    if (_csiService) {
        _csiService->setCsiCallback(nullptr);
    }
    teardownCsiTransport();
    if (_csiFrontendLifecycleMutex) {
        vSemaphoreDelete(_csiFrontendLifecycleMutex);
        _csiFrontendLifecycleMutex = nullptr;
    }
}

void WifiSensingApiService::injectComponents(WIFISENSING::WifiSensingSettings* settings,
                                  WIFISENSING::CSI::CsiService* csiService,
                                  WIFISENSING::WifiSensingService* wifiSensingService) {
    _settings = settings;
    _csiService = csiService;
    _wifiSensingService = wifiSensingService;

    if (_settings) {
        _configEndpoint = std::make_unique<HttpEndpoint<RTC::WifiSensingData>>(
            WIFISENSING::WifiSensingSettings::readState,
            WIFISENSING::WifiSensingSettings::updateState,
            _settings,
            _server,
            kWifiSensingConfigPath,
            _securityManager,
            AuthenticationPredicates::IS_ADMIN,
            AuthenticationPredicates::IS_AUTHENTICATED,
            nullptr,
            [this]() {
                if (_powerManager) {
                    _powerManager->notifyActivity(_activityTag);
                }
            });
    } else {
        _configEndpoint.reset();
    }
}

void WifiSensingApiService::begin() {
    LOGI("WiFi Sensing API endpoints registered");
    _csiEndpoint.begin();

    // --- REST Registration ---
    _server->on("/api/wifisensing/status", HTTP_GET,
                wrapAuth([this](PsychicRequest* request) {
                    return handleGetStatus(request);
                }));
    _server->on(kCsiCalibratePath, HTTP_POST,
                wrapAdmin([this](PsychicRequest* request) {
                    return handlePostCsiCalibrate(request);
                }));

    if (_configEndpoint) {
        _configEndpoint->begin();
    } else {
        LOGE("WiFi sensing config endpoint was not initialized");
    }

    // 4. Register CSI Callback
    if (_csiService) {
        _csiService->setCsiCallback([this](const WIFISENSING::CSI::CsiPacket* batch, size_t count) {
            if (!batch || count == 0) return;

            for (size_t offset = 0; offset < count;) {
                const size_t remaining = count - offset;
                const size_t batchCount =
                    (remaining > WIFISENSING::CSI::MAX_CSI_BATCH_PACKETS)
                        ? WIFISENSING::CSI::MAX_CSI_BATCH_PACKETS
                        : remaining;
                const auto* batchStart = batch + offset;
                const size_t reserveLen = API::CSI_WIRE::RECORD_MAX_BYTES * batchCount;

                // Keep this byte layout aligned with docs/engineering/integrations/csi.md
                // and interface/src/lib/features/wifisensing/csi/parseCsiFrame.ts.
                const bool delivered = _csiEndpoint.broadcaster().broadcastSerialized(
                    reserveLen,
                    [batchStart, batchCount](uint8_t* buffer, size_t capacity) -> size_t {
                        return API::CSI_WIRE::writeBatch(buffer, capacity, batchStart, batchCount);
                    });
                _csiService->recordBatchDelivery(batchCount, delivered);

                offset += batchCount;
            }
        });
    }

}

void WifiSensingApiService::handleCsiFrontendStateChange(bool start) {
    // CSI remains a dedicated data plane. Frontend is just one consumer; future
    // consumers (for example alarms) can keep the service alive independently.
    if (start) {
        _csiFrontendDesiredActive.store(true, std::memory_order_release);
        cancelPendingCsiFrontendStop();

        SYSTEM::ScopeLock lock(_csiFrontendLifecycleMutex, pdMS_TO_TICKS(250));
        if (!lock.isLocked()) {
            LOGW("CSI frontend start deferred: lifecycle lock busy");
            return;
        }

        if (!ensureCsiTransportReady()) {
            LOGE("Failed to prepare CSI WebSocket transport for frontend");
            return;
        }
        if (_csiService) {
            _csiService->setConsumerActive(WIFISENSING::CSI::CsiConsumer::Frontend, true);
            LOGI("CSI frontend consumer: ACTIVE");
        }
        return;
    }

    _csiFrontendDesiredActive.store(false, std::memory_order_release);
    scheduleCsiFrontendStop();
}

bool WifiSensingApiService::ensureCsiTransportReady() {
    // The queue is enabled lazily on first client so CSI does not keep its
    // transport machinery alive when no frontend is listening.
    if (!_csiEndpoint.broadcaster().isQueueEnabled()) {
        _csiEndpoint.broadcaster().enableQueue(
            kCsiWsQueueDepth,
            kCsiWsQueueStackBytes,
            API::CSI_WIRE::BATCH_MAX_BYTES);
        if (!_csiEndpoint.broadcaster().isQueueEnabled()) {
            LOGE("Failed to enable CSI WebSocket queue");
            return false;
        }
    }
    LOGI("CSI transport ready: max payload=%u bytes, queue depth=%u",
         static_cast<unsigned>(API::CSI_WIRE::BATCH_MAX_BYTES),
         static_cast<unsigned>(kCsiWsQueueDepth));
    return true;
}

void WifiSensingApiService::teardownCsiTransport() {
    // When the last client leaves, we intentionally drop the dedicated queue.
    // That keeps CSI cheap when the feature is idle and mirrors the previous
    // first-client/last-client semantics.
    _csiEndpoint.broadcaster().disableQueue();
}

void WifiSensingApiService::cancelPendingCsiFrontendStop() {
    _csiFrontendStopDueMs.store(0, std::memory_order_release);
}

void WifiSensingApiService::scheduleCsiFrontendStop() {
    const uint32_t due = millis() + SENSOR::WIFI_SENSING::CSI_FRONTEND_STOP_GRACE_MS;
    _csiFrontendStopDueMs.store(due, std::memory_order_release);

    bool expected = false;
    if (!_csiFrontendStopTaskRunning.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) {
        return;
    }

    const BaseType_t created = xTaskCreatePinnedToCore(
        csiFrontendStopTask,
        "csi_ws_stop",
        CONFIG::TASKS::STACK_CSI_FRONTEND_STOP,
        this,
        CONFIG::TASKS::PRIO_CSI_FRONTEND_STOP,
        nullptr,
        CONFIG::TASKS::CORE_CSI_FRONTEND_STOP);

    if (created != pdPASS) {
        _csiFrontendStopTaskRunning.store(false, std::memory_order_release);
        LOGW("CSI frontend stop worker creation failed; stopping synchronously");
        runCsiFrontendStopWorker();
    }
}

void WifiSensingApiService::runCsiFrontendStopWorker() {
    while (true) {
        const uint32_t due = _csiFrontendStopDueMs.load(std::memory_order_acquire);
        if (due == 0 || _csiFrontendDesiredActive.load(std::memory_order_acquire)) {
            _csiFrontendStopTaskRunning.store(false, std::memory_order_release);
            return;
        }

        const uint32_t now = millis();
        const int32_t remaining = static_cast<int32_t>(due - now);
        if (remaining <= 0) {
            break;
        }

        const uint32_t sleepMs = remaining > 100 ? 100 : static_cast<uint32_t>(remaining);
        vTaskDelay(pdMS_TO_TICKS(sleepMs));
    }

    SYSTEM::ScopeLock lock(_csiFrontendLifecycleMutex, pdMS_TO_TICKS(1000));
    if (!lock.isLocked()) {
        LOGW("CSI frontend stop skipped: lifecycle lock busy");
        _csiFrontendStopTaskRunning.store(false, std::memory_order_release);
        return;
    }

    if (_csiFrontendDesiredActive.load(std::memory_order_acquire) ||
        _csiEndpoint.broadcaster().hasClients()) {
        _csiFrontendStopTaskRunning.store(false, std::memory_order_release);
        return;
    }

    if (_csiService) {
        _csiService->setConsumerActive(WIFISENSING::CSI::CsiConsumer::Frontend, false);
        LOGI("CSI frontend consumer: INACTIVE");
    }
    teardownCsiTransport();
    _csiFrontendStopDueMs.store(0, std::memory_order_release);
    _csiFrontendStopTaskRunning.store(false, std::memory_order_release);
}

void WifiSensingApiService::csiFrontendStopTask(void* context) {
    auto* self = static_cast<WifiSensingApiService*>(context);
    if (self) {
        self->runCsiFrontendStopWorker();
    }
    vTaskDelete(nullptr);
}

void WifiSensingApiService::cleanupClient(int fd) {
    _csiEndpoint.cleanupClient(fd);
}

esp_err_t WifiSensingApiService::handleGetStatus(PsychicRequest* request) {
    RTC::WifiSensingData config{};
    RTC::withConfig([&](const RTC::ConfigStore& cfg) {
        config = cfg.wifiSensing;
    });

    WIFISENSING::RssiStats stats{};
    bool running = false;
    bool active = false;
    bool motionDetected = false;
    const char* connectedSsid = "";
    uint8_t connectedChannel = 0;

    if (_wifiSensingService) {
        running = _wifiSensingService->isRunning();
        active = _wifiSensingService->isActive();
        motionDetected = _wifiSensingService->isMotionDetected();
        connectedSsid = _wifiSensingService->getConnectedSSID();
        connectedChannel = _wifiSensingService->getConnectedChannel();
        stats = _wifiSensingService->getStats();
    }

    WIFISENSING::CSI::CsiMetricsSnapshot csi{};
    if (_csiService) {
        csi = _csiService->getMetricsSnapshot();
    }

    Utils::JsonResponseWriter writer(request->request());
    if (!writer.beginResponse()) {
        return ESP_FAIL;
    }

    writer.raw("{");
    writer.key("schema"); writer.string("wifisensing.status.v1"); writer.raw(",");
    writer.key("enabled"); writer.value(config.enabled); writer.raw(",");
    writer.key("running"); writer.value(running); writer.raw(",");
    writer.key("active"); writer.value(active); writer.raw(",");
    writer.key("connectedSSID"); writer.string(connectedSsid); writer.raw(",");
    writer.key("connectedChannel"); writer.value(static_cast<unsigned int>(connectedChannel)); writer.raw(",");
    writer.key("motionDetected"); writer.value(motionDetected); writer.raw(",");
    writer.key("variance_threshold"); writer.value(config.varianceThreshold, 2); writer.raw(",");
    writer.key("sample_interval_ms"); writer.value(static_cast<unsigned int>(config.sampleIntervalMs)); writer.raw(",");

    writer.key("stats"); writer.raw("{");
    writer.key("current"); writer.value(static_cast<int>(stats.current)); writer.raw(",");
    writer.key("filtered"); writer.value(static_cast<int>(stats.filtered)); writer.raw(",");
    writer.key("min"); writer.value(static_cast<int>(stats.min)); writer.raw(",");
    writer.key("max"); writer.value(static_cast<int>(stats.max)); writer.raw(",");
    writer.key("avg"); writer.value(stats.avg, 2); writer.raw(",");
    writer.key("variance"); writer.value(stats.variance, 2); writer.raw(",");
    writer.key("sampleCount"); writer.value(static_cast<unsigned int>(stats.sampleCount)); writer.raw(",");
    writer.key("windowMs"); writer.value(static_cast<unsigned long>(stats.windowMs));
    writer.raw("},");

    writer.key("csi"); writer.raw("{");
    writer.key("enabled"); writer.value(csi.enabled); writer.raw(",");
    writer.key("queue_allocated"); writer.value(csi.queueAllocated); writer.raw(",");
    writer.key("active_consumer_mask"); writer.value(static_cast<unsigned long>(csi.activeConsumerMask)); writer.raw(",");
    writer.key("consumer_count"); writer.value(static_cast<unsigned int>(csi.activeConsumerCount)); writer.raw(",");
    writer.key("frontend_consumer_active"); writer.value(csi.frontendConsumerActive); writer.raw(",");
    writer.key("alarm_consumer_active"); writer.value(csi.alarmConsumerActive); writer.raw(",");
    writer.key("boot_consumer_active"); writer.value(csi.bootConsumerActive); writer.raw(",");
    writer.key("matrix_visualization_consumer_active"); writer.value(csi.matrixVisualizationConsumerActive); writer.raw(",");
    writer.key("queue_depth"); writer.value(static_cast<unsigned long>(csi.queueDepth)); writer.raw(",");
    writer.key("queue_capacity"); writer.value(static_cast<unsigned long>(csi.queueCapacity)); writer.raw(",");
    writer.key("queue_drops_total"); writer.value(static_cast<unsigned long>(csi.queueDropsTotal)); writer.raw(",");
    writer.key("queue_drops_last_sec"); writer.value(static_cast<unsigned long>(csi.queueDropsLastSec)); writer.raw(",");
    writer.key("rx_frames_total"); writer.value(static_cast<unsigned long>(csi.rxFramesTotal)); writer.raw(",");
    writer.key("rx_accepted_total"); writer.value(static_cast<unsigned long>(csi.rxAcceptedTotal)); writer.raw(",");
    writer.key("rx_throttled_total"); writer.value(static_cast<unsigned long>(csi.rxThrottledTotal)); writer.raw(",");
    writer.key("queued_packets_total"); writer.value(static_cast<unsigned long>(csi.queuedPacketsTotal)); writer.raw(",");
    writer.key("dequeued_packets_total"); writer.value(static_cast<unsigned long>(csi.dequeuedPacketsTotal)); writer.raw(",");
    writer.key("packets_forwarded_total"); writer.value(static_cast<unsigned long>(csi.packetsForwardedTotal)); writer.raw(",");
    writer.key("batches_forwarded_total"); writer.value(static_cast<unsigned long>(csi.batchesForwardedTotal)); writer.raw(",");
    writer.key("batches_dropped_total"); writer.value(static_cast<unsigned long>(csi.batchesDroppedTotal)); writer.raw(",");
    writer.key("packets_per_sec"); writer.value(static_cast<unsigned long>(csi.packetsPerSec)); writer.raw(",");
    writer.key("batches_per_sec"); writer.value(static_cast<unsigned long>(csi.batchesPerSec)); writer.raw(",");
    writer.key("last_packet_ms"); writer.value(static_cast<unsigned long>(csi.lastPacketMs)); writer.raw(",");
    writer.key("last_batch_ms"); writer.value(static_cast<unsigned long>(csi.lastBatchMs)); writer.raw(",");
    writer.key("calibration_count"); writer.value(csi.calibrationCount); writer.raw(",");
    writer.key("calibration_target"); writer.value(csi.calibrationTarget); writer.raw(",");
    writer.key("calibration_state"); writer.string(csi.calibrationState); writer.raw(",");
    writer.key("motion"); writer.raw("{");
    writer.key("enabled"); writer.value(config.csiAlarmEnabled); writer.raw(",");
    writer.key("state"); writer.string(WIFISENSING::CSI::toString(csi.motion.state)); writer.raw(",");
    writer.key("baseline_ready"); writer.value(csi.motion.baselineReady); writer.raw(",");
    writer.key("detected"); writer.value(csi.motion.motion); writer.raw(",");
    writer.key("noisy"); writer.value(csi.motion.noisy); writer.raw(",");
    writer.key("needs_calibration"); writer.value(csi.motion.needsCalibration); writer.raw(",");
    writer.key("score"); writer.value(csi.motion.score, 2); writer.raw(",");
    writer.key("confidence"); writer.value(csi.motion.confidence, 2); writer.raw(",");
    writer.key("frames_seen"); writer.value(static_cast<unsigned long>(csi.motion.framesSeen)); writer.raw(",");
    writer.key("width"); writer.value(static_cast<unsigned int>(csi.motion.width)); writer.raw(",");
    writer.key("band_count"); writer.value(static_cast<unsigned int>(csi.motion.bandCount)); writer.raw(",");
    writer.key("selected_carriers"); writer.value(static_cast<unsigned int>(csi.motion.selectedCarrierCount)); writer.raw(",");
    writer.key("valid_carriers"); writer.value(static_cast<unsigned int>(csi.motion.validCarrierCount)); writer.raw(",");
    writer.key("last_reset_reason"); writer.string(WIFISENSING::CSI::toString(csi.motion.lastResetReason));
    writer.raw("},");
    writer.key("ws_client_count"); writer.value(static_cast<unsigned long>(_csiEndpoint.broadcaster().getClientCount())); writer.raw(",");
    writer.key("ws_queue_enabled"); writer.value(_csiEndpoint.broadcaster().isQueueEnabled());
    writer.raw("}");

    writer.raw("}");
    writer.finish();
    return ESP_OK;
}

esp_err_t WifiSensingApiService::handlePostCsiCalibrate(PsychicRequest* request) {
    if (!_csiService) {
        return Response::success(request, [](JsonVariant& root) {
            root["ok"] = false;
            root["error"] = "csi_service_unavailable";
        });
    }

    _csiService->requestMotionCalibration();
    return Response::success(request, [](JsonVariant& root) {
        root["ok"] = true;
        root["state"] = "calibrating";
    });
}

}  // namespace API
