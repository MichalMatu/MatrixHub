/**
 * @file AlarmsApiService.h
 * @brief REST API endpoints for alarm rules management
 * 
 * Endpoints:
 *   GET  /api/alarms/rules  - Get all alarm rules
 *   POST /api/alarms/rules  - Replace all alarm rules
 */

#pragma once

#include <PsychicHttpServer.h>
#include <security/SecurityManager.h>
#include "../BaseApiService.h"
#include "../../system/rtc/types/RtcAlarmTypes.h"

namespace ALARMS {
class AlarmService;
class AlarmSettingsService;
}

namespace WIFISENSING {
class WifiSensingService;
}

namespace BLE { class BleService; }
namespace GPIO { class GpioService; }

namespace SYSTEM { 
    class HeapMonitor; 
}

namespace API {

class AlarmsApiService : public BaseApiService {
public:
    AlarmsApiService(PsychicHttpServer* server,
                     SecurityManager* securityManager,
                     POWER::PowerManager* powerManager,
                     ALARMS::AlarmService* service,
                     ALARMS::AlarmSettingsService* settings,
                     GPIO::GpioService* gpioService,
                     WIFISENSING::WifiSensingService* wifiSensing,
                     BLE::BleService* bleService,
                     SYSTEM::HeapMonitor* heapMonitor);

    /**
     * Register API endpoints
     */
    void begin() override;

private:
    ALARMS::AlarmService* _service; // Injected
    WIFISENSING::WifiSensingService* _wifiSensing; // Injected
    BLE::BleService* _bleService;
    GPIO::GpioService* _gpioService;
    SYSTEM::HeapMonitor* _heapMonitor;
    // Borrowed transactional rules service owned by ServiceRegistry. Keep
    // transport/serialization here, but keep save/apply/rollback behavior in
    // one shared settings object so GET/POST cannot diverge from runtime state.
    ALARMS::AlarmSettingsService* _settings;

    esp_err_t handleGetRules(PsychicRequest* request);
    esp_err_t handleWriteRules(PsychicRequest* request);
};

}  // namespace API
