#pragma once

#include <PsychicHttpServer.h>
#include <core/HttpEndpoint.h>
#include <freertos/semphr.h>
#include <security/SecurityManager.h>
#include <memory>
#include "../BaseApiService.h"

#include "../../airmouse/AirMouseService.h"
#include "../../airmouse/AirMouseSettingsService.h"

namespace MACROS { class MacroService; }

namespace API {

/**
 * @class AirMouseApiService
 * @brief HTTP API for Air Mouse configuration and control
 * 
 * Endpoints:
 *   GET  /api/airmouse/status   - Get current status and settings
 *   POST /api/airmouse/config   - Update settings
 *   POST /api/airmouse/calibrate - Trigger gyro calibration
 */
class AirMouseApiService : public BaseApiService {
public:
    AirMouseApiService(PsychicHttpServer* server,
                       SecurityManager* securityManager,
                       POWER::PowerManager* powerManager,
                       AIRMOUSE::AirMouseService* service,
                       MACROS::MacroService* macroService,
                       AIRMOUSE::AirMouseSettingsService* settings);
    void begin() override;
    void cleanupClient(int fd) override;
    void setFsMutex(SemaphoreHandle_t fsMutex) { _fsMutex = fsMutex; }
    
private:
    AIRMOUSE::AirMouseService* _service; // Injected
    MACROS::MacroService* _macroService; // Injected
    // Ownership lives above the API layer so runtime config follows the same
    // registry-managed lifecycle pattern as other settings services.
    AIRMOUSE::AirMouseSettingsService* _settings;
    std::unique_ptr<HttpEndpoint<RTC::AirMouseData>> _configEndpoint;
    SemaphoreHandle_t _fsMutex = nullptr;
};

}  // namespace API
