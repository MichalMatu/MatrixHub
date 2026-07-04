/**
 * @file ShellyApiService.h
 * @brief API endpoints for Shelly device management (facade)
 * 
 * Routes requests through facade methods for:
 * - device list / status reads
 * - add / update device
 * - remove device
 * - relay control
 */

#pragma once

#include <PsychicHttpServer.h>
#include <security/SecurityManager.h>
#include "../BaseApiService.h"
#include "../../shelly/ShellyService.h"

namespace API {

class ShellyApiService : public BaseApiService {
public:
    ShellyApiService(PsychicHttpServer* server, SecurityManager* security, POWER::PowerManager* powerManager, SHELLY::ShellyService* service);
    ~ShellyApiService() = default;

    /**
     * Register API endpoints
     */
    void begin() override;

private:
    SHELLY::ShellyService* _service;

    esp_err_t handleDeviceList(PsychicRequest* request);
    esp_err_t handleDeviceUpsert(PsychicRequest* request);
    esp_err_t handleDeviceDelete(PsychicRequest* request);
    esp_err_t handleRelayControl(PsychicRequest* request);
};

} // namespace API
