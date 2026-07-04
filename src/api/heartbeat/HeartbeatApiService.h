/**
 * @file HeartbeatApiService.h
 * @brief API endpoint for multi-slot heartbeat pinger settings
 * 
 * Provides GET/POST /api/heartbeat for managing dead man's switch services:
 * - Healthchecks.io, Uptime Kuma Push, Cronitor, Better Uptime, etc.
 */

#pragma once

#include <PsychicHttpServer.h>
#include <core/HttpEndpoint.h>
#include <security/SecurityManager.h>
#include <memory>
#include "../BaseApiService.h"
#include "../../system/rtc/types/RtcSystemTypes.h"
#include "../../system/health/heartbeat/HeartbeatSettingsService.h"

namespace API {

class HeartbeatApiService : public BaseApiService {
public:
    HeartbeatApiService(PsychicHttpServer* server,
                        SecurityManager* securityManager,
                        POWER::PowerManager* powerManager,
                        SYSTEM::HeartbeatSettingsService* settings);
    
    /**
     * @brief Register /api/heartbeat GET/POST endpoints
     */
    void begin() override;

private:
    SYSTEM::HeartbeatSettingsService* _settings = nullptr;
    std::unique_ptr<HttpEndpoint<RTC::HeartbeatData>> _configEndpoint;
};

}  // namespace API
