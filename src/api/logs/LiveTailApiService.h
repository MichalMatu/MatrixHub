#pragma once

#include <PsychicHttpServer.h>
#include <security/SecurityManager.h>
#include "../BaseApiService.h"

namespace API {

/**
 * @brief Routing facade for runtime live tail endpoints backed by LogRingBuffer.
 *
 * Registers:
 * - GET /rest/logs/tail
 * - DELETE /rest/logs/tail
 */
class LiveTailApiService : public BaseApiService {
public:
    LiveTailApiService(PsychicHttpServer* server, SecurityManager* securityManager, POWER::PowerManager* powerManager);

    void begin() override;
};

}  // namespace API
