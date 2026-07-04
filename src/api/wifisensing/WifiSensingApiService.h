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
    bool ensureCsiTransportReady();
    void teardownCsiTransport();
    void cancelPendingCsiFrontendStop();
    void scheduleCsiFrontendStop();
    void runCsiFrontendStopWorker();
    static void csiFrontendStopTask(void* context);
    esp_err_t handleGetStatus(PsychicRequest* request);
    esp_err_t handlePostCsiCalibrate(PsychicRequest* request);

    WIFISENSING::WifiSensingSettings* _settings = nullptr;
    WIFISENSING::CSI::CsiService* _csiService = nullptr;
    WIFISENSING::WifiSensingService* _wifiSensingService = nullptr;
    std::unique_ptr<HttpEndpoint<RTC::WifiSensingData>> _configEndpoint;

    JwtAuthenticator _csiAuthenticator;
    WsEndpointRuntime _csiEndpoint;
    SemaphoreHandle_t _csiFrontendLifecycleMutex = nullptr;
    std::atomic<bool> _csiFrontendDesiredActive{false};
    std::atomic<bool> _csiFrontendStopTaskRunning{false};
    std::atomic<uint32_t> _csiFrontendStopDueMs{0};
};

}  // namespace API
