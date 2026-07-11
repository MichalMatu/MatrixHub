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
#include "CsiApiShutdownGate.h"
#include "CsiCaptureCleanupGate.h"
#include "CsiDeferredWorkerGate.h"
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
    void prepareForSystemShutdown();
    void shutdown();
    void cleanupClient(int fd) override;
    void reconcileDeferredCsiLifecycle();

private:
    void fenceLifecycleWorkers();
    void handleCsiFrontendStateChange(bool start);
    bool handleCsiCaptureStateChange(bool start);
    esp_err_t handleCsiCaptureFrame(httpd_req_t* req, int fd);
    void handleCsiCaptureClientCleanup(int fd);
    bool startCsiCaptureSession(int fd);
    bool stopCsiCaptureSession(int fd);
    bool enqueueCsiCaptureEndLocked();
    void resetCsiCaptureCountersLocked();
    void releaseCsiCaptureSessionLocked();
    void releaseCsiCaptureSessionOrDefer(int fd,
                                         uint32_t sessionGeneration);
    void sendCsiCaptureError(int fd, uint32_t sessionId, uint16_t errorCode);
    void offerCsiCaptureBatch(const WIFISENSING::CSI::CsiPacket* batch, size_t count);
    bool ensureCsiTransportReady();
    bool ensureCsiCaptureTransportReady();
    bool teardownCsiTransport();
    bool teardownCsiCaptureTransport();
    void cancelPendingCsiFrontendStop();
    void cancelPendingCsiCaptureStop();
    void publishCsiFrontendSchedule(uint32_t dueMs);
    void publishCsiCaptureSchedule(uint32_t dueMs);
    bool readCsiFrontendSchedule(uint32_t generation, uint32_t& dueMs);
    bool readCsiCaptureSchedule(uint32_t generation, uint32_t& dueMs);
    bool readCsiCaptureCleanupRequest(CsiCaptureCleanupRequest& request);
    bool completeCsiCaptureCleanupRequest(uint32_t requestGeneration);
    void scheduleCsiFrontendReconcile(uint32_t dueMs);
    bool scheduleCsiCaptureCleanup(int fd, uint32_t sessionGeneration);
    void scheduleCsiFrontendStop();
    void scheduleCsiCaptureStop();
    void tryStartCsiFrontendLifecycleWorker();
    void tryStartCsiCaptureLifecycleWorker();
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
    std::atomic<bool> _shuttingDown{false};
    // This fence covers callbacks that explicitly acquire a Lease. The raw
    // user_ctx handlers owned by WsEndpointRuntime/HttpEndpoint cannot be made
    // destruction-safe locally: full request-lifetime fencing requires those
    // framework dispatchers to acquire a stable, post-destruction guard or to
    // unregister their routes before this object is released.
    CsiApiShutdownGate _shutdownGate;
    portMUX_TYPE _csiDeferredScheduleLock = portMUX_INITIALIZER_UNLOCKED;
    std::atomic<bool> _csiFrontendDesiredActive{false};
    CsiDeferredWorkerGate _csiFrontendWorkerGate;
    uint32_t _csiFrontendScheduleGeneration = 0;
    uint32_t _csiFrontendStopDueMs = 0;
    std::atomic<uint32_t> _csiFrontendWorkerSpawnRetryMs{0};
    std::atomic<bool> _csiCaptureDesiredActive{false};
    CsiDeferredWorkerGate _csiCaptureWorkerGate;
    uint32_t _csiCaptureScheduleGeneration = 0;
    uint32_t _csiCaptureStopDueMs = 0;
    CsiCaptureCleanupGate _csiCaptureCleanupGate;
    std::atomic<uint32_t> _csiCaptureWorkerSpawnRetryMs{0};
    std::atomic<uint32_t> _csiCaptureLastPowerActivityMs{0};
    std::atomic<uint32_t> _csiCaptureSessionId{0};
    std::atomic<uint32_t> _csiCaptureSessionGeneration{0};
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
