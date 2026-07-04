#pragma once

#include <PsychicHttpServer.h>
#include <security/SecurityManager.h>
#include "../BaseApiService.h"
#include "SystemApiFacets.h"
#include <memory>

class WiFiSettingsService;
namespace POWER { class PowerManager; }
namespace ALARMS { class AlarmService; }
namespace WIFISENSING { class WifiSensingService; }
namespace BLE { class BleService; }
namespace SHELLY { class ShellyService; }
namespace MACROS { class MacroService; }
namespace AIRMOUSE { class AirMouseService; }
namespace TELEGRAM { class TelegramWorker; }

namespace API {

class SystemWebsocketBroadcaster;

class SystemApiService : public BaseApiService {
public:
    SystemApiService(
        PsychicHttpServer* server,
        SecurityManager* securityManager,
        POWER::PowerManager* powerManager,
        const SystemApiRouteDeps& routeDeps,
        const SystemApiBroadcastDeps& broadcastDeps);
    virtual ~SystemApiService();
    
    void sendShellyEvent(const void* devicePtr);
    void begin() override;
    void cleanupClient(int fd) override;

private:
    SystemApiRouteDeps _routeDeps;
    std::unique_ptr<SystemWebsocketBroadcaster> _broadcaster;

    esp_err_t handleTasks(PsychicRequest* request);
    esp_err_t handleWifiRecovery(PsychicRequest* request);
    esp_err_t handleSystemInfo(PsychicRequest* request);
    esp_err_t handleNetworkInfo(PsychicRequest* request);
};

}  // namespace API
