#pragma once

#include "../../system/health/SystemHealth.h"

#include <cstddef>

namespace API {

struct SystemApiDependencies {
    SYSTEM::HEALTH::SystemDiagnostics (*getDiagnostics)();
    // Async request hook used by the admin recovery endpoint. The bool reports
    // request acceptance, not end-to-end reconnection success.
    bool (*requestWiFiRecovery)(const char* reason);
    bool (*isWatchdogInitialized)();
    uint32_t (*getWatchdogTimeoutSec)();
    size_t (*getFsTotalBytes)();
    size_t (*getFsUsedBytes)();
};

}  // namespace API
