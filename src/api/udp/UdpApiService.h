/**
 * @file UdpApiService.h
 * @brief API endpoint for UDP pusher settings
 * 
 * Provides GET/POST /api/udp for managing UDP data push to LAN servers:
 * - InfluxDB (line protocol)
 * - Telegraf
 * - Custom receivers
 */

#pragma once

#include <PsychicHttpServer.h>
#include <core/HttpEndpoint.h>
#include <security/SecurityManager.h>
#include <memory>
#include "../BaseApiService.h"
#include "../../system/rtc/types/RtcSystemTypes.h"

namespace UDPPUSH {
class UdpPusher;
class UdpSettingsService;
}

namespace API {

class UdpApiService : public BaseApiService {
public:
    UdpApiService(PsychicHttpServer* server,
                  SecurityManager* securityManager,
                  POWER::PowerManager* powerManager,
                  UDPPUSH::UdpPusher* udpPusher,
                  UDPPUSH::UdpSettingsService* settings);
    
    /**
     * @brief Register /api/udp GET/POST endpoints
     */
    void begin() override;

private:
    // Borrowed runtime worker owned by ServiceRegistry. The API test endpoint
    // must talk to the same instance that Phase 6 booted and the main loop
    // updates, otherwise live behavior and diagnostics would drift apart.
    UDPPUSH::UdpPusher* _udpPusher;
    // Borrowed RTC-backed config service owned by ServiceRegistry.
    UDPPUSH::UdpSettingsService* _settings;
    std::unique_ptr<HttpEndpoint<RTC::UdpPusherData>> _configEndpoint;

    StateHandlerResult validateConfigUpdate(PsychicRequest* request, JsonObject& jsonObject);
};

}  // namespace API
