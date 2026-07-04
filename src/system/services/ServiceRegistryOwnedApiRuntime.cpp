#include "ServiceRegistryOwnedApiRuntime.h"

namespace SERVICE_REGISTRY_INIT_RUNTIME {

void initializeMacroApiService(
    SYSTEM::PsramStaticService<MACROS::MacroApiService>& macroApi,
    PsychicHttpServer* server,
    SecurityManager* securityManager,
    POWER::PowerManager* powerManager,
    MACROS::MacroService* macroService,
    MACROS::MacroSettingsService* macroSettings) {
    // Construction and begin stay together here so tests catch any refactor
    // that wires the API but forgets to actually start it.
    auto* api = macroApi.init(server, securityManager, powerManager, macroService, macroSettings);
    if (api) {
        api->begin();
    }
}

void initializeAirMouseApiService(
    SYSTEM::PsramStaticService<API::AirMouseApiService>& airMouseApi,
    PsychicHttpServer* server,
    SecurityManager* securityManager,
    POWER::PowerManager* powerManager,
    AIRMOUSE::AirMouseService* airMouseService,
    MACROS::MacroService* macroService,
    AIRMOUSE::AirMouseSettingsService* airMouseSettings,
    SemaphoreHandle_t fsMutex) {
    // AirMouse must receive its FS mutex before begin(); keeping that ordering
    // boxed in one helper makes startup regressions much easier to spot.
    auto* api = airMouseApi.init(server,
                                 securityManager,
                                 powerManager,
                                 airMouseService,
                                 macroService,
                                 airMouseSettings);
    if (api) {
        api->setFsMutex(fsMutex);
        api->begin();
    }
}

}  // namespace SERVICE_REGISTRY_INIT_RUNTIME
