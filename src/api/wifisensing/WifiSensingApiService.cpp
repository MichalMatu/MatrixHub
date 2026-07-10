#include "WifiSensingApiService.h"
#include "CsiCaptureWireFormat.h"
#include "CsiCaptureSequenceWindow.h"
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
#include "../../system/utils/Random.h"
#include "../../system/utils/json/JsonResponseWriter.h"
#include <ArduinoJson.h>
#include <PsychicJson.h>
#include <utils/ResponseUtils.h>
#include <LittleFS.h>
#include <services/RestartService.h>
#include "../../system/errors/ErrorCodes.h"
#include <cstring>

#undef LOG_TAG
#define LOG_TAG "ApiWifiSensing"

namespace API {
namespace {
constexpr const char* kWifiSensingConfigPath = "/api/wifisensing/config";
constexpr const char* kCsiCalibratePath = "/api/wifisensing/csi/calibrate";
constexpr size_t kCsiWsQueueDepth = 4;
constexpr uint32_t kCsiWsQueueStackBytes = 4096;
constexpr size_t kCsiCaptureWsQueueDepth = 8;
constexpr uint32_t kCsiCaptureWsQueueStackBytes = 4096;
constexpr uint32_t kCsiCapturePowerKeepAliveMs = 30000;
} // namespace

// Constructor
WifiSensingApiService::WifiSensingApiService(PsychicHttpServer* server, SecurityManager* securityManager, POWER::PowerManager* powerManager)
    : BaseApiService(server, securityManager, powerManager, "api/wifisensing"),
      _csiAuthenticator(securityManager),
      _csiEndpoint(server, CONFIG::Keys::kWsCsi, CONFIG::Keys::kCsiBroadcastName, &_csiAuthenticator, [this](bool start) {
          handleCsiFrontendStateChange(start);
      }, 50), // <-- 50ms send timeout specifically for real-time CSI data streaming
      _csiCaptureAuthenticator(securityManager, AuthenticationPredicates::IS_ADMIN),
      _csiCaptureEndpoint(server,
                          CONFIG::Keys::kWsCsiCaptureV1,
                          CONFIG::Keys::kCsiCaptureBroadcastName,
                          &_csiCaptureAuthenticator,
                          nullptr,
                          100) {
    _csiFrontendLifecycleMutex = xSemaphoreCreateMutex();
    if (!_csiFrontendLifecycleMutex) {
        LOGE("Failed to create CSI frontend lifecycle mutex");
    }
    _csiCaptureSessionMutex = xSemaphoreCreateMutex();
    if (!_csiCaptureSessionMutex) {
        LOGE("Failed to create CSI capture session mutex");
    }
    _csiEndpoint.setRequestCallback([this]() {
        if (_powerManager) {
            _powerManager->notifyActivity("ws/csi");
        }
    });
    _csiCaptureEndpoint.setRequestCallback([this]() {
        if (_powerManager) {
            _powerManager->notifyActivity("ws/csi-capture");
        }
    });
    _csiCaptureEndpoint.setFrameHandler([this](httpd_req_t* req, int fd) {
        return handleCsiCaptureFrame(req, fd);
    });
    _csiCaptureEndpoint.setCleanupCallback([this](int fd) {
        handleCsiCaptureClientCleanup(fd);
    });
}

WifiSensingApiService::~WifiSensingApiService() {
    cancelPendingCsiFrontendStop();
    cancelPendingCsiCaptureStop();
    const TickType_t waitStep = pdMS_TO_TICKS(10);
    TickType_t waited = 0;
    while ((_csiFrontendStopTaskRunning.load(std::memory_order_acquire) ||
            _csiCaptureStopTaskRunning.load(std::memory_order_acquire)) &&
           waited < pdMS_TO_TICKS(1000)) {
        vTaskDelay(waitStep);
        waited += waitStep;
    }
    if (_csiService) {
        _csiService->setCsiCallback(nullptr);
    }
    teardownCsiTransport();
    teardownCsiCaptureTransport();
    if (_csiFrontendLifecycleMutex) {
        vSemaphoreDelete(_csiFrontendLifecycleMutex);
        _csiFrontendLifecycleMutex = nullptr;
    }
    if (_csiCaptureSessionMutex) {
        vSemaphoreDelete(_csiCaptureSessionMutex);
        _csiCaptureSessionMutex = nullptr;
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
    _csiCaptureEndpoint.begin();

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

                offerCsiCaptureBatch(batchStart, batchCount);

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

esp_err_t WifiSensingApiService::handleCsiCaptureFrame(httpd_req_t* req, int fd) {
    if (!req || fd < 0) {
        return ESP_FAIL;
    }

    httpd_ws_frame_t frame{};
    frame.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t result = httpd_ws_recv_frame(req, &frame, 0);
    if (result != ESP_OK) {
        return result;
    }

    constexpr size_t kMaxCommandBytes = 16;
    if (frame.len == 0) {
        return ESP_OK;
    }
    if (frame.len > kMaxCommandBytes) {
        LOGW("Closing CSI capture fd %d after oversized command (%u bytes)",
             fd,
             static_cast<unsigned>(frame.len));
        _csiCaptureEndpoint.broadcaster().removeClient(fd, true);
        handleCsiCaptureClientCleanup(fd);
        return ESP_OK;
    }

    char command[kMaxCommandBytes + 1] = {};
    frame.payload = reinterpret_cast<uint8_t*>(command);
    result = httpd_ws_recv_frame(req, &frame, frame.len);
    if (result != ESP_OK) {
        return result;
    }
    command[frame.len] = '\0';

    if (!_csiCaptureEndpoint.broadcaster().markClientReady(fd)) {
        LOGW("Closing CSI capture fd %d: client readiness update failed", fd);
        _csiCaptureEndpoint.broadcaster().removeClient(fd, true);
        handleCsiCaptureClientCleanup(fd);
        return ESP_OK;
    }
    if (frame.type != HTTPD_WS_TYPE_TEXT) {
        sendCsiCaptureError(
            fd,
            _csiCaptureSessionId.load(std::memory_order_acquire),
            static_cast<uint16_t>(API::CSI_CAPTURE_WIRE::ErrorCode::UnsupportedCommand));
        return ESP_OK;
    }

    if (frame.len == 5 && memcmp(command, "START", 5) == 0) {
        (void)startCsiCaptureSession(fd);
        return ESP_OK;
    }
    if (frame.len == 4 && memcmp(command, "STOP", 4) == 0) {
        (void)stopCsiCaptureSession(fd);
        return ESP_OK;
    }

    sendCsiCaptureError(
        fd,
        _csiCaptureSessionId.load(std::memory_order_acquire),
        static_cast<uint16_t>(API::CSI_CAPTURE_WIRE::ErrorCode::UnsupportedCommand));
    return ESP_OK;
}

bool WifiSensingApiService::startCsiCaptureSession(int fd) {
    bool reserved = false;
    uint32_t sessionId = 0;
    API::CSI_CAPTURE_WIRE::ErrorCode error = API::CSI_CAPTURE_WIRE::ErrorCode::Busy;

    {
        SYSTEM::ScopeLock lock(_csiCaptureSessionMutex, pdMS_TO_TICKS(250));
        if (!lock.isLocked()) {
            sendCsiCaptureError(fd, 0, static_cast<uint16_t>(error));
            return false;
        }

        const int activeFd = _csiCaptureClientFd.load(std::memory_order_relaxed);
        if (activeFd >= 0) {
            error = activeFd == fd
                        ? API::CSI_CAPTURE_WIRE::ErrorCode::AlreadyStarted
                        : API::CSI_CAPTURE_WIRE::ErrorCode::Busy;
        } else {
            do {
                sessionId = UTILS::RNG::randomU32();
            } while (sessionId == 0);

            _csiCaptureSessionId.store(sessionId, std::memory_order_release);
            _csiCaptureClientFd.store(fd, std::memory_order_release);
            _csiCaptureStarting.store(true, std::memory_order_release);
            _csiCaptureAccepting.store(false, std::memory_order_release);
            _csiCaptureStopping.store(false, std::memory_order_release);
            resetCsiCaptureCountersLocked();
            reserved = true;
        }
    }

    if (!reserved) {
        sendCsiCaptureError(fd, sessionId, static_cast<uint16_t>(error));
        return false;
    }

    handleCsiCaptureStateChange(true);
    if (!_csiCaptureEndpoint.broadcaster().isQueueEnabled() ||
        !_csiService ||
        !_csiService->isConsumerActive(WIFISENSING::CSI::CsiConsumer::DiagnosticCapture)) {
        {
            SYSTEM::ScopeLock lock(_csiCaptureSessionMutex, pdMS_TO_TICKS(250));
            if (lock.isLocked() && _csiCaptureClientFd.load(std::memory_order_relaxed) == fd) {
                releaseCsiCaptureSessionLocked();
            }
        }
        sendCsiCaptureError(
            fd,
            sessionId,
            static_cast<uint16_t>(API::CSI_CAPTURE_WIRE::ErrorCode::TransportUnavailable));
        handleCsiCaptureStateChange(false);
        return false;
    }

    bool helloEnqueued = false;
    bool metricsValid = false;
    {
        // While STARTING is visible, the processing task enters this same lock
        // instead of discarding a just-accepted packet. HELLO is queued before
        // ACCEPTING becomes visible, creating a real FIFO source boundary.
        SYSTEM::ScopeLock lock(_csiCaptureSessionMutex, pdMS_TO_TICKS(500));
        if (lock.isLocked() &&
            _csiCaptureClientFd.load(std::memory_order_relaxed) == fd &&
            _csiCaptureSessionId.load(std::memory_order_relaxed) == sessionId &&
            _csiCaptureStarting.load(std::memory_order_relaxed)) {
            const auto metrics = _csiService->getMetricsSnapshot();
            metricsValid = metrics.queueMetricsValid;
            if (metricsValid) {
                API::CSI_CAPTURE_WIRE::HelloPayload hello;
                hello.startedAtMs = millis();
                hello.rxFramesStart = metrics.rxFramesTotal;
                hello.rxAcceptedStart = metrics.rxAcceptedTotal;
                hello.queuedPacketsStart = metrics.queuedPacketsTotal;
                hello.sourceQueueDropsStart = metrics.queueDropsTotal;
                hello.rxThrottleIntervalUs =
                    WIFISENSING::CSI::CsiService::CSI_RX_THROTTLE_INTERVAL_US;
                hello.motionControlEpoch = metrics.motionControlEpoch;

                _csiCaptureStartExclusiveSequence.store(
                    metrics.rxAcceptedTotal, std::memory_order_relaxed);
                _csiCaptureStopInclusiveSequence.store(
                    metrics.rxAcceptedTotal, std::memory_order_relaxed);
                _csiCaptureMotionControlEpochStart.store(
                    metrics.motionControlEpoch, std::memory_order_relaxed);

                int target = fd;
                helloEnqueued = _csiCaptureEndpoint.broadcaster().broadcastSerialized(
                    &target,
                    1,
                    API::CSI_CAPTURE_WIRE::HELLO_MESSAGE_BYTES,
                    [sessionId, hello](uint8_t* buffer, size_t capacity) -> size_t {
                        return API::CSI_CAPTURE_WIRE::writeHello(
                            buffer, capacity, sessionId, hello);
                    });
            }

            if (helloEnqueued) {
                _csiCaptureAccepting.store(true, std::memory_order_release);
                _csiCaptureStarting.store(false, std::memory_order_release);
            } else {
                releaseCsiCaptureSessionLocked();
            }
        }
    }

    if (!helloEnqueued) {
        if (!metricsValid) {
            LOGE("CSI capture START rejected: source queue metrics unavailable");
        } else {
            LOGE("Failed to enqueue CSI capture HELLO for fd %d", fd);
        }
        sendCsiCaptureError(
            fd,
            sessionId,
            static_cast<uint16_t>(API::CSI_CAPTURE_WIRE::ErrorCode::TransportUnavailable));
        handleCsiCaptureStateChange(false);
        return false;
    }

    LOGI("CSI capture session %lu started for fd %d",
         static_cast<unsigned long>(sessionId),
         fd);
    _csiCaptureLastPowerActivityMs.store(millis(), std::memory_order_relaxed);
    return true;
}

bool WifiSensingApiService::stopCsiCaptureSession(int fd) {
    bool endEnqueued = false;
    bool endAttempted = false;
    uint32_t sessionId = 0;

    {
        SYSTEM::ScopeLock lock(_csiCaptureSessionMutex, pdMS_TO_TICKS(500));
        if (!lock.isLocked() ||
            _csiCaptureClientFd.load(std::memory_order_relaxed) != fd ||
            !_csiCaptureAccepting.load(std::memory_order_relaxed) ||
            _csiCaptureStopping.load(std::memory_order_relaxed)) {
            sendCsiCaptureError(
                fd,
                _csiCaptureSessionId.load(std::memory_order_acquire),
                static_cast<uint16_t>(API::CSI_CAPTURE_WIRE::ErrorCode::NotStarted));
            return false;
        }

        sessionId = _csiCaptureSessionId.load(std::memory_order_relaxed);
        const auto metrics = _csiService
                                 ? _csiService->getMetricsSnapshot()
                                 : WIFISENSING::CSI::CsiMetricsSnapshot{};
        const uint32_t startExclusive =
            _csiCaptureStartExclusiveSequence.load(std::memory_order_relaxed);
        const uint32_t stopInclusive = metrics.queueMetricsValid
                                           ? metrics.rxAcceptedTotal
                                           : _csiCaptureLastAcceptedSequence.load(
                                                 std::memory_order_relaxed);
        _csiCaptureStopInclusiveSequence.store(stopInclusive, std::memory_order_relaxed);
        _csiCaptureStopping.store(true, std::memory_order_release);

        const uint32_t stopDistance =
            API::CSI_CAPTURE::distanceAfter(stopInclusive, startExclusive);
        const uint32_t recordsOffered =
            _csiCaptureRecordsOffered.load(std::memory_order_relaxed);
        const uint32_t lastDistance = API::CSI_CAPTURE::distanceAfter(
            _csiCaptureLastAcceptedSequence.load(std::memory_order_relaxed),
            startExclusive);
        const bool allFencedRecordsOffered =
            stopDistance == 0 ||
            (recordsOffered > 0 && lastDistance >= stopDistance &&
             lastDistance < API::CSI_CAPTURE::SEQUENCE_HALF_RANGE);

        if (!metrics.queueMetricsValid ||
            stopDistance >= API::CSI_CAPTURE::SEQUENCE_HALF_RANGE ||
            allFencedRecordsOffered) {
            endAttempted = true;
            endEnqueued = enqueueCsiCaptureEndLocked();
        }
    }

    if (endAttempted) {
        handleCsiCaptureStateChange(false);
    }
    if (endAttempted && !endEnqueued) {
        LOGE("CSI capture session %lu ended without queued END frame",
             static_cast<unsigned long>(sessionId));
        return false;
    }

    if (endAttempted) {
        LOGI("CSI capture session %lu stopped cleanly",
             static_cast<unsigned long>(sessionId));
    } else {
        LOGI("CSI capture session %lu STOP fenced; draining accepted source packets",
             static_cast<unsigned long>(sessionId));
    }
    return true;
}

void WifiSensingApiService::resetCsiCaptureCountersLocked() {
    _csiCaptureStartExclusiveSequence.store(0, std::memory_order_relaxed);
    _csiCaptureStopInclusiveSequence.store(0, std::memory_order_relaxed);
    _csiCaptureMotionControlEpochStart.store(0, std::memory_order_relaxed);
    _csiCaptureReplayOriginEnqueued.store(false, std::memory_order_relaxed);
    _csiCaptureFirstAcceptedSequence.store(0, std::memory_order_relaxed);
    _csiCaptureLastAcceptedSequence.store(0, std::memory_order_relaxed);
    _csiCaptureDataBatchesOffered.store(0, std::memory_order_relaxed);
    _csiCaptureDataBatchesEnqueued.store(0, std::memory_order_relaxed);
    _csiCaptureDataBatchesDropped.store(0, std::memory_order_relaxed);
    _csiCaptureRecordsOffered.store(0, std::memory_order_relaxed);
    _csiCaptureRecordsEnqueued.store(0, std::memory_order_relaxed);
    _csiCaptureRecordsDropped.store(0, std::memory_order_relaxed);
    _csiCaptureTruncatedRecords.store(0, std::memory_order_relaxed);
}

void WifiSensingApiService::releaseCsiCaptureSessionLocked() {
    _csiCaptureStarting.store(false, std::memory_order_release);
    _csiCaptureAccepting.store(false, std::memory_order_release);
    _csiCaptureStopping.store(false, std::memory_order_release);
    _csiCaptureClientFd.store(-1, std::memory_order_release);
    _csiCaptureDesiredActive.store(false, std::memory_order_release);
}

bool WifiSensingApiService::enqueueCsiCaptureEndLocked() {
    const int targetFd = _csiCaptureClientFd.load(std::memory_order_relaxed);
    const uint32_t sessionId = _csiCaptureSessionId.load(std::memory_order_relaxed);
    if (targetFd < 0 || sessionId == 0) {
        releaseCsiCaptureSessionLocked();
        return false;
    }

    const auto metrics = _csiService
                             ? _csiService->getMetricsSnapshot()
                             : WIFISENSING::CSI::CsiMetricsSnapshot{};
    API::CSI_CAPTURE_WIRE::EndPayload end;
    end.stoppedAtMs = millis();
    end.firstAcceptedSequence =
        _csiCaptureFirstAcceptedSequence.load(std::memory_order_relaxed);
    end.lastAcceptedSequence =
        _csiCaptureLastAcceptedSequence.load(std::memory_order_relaxed);
    end.recordsOffered = _csiCaptureRecordsOffered.load(std::memory_order_relaxed);
    end.recordsEnqueued = _csiCaptureRecordsEnqueued.load(std::memory_order_relaxed);
    end.recordsDropped = _csiCaptureRecordsDropped.load(std::memory_order_relaxed);
    end.dataBatchesOffered =
        _csiCaptureDataBatchesOffered.load(std::memory_order_relaxed);
    end.dataBatchesEnqueued =
        _csiCaptureDataBatchesEnqueued.load(std::memory_order_relaxed);
    end.dataBatchesDropped =
        _csiCaptureDataBatchesDropped.load(std::memory_order_relaxed);
    end.truncatedRecords =
        _csiCaptureTruncatedRecords.load(std::memory_order_relaxed);
    end.rxFramesEnd = metrics.rxFramesTotal;
    // This field is the inclusive STOP fence, not a later drain-time sample.
    // Keeping the boundary on the wire lets independent collectors prove the
    // complete (HELLO.rxAcceptedStart, END.rxAcceptedEnd] source window.
    end.rxAcceptedEnd =
        _csiCaptureStopInclusiveSequence.load(std::memory_order_relaxed);
    end.queuedPacketsEnd = metrics.queuedPacketsTotal;
    end.sourceQueueDropsEnd = metrics.queueDropsTotal;

    if (!metrics.queueMetricsValid) {
        end.sessionErrorFlags |= API::CSI_CAPTURE_WIRE::SOURCE_METRICS_UNAVAILABLE;
    }
    if (metrics.motionControlEpoch !=
        _csiCaptureMotionControlEpochStart.load(std::memory_order_relaxed)) {
        end.sessionErrorFlags |= API::CSI_CAPTURE_WIRE::MOTION_CONTROL_CHANGED;
    }
    if (!_csiCaptureReplayOriginEnqueued.load(std::memory_order_relaxed)) {
        end.sessionErrorFlags |= API::CSI_CAPTURE_WIRE::MISSING_REPLAY_ORIGIN;
    }

    const uint32_t startExclusive =
        _csiCaptureStartExclusiveSequence.load(std::memory_order_relaxed);
    const uint32_t stopInclusive =
        _csiCaptureStopInclusiveSequence.load(std::memory_order_relaxed);
    if (!API::CSI_CAPTURE::isCompleteWindow(
            startExclusive,
            stopInclusive,
            end.recordsOffered,
            end.firstAcceptedSequence,
            end.lastAcceptedSequence)) {
        end.sessionErrorFlags |= API::CSI_CAPTURE_WIRE::SOURCE_SEQUENCE_INCOMPLETE;
    }

    int target = targetFd;
    const bool enqueued = _csiCaptureEndpoint.broadcaster().broadcastSerialized(
        &target,
        1,
        API::CSI_CAPTURE_WIRE::END_MESSAGE_BYTES,
        [sessionId, end](uint8_t* buffer, size_t capacity) -> size_t {
            return API::CSI_CAPTURE_WIRE::writeEnd(buffer, capacity, sessionId, end);
        });
    releaseCsiCaptureSessionLocked();
    return enqueued;
}

void WifiSensingApiService::sendCsiCaptureError(int fd,
                                                uint32_t sessionId,
                                                uint16_t errorCode) {
    if (fd < 0) {
        return;
    }
    int target = fd;
    (void)_csiCaptureEndpoint.broadcaster().broadcastSerialized(
        &target,
        1,
        API::CSI_CAPTURE_WIRE::ERROR_MESSAGE_BYTES,
        [sessionId, errorCode](uint8_t* buffer, size_t capacity) -> size_t {
            return API::CSI_CAPTURE_WIRE::writeError(
                buffer,
                capacity,
                sessionId,
                static_cast<API::CSI_CAPTURE_WIRE::ErrorCode>(errorCode));
        });
}

void WifiSensingApiService::offerCsiCaptureBatch(
    const WIFISENSING::CSI::CsiPacket* batch,
    size_t count) {
    if (!batch || count == 0 ||
        (!_csiCaptureStarting.load(std::memory_order_acquire) &&
         !_csiCaptureAccepting.load(std::memory_order_acquire))) {
        return;
    }

    if (_powerManager) {
        const uint32_t now = millis();
        uint32_t previous =
            _csiCaptureLastPowerActivityMs.load(std::memory_order_relaxed);
        if (now - previous >= kCsiCapturePowerKeepAliveMs &&
            _csiCaptureLastPowerActivityMs.compare_exchange_strong(
                previous, now, std::memory_order_relaxed)) {
            _powerManager->notifyActivity("ws/csi-capture");
        }
    }

    bool sessionEnded = false;
    bool endEnqueued = false;
    uint32_t endedSessionId = 0;
    {
        SYSTEM::ScopeLock lock(_csiCaptureSessionMutex, pdMS_TO_TICKS(100));
        if (!lock.isLocked()) {
            if (_csiCaptureAccepting.load(std::memory_order_acquire)) {
                _csiCaptureDataBatchesOffered.fetch_add(1, std::memory_order_relaxed);
                _csiCaptureDataBatchesDropped.fetch_add(1, std::memory_order_relaxed);
                _csiCaptureRecordsOffered.fetch_add(
                    static_cast<uint32_t>(count), std::memory_order_relaxed);
                _csiCaptureRecordsDropped.fetch_add(
                    static_cast<uint32_t>(count), std::memory_order_relaxed);
            }
            return;
        }

        if (!_csiCaptureAccepting.load(std::memory_order_relaxed)) {
            return;
        }

        const int targetFd = _csiCaptureClientFd.load(std::memory_order_relaxed);
        const uint32_t sessionId = _csiCaptureSessionId.load(std::memory_order_relaxed);
        if (targetFd < 0 || sessionId == 0) {
            return;
        }

        const uint32_t startExclusive =
            _csiCaptureStartExclusiveSequence.load(std::memory_order_relaxed);
        const bool stopping = _csiCaptureStopping.load(std::memory_order_relaxed);
        const uint32_t stopInclusive =
            _csiCaptureStopInclusiveSequence.load(std::memory_order_relaxed);
        const auto slice = API::CSI_CAPTURE::selectBatchSlice(
            batch, count, startExclusive, stopping, stopInclusive);
        const auto* captureBatch = batch + slice.offset;
        const size_t captureCount = slice.count;

        if (captureCount == 0) {
            if (stopping && slice.reachedStopBoundary) {
                endedSessionId = sessionId;
                sessionEnded = true;
                endEnqueued = enqueueCsiCaptureEndLocked();
            }
            if (!sessionEnded) {
                return;
            }
        } else {
            _csiCaptureDataBatchesOffered.fetch_add(1, std::memory_order_relaxed);
            const uint32_t previousRecords = _csiCaptureRecordsOffered.fetch_add(
                static_cast<uint32_t>(captureCount), std::memory_order_relaxed);
            const bool replayOrigin = previousRecords == 0;

            uint32_t truncated = 0;
            for (size_t i = 0; i < captureCount; ++i) {
                const uint32_t sequence = captureBatch[i].acceptedSequence;
                if (previousRecords == 0 && i == 0) {
                    _csiCaptureFirstAcceptedSequence.store(
                        sequence, std::memory_order_relaxed);
                }
                _csiCaptureLastAcceptedSequence.store(
                    sequence, std::memory_order_relaxed);
                if (captureBatch[i].originalLen > captureBatch[i].len) {
                    ++truncated;
                }
            }
            if (truncated > 0) {
                _csiCaptureTruncatedRecords.fetch_add(
                    truncated, std::memory_order_relaxed);
            }

            int target = targetFd;
            const size_t reserveBytes =
                API::CSI_CAPTURE_WIRE::BATCH_HEADER_BYTES +
                (API::CSI_CAPTURE_WIRE::RECORD_MAX_BYTES * captureCount);
            const bool enqueued =
                _csiCaptureEndpoint.broadcaster().broadcastSerialized(
                    &target,
                    1,
                    reserveBytes,
                    [captureBatch, captureCount, sessionId, replayOrigin](
                        uint8_t* buffer, size_t capacity) -> size_t {
                        return API::CSI_CAPTURE_WIRE::writeBatch(
                            buffer,
                            capacity,
                            sessionId,
                            captureBatch,
                            captureCount,
                            replayOrigin);
                    });

            if (enqueued) {
                _csiCaptureDataBatchesEnqueued.fetch_add(
                    1, std::memory_order_relaxed);
                _csiCaptureRecordsEnqueued.fetch_add(
                    static_cast<uint32_t>(captureCount), std::memory_order_relaxed);
                if (replayOrigin) {
                    _csiCaptureReplayOriginEnqueued.store(
                        true, std::memory_order_relaxed);
                }
            } else {
                _csiCaptureDataBatchesDropped.fetch_add(
                    1, std::memory_order_relaxed);
                _csiCaptureRecordsDropped.fetch_add(
                    static_cast<uint32_t>(captureCount), std::memory_order_relaxed);
            }

            if (stopping && slice.reachedStopBoundary) {
                endedSessionId = sessionId;
                sessionEnded = true;
                endEnqueued = enqueueCsiCaptureEndLocked();
            }
        }
    }

    if (sessionEnded) {
        // Never disable the CSI consumer synchronously from this processing
        // callback; the deferred worker owns that lifecycle transition.
        handleCsiCaptureStateChange(false);
        if (!endEnqueued) {
            LOGE("CSI capture session %lu ended without queued END frame",
                 static_cast<unsigned long>(endedSessionId));
        } else {
            LOGI("CSI capture session %lu stopped at accepted-sequence fence",
                 static_cast<unsigned long>(endedSessionId));
        }
    }
}

void WifiSensingApiService::handleCsiCaptureClientCleanup(int fd) {
    bool wasActive = false;
    bool sessionAlreadyEnded = false;
    {
        SYSTEM::ScopeLock lock(_csiCaptureSessionMutex, pdMS_TO_TICKS(250));
        if (lock.isLocked()) {
            const int activeFd = _csiCaptureClientFd.load(std::memory_order_relaxed);
            if (activeFd == fd) {
                releaseCsiCaptureSessionLocked();
                wasActive = true;
            } else if (activeFd < 0 &&
                       _csiCaptureEndpoint.broadcaster().isQueueEnabled()) {
                sessionAlreadyEnded = true;
            }
        }
    }

    if (wasActive) {
        LOGW("CSI capture client %d disconnected without a clean END", fd);
        handleCsiCaptureStateChange(false);
    } else if (sessionAlreadyEnded) {
        scheduleCsiCaptureStop();
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

void WifiSensingApiService::handleCsiCaptureStateChange(bool start) {
    if (start) {
        _csiCaptureDesiredActive.store(true, std::memory_order_release);
        cancelPendingCsiCaptureStop();

        SYSTEM::ScopeLock lock(_csiFrontendLifecycleMutex, pdMS_TO_TICKS(250));
        if (!lock.isLocked()) {
            LOGW("CSI capture start deferred: lifecycle lock busy");
            return;
        }

        if (!ensureCsiCaptureTransportReady()) {
            LOGE("Failed to prepare CSI capture WebSocket transport");
            return;
        }
        if (_csiService) {
            _csiService->setConsumerActive(
                WIFISENSING::CSI::CsiConsumer::DiagnosticCapture, true);
            LOGI("CSI diagnostic capture consumer: ACTIVE");
        }
        return;
    }

    // STOP can be completed by the CSI processing task itself. Disabling the
    // last consumer synchronously there would wait on/delete the current task.
    // The dedicated stop worker performs the transition after the drain grace.
    _csiCaptureDesiredActive.store(false, std::memory_order_release);
    scheduleCsiCaptureStop();
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

bool WifiSensingApiService::ensureCsiCaptureTransportReady() {
    if (!_csiCaptureEndpoint.broadcaster().isQueueEnabled()) {
        _csiCaptureEndpoint.broadcaster().enableQueue(
            kCsiCaptureWsQueueDepth,
            kCsiCaptureWsQueueStackBytes,
            API::CSI_CAPTURE_WIRE::BATCH_MAX_BYTES);
        if (!_csiCaptureEndpoint.broadcaster().isQueueEnabled()) {
            LOGE("Failed to enable CSI capture WebSocket queue");
            return false;
        }
    }
    LOGI("CSI capture transport ready: max payload=%u bytes, queue depth=%u",
         static_cast<unsigned>(API::CSI_CAPTURE_WIRE::BATCH_MAX_BYTES),
         static_cast<unsigned>(kCsiCaptureWsQueueDepth));
    return true;
}

void WifiSensingApiService::teardownCsiTransport() {
    // When the last client leaves, we intentionally drop the dedicated queue.
    // That keeps CSI cheap when the feature is idle and mirrors the previous
    // first-client/last-client semantics.
    _csiEndpoint.broadcaster().disableQueue();
}

void WifiSensingApiService::teardownCsiCaptureTransport() {
    _csiCaptureEndpoint.broadcaster().disableQueue();
}

void WifiSensingApiService::cancelPendingCsiFrontendStop() {
    _csiFrontendStopDueMs.store(0, std::memory_order_release);
}

void WifiSensingApiService::cancelPendingCsiCaptureStop() {
    _csiCaptureStopDueMs.store(0, std::memory_order_release);
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

void WifiSensingApiService::scheduleCsiCaptureStop() {
    const uint32_t due = millis() + SENSOR::WIFI_SENSING::CSI_FRONTEND_STOP_GRACE_MS;
    _csiCaptureStopDueMs.store(due, std::memory_order_release);

    bool expected = false;
    if (!_csiCaptureStopTaskRunning.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) {
        return;
    }

    const BaseType_t created = xTaskCreatePinnedToCore(
        csiCaptureStopTask,
        "csi_cap_stop",
        CONFIG::TASKS::STACK_CSI_CAPTURE_STOP,
        this,
        CONFIG::TASKS::PRIO_CSI_CAPTURE_STOP,
        nullptr,
        CONFIG::TASKS::CORE_CSI_CAPTURE_STOP);

    if (created != pdPASS) {
        _csiCaptureStopTaskRunning.store(false, std::memory_order_release);
        if (_csiService && _csiService->isProcessingTaskContext()) {
            LOGE("CSI capture stop worker creation failed in processing task; "
                 "deferring cleanup until client disconnect");
            return;
        }
        LOGW("CSI capture stop worker creation failed; stopping synchronously");
        runCsiCaptureStopWorker();
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

void WifiSensingApiService::runCsiCaptureStopWorker() {
    while (true) {
        const uint32_t due = _csiCaptureStopDueMs.load(std::memory_order_acquire);
        if (due == 0 || _csiCaptureDesiredActive.load(std::memory_order_acquire)) {
            _csiCaptureStopTaskRunning.store(false, std::memory_order_release);
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
        LOGW("CSI capture stop skipped: lifecycle lock busy");
        _csiCaptureStopTaskRunning.store(false, std::memory_order_release);
        return;
    }

    if (_csiCaptureDesiredActive.load(std::memory_order_acquire) ||
        _csiCaptureStarting.load(std::memory_order_acquire) ||
        _csiCaptureAccepting.load(std::memory_order_acquire) ||
        _csiCaptureStopping.load(std::memory_order_acquire)) {
        _csiCaptureStopTaskRunning.store(false, std::memory_order_release);
        return;
    }

    if (_csiService) {
        _csiService->setConsumerActive(
            WIFISENSING::CSI::CsiConsumer::DiagnosticCapture, false);
    }
    if (!_csiCaptureEndpoint.broadcaster().hasClients()) {
        teardownCsiCaptureTransport();
    }
    _csiCaptureStopDueMs.store(0, std::memory_order_release);
    _csiCaptureStopTaskRunning.store(false, std::memory_order_release);
}

void WifiSensingApiService::csiFrontendStopTask(void* context) {
    auto* self = static_cast<WifiSensingApiService*>(context);
    if (self) {
        self->runCsiFrontendStopWorker();
    }
    vTaskDelete(nullptr);
}

void WifiSensingApiService::csiCaptureStopTask(void* context) {
    auto* self = static_cast<WifiSensingApiService*>(context);
    if (self) {
        self->runCsiCaptureStopWorker();
    }
    vTaskDelete(nullptr);
}

void WifiSensingApiService::cleanupClient(int fd) {
    _csiEndpoint.cleanupClient(fd);
    _csiCaptureEndpoint.cleanupClient(fd);
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
    writer.key("queue_metrics_valid"); writer.value(csi.queueMetricsValid); writer.raw(",");
    writer.key("active_consumer_mask"); writer.value(static_cast<unsigned long>(csi.activeConsumerMask)); writer.raw(",");
    writer.key("consumer_count"); writer.value(static_cast<unsigned int>(csi.activeConsumerCount)); writer.raw(",");
    writer.key("frontend_consumer_active"); writer.value(csi.frontendConsumerActive); writer.raw(",");
    writer.key("alarm_consumer_active"); writer.value(csi.alarmConsumerActive); writer.raw(",");
    writer.key("boot_consumer_active"); writer.value(csi.bootConsumerActive); writer.raw(",");
    writer.key("matrix_visualization_consumer_active"); writer.value(csi.matrixVisualizationConsumerActive); writer.raw(",");
    writer.key("diagnostic_capture_consumer_active"); writer.value(csi.diagnosticCaptureConsumerActive); writer.raw(",");
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
    writer.key("motion_control_epoch"); writer.value(static_cast<unsigned long>(csi.motionControlEpoch)); writer.raw(",");
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
    writer.key("ws_queue_enabled"); writer.value(_csiEndpoint.broadcaster().isQueueEnabled()); writer.raw(",");
    writer.key("capture"); writer.raw("{");
    writer.key("client_count"); writer.value(static_cast<unsigned long>(_csiCaptureEndpoint.broadcaster().getClientCount())); writer.raw(",");
    writer.key("queue_enabled"); writer.value(_csiCaptureEndpoint.broadcaster().isQueueEnabled()); writer.raw(",");
    writer.key("starting"); writer.value(_csiCaptureStarting.load(std::memory_order_relaxed)); writer.raw(",");
    writer.key("accepting"); writer.value(_csiCaptureAccepting.load(std::memory_order_relaxed)); writer.raw(",");
    writer.key("stopping"); writer.value(_csiCaptureStopping.load(std::memory_order_relaxed)); writer.raw(",");
    writer.key("session_id"); writer.value(static_cast<unsigned long>(_csiCaptureSessionId.load(std::memory_order_relaxed))); writer.raw(",");
    writer.key("start_exclusive_sequence"); writer.value(static_cast<unsigned long>(_csiCaptureStartExclusiveSequence.load(std::memory_order_relaxed))); writer.raw(",");
    writer.key("stop_inclusive_sequence"); writer.value(static_cast<unsigned long>(_csiCaptureStopInclusiveSequence.load(std::memory_order_relaxed))); writer.raw(",");
    writer.key("records_offered"); writer.value(static_cast<unsigned long>(_csiCaptureRecordsOffered.load(std::memory_order_relaxed))); writer.raw(",");
    writer.key("records_enqueued"); writer.value(static_cast<unsigned long>(_csiCaptureRecordsEnqueued.load(std::memory_order_relaxed))); writer.raw(",");
    writer.key("records_dropped"); writer.value(static_cast<unsigned long>(_csiCaptureRecordsDropped.load(std::memory_order_relaxed))); writer.raw(",");
    writer.key("truncated_records"); writer.value(static_cast<unsigned long>(_csiCaptureTruncatedRecords.load(std::memory_order_relaxed)));
    writer.raw("}");
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
