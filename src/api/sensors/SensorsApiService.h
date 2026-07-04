#pragma once

#include <PsychicHttpServer.h>
#include <security/SecurityManager.h>
#include "../BaseApiService.h"

namespace SYSTEM { class HeapMonitor; }

namespace API {

class SensorsApiService : public BaseApiService {
public:
    SensorsApiService(PsychicHttpServer* server, SecurityManager* securityManager, POWER::PowerManager* powerManager, SYSTEM::HeapMonitor* heapMonitor);
    void begin() override;

private:
    esp_err_t handleGetSensors(PsychicRequest* request);
    SYSTEM::HeapMonitor* _heapMonitor;
};

}  // namespace API
