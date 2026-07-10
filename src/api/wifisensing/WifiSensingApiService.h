#pragma once

#include <PsychicHttpServer.h>
#include <core/HttpEndpoint.h>
#include <security/SecurityManager.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <memory>
#include <atomic>
#include "../BaseApiService.h"
#include "../common/JwtAuthenticator.h"
#include "../common/WsEndpointRuntime.h"
#include "../../wifisensing/csi/data/CsiTypes.h"
#include "../../system/rtc/types/RtcWifiSensingTypes.h"

// Forward declaration
namespace WIFISENSING {
class WifiSensingSettings;
class WifiSensingService;
namespace CSI {
class CsiService;
}
}

namespace API {

class WifiSensingApiService : public BaseApiService {
public:
    WifiSensingApiService(PsychicHttpServer* server, SecurityManager* securityManager, POWER::PowerManager* powerManager);
    ~WifiSensingApiService();
    
    void injectComponents(WIFISENSING::WifiSensingSettings* settings,
                          WIFISENSING::CSI::CsiService* csiService,
                          WIFISENSING::WifiSensingService* wifiSensingService);

    void begin() override;
    void cleanupClient(int fd) override;

private:
    void handleCsiFrontendStateChange(bool start);
    void handleCsiCaptureStateChange(bool start);
    esp_err_t handleCsiCaptureFrame(httpd_req_t* req, int fd);
    void handleCsiCaptureClientCleanup(int fd);
    bool startCsiCaptureSession(int fd);
    bool stopCsiCaptureSession(int fd);
    bool enqueueCsiCaptureEndLocked();
    void resetCsiCaptureCountersLocked();
    void releaseCsiCaptureSessionLocked();
    void sendCsiCaptureError(int fd, uint32_t sessionId, uint16_t errorCode);
    void offerCsiCaptureBatch(const WIFISENSING::CSI::CsiPacket* batch, size_t count);
    bool ensureCsiTransportReady();
    bool ensureCsiCaptureTransportReady();
    void teardownCsiTransport();
    void teardownCsiCaptureTransport();
    void cancelPendingCsiFrontendStop();
    void cancelPendingCsiCaptureStop();
    void scheduleCsiFrontendStop();
    void scheduleCsiCaptureStop();
    void runCsiFrontendStopWorker();
    void runCsiCaptureStopWorker();
    static void csiFrontendStopTask(void* context);
    static void csiCaptureStopTask(void* context);
    esp_err_t handleGetStatus(PsychicRequest* request);
    esp_err_t handlePostCsiCalibrate(PsychicRequest* request);

    WIFISENSING::WifiSensingSettings* _settings = nullptr;
    WIFISENSING::CSI::CsiService* _csiService = nullptr;
    WIFISENSING::WifiSensingService* _wifiSensingService = nullptr;
    std::unique_ptr<HttpEndpoint<RTC::WifiSensingData>> _configEndpoint;

    JwtAuthenticator _csiAuthenticator;
    WsEndpointRuntime _csiEndpoint;
    JwtAuthenticator _csiCaptureAuthenticator;
    WsEndpointRuntime _csiCaptureEndpoint;
    SemaphoreHandle_t _csiFrontendLifecycleMutex = nullptr;
    SemaphoreHandle_t _csiCaptureSessionMutex = nullptr;
    std::atomic<bool> _csiFrontendDesiredActive{false};
    std::atomic<bool> _csiFrontendStopTaskRunning{false};
    std::atomic<uint32_t> _csiFrontendStopDueMs{0};
    std::atomic<bool> _csiCaptureDesiredActive{false};
    std::atomic<bool> _csiCaptureStopTaskRunning{false};
    std::atomic<uint32_t> _csiCaptureStopDueMs{0};
    std::atomic<uint32_t> _csiCaptureLastPowerActivityMs{0};
    std::atomic<uint32_t> _csiCaptureSessionId{0};
    std::atomic<int> _csiCaptureClientFd{-1};
    std::atomic<bool> _csiCaptureStarting{false};
    std::atomic<bool> _csiCaptureAccepting{false};
    std::atomic<bool> _csiCaptureStopping{false};
    std::atomic<uint32_t> _csiCaptureStartExclusiveSequence{0};
    std::atomic<uint32_t> _csiCaptureStopInclusiveSequence{0};
    std::atomic<uint32_t> _csiCaptureMotionControlEpochStart{0};
    std::atomic<bool> _csiCaptureReplayOriginEnqueued{false};
    std::atomic<uint32_t> _csiCaptureFirstAcceptedSequence{0};
    std::atomic<uint32_t> _csiCaptureLastAcceptedSequence{0};
    std::atomic<uint32_t> _csiCaptureDataBatchesOffered{0};
    std::atomic<uint32_t> _csiCaptureDataBatchesEnqueued{0};
    std::atomic<uint32_t> _csiCaptureDataBatchesDropped{0};
    std::atomic<uint32_t> _csiCaptureRecordsOffered{0};
    std::atomic<uint32_t> _csiCaptureRecordsEnqueued{0};
    std::atomic<uint32_t> _csiCaptureRecordsDropped{0};
    std::atomic<uint32_t> _csiCaptureTruncatedRecords{0};
};

}  // namespace API
