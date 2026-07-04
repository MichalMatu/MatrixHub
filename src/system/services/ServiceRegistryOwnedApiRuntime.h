#pragma once

#include "../utils/StaticService.h"
#include "../../api/airmouse/AirMouseApiService.h"
#include "../../api/macros/MacroApiService.h"

namespace SERVICE_REGISTRY_INIT_RUNTIME {

// These wrappers pin down the exact construction/begin sequence for APIs that
// are owned by the registry. Bugs here are typically order-of-operations bugs,
// so keeping the flow explicit makes later debugging much easier.

void initializeMacroApiService(
    SYSTEM::PsramStaticService<MACROS::MacroApiService>& macroApi,
    PsychicHttpServer* server,
    SecurityManager* securityManager,
    POWER::PowerManager* powerManager,
    MACROS::MacroService* macroService,
    MACROS::MacroSettingsService* macroSettings);

void initializeAirMouseApiService(
    SYSTEM::PsramStaticService<API::AirMouseApiService>& airMouseApi,
    PsychicHttpServer* server,
    SecurityManager* securityManager,
    POWER::PowerManager* powerManager,
    AIRMOUSE::AirMouseService* airMouseService,
    MACROS::MacroService* macroService,
    AIRMOUSE::AirMouseSettingsService* airMouseSettings,
    SemaphoreHandle_t fsMutex);

}  // namespace SERVICE_REGISTRY_INIT_RUNTIME
