/**
 * @file BleApiService.h
 * @brief REST API for BLE module status and diagnostics
 */

#pragma once

#include <PsychicHttpServer.h>
#include <security/SecurityManager.h>
#include "../BaseApiService.h"

namespace BLE {
class BleSettingsService;
class BleService;
}

namespace API {

class BleApiService : public BaseApiService {
public:
    BleApiService(PsychicHttpServer* server, SecurityManager* securityManager, POWER::PowerManager* powerManager,
                  BLE::BleSettingsService* settings, BLE::BleService* service);
    
    void begin() override;

private:
    BLE::BleSettingsService* _settings = nullptr;
    BLE::BleService* _bleService = nullptr;

    // GET /api/ble/status - returns current BLE status
    esp_err_t handleGetStatus(PsychicRequest* request);
    
    // POST /api/ble/scan - start discovery mode
    esp_err_t handleStartScan(PsychicRequest* request);
    
    // DELETE /api/ble/scan - stop discovery mode
    esp_err_t handleStopScan(PsychicRequest* request);
};

}  // namespace API
