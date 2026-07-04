#include "ServiceRegistryInitializationRuntime.h"
#include "ServiceRegistryOwnedApiRuntime.h"

#include "ServiceRegistryApi.h"
#include "../init/services/ApiServicesInitializer.h"

namespace SERVICE_REGISTRY_INIT_RUNTIME {

void initializeApiServices(
    ApiServices* api,
    const SharedApiServicesDeps& sharedDeps,
    const OwnedApiServicesDeps& ownedDeps) {
    if (!api) {
        return;
    }

    // Phase 1 keeps the broad API initializer path unchanged so transport
    // wiring still happens in one place. The refactor here is intentionally
    // structural only: all previous forwarded dependencies should map 1:1 into
    // the local initializer deps bundle below. If a non-macro/non-airmouse API
    // regresses, compare this mapping against ApiServicesInitializer::initialize().
    ApiServicesInitializerDeps initializerDeps;
    // Keep the mapping intentionally verbose. This makes dependency drift easy
    // to diff during debugging: each field shows exactly which registry-owned
    // service feeds the shared API boot path.
    initializerDeps.server = sharedDeps.server;
    initializerDeps.framework = sharedDeps.framework;
    initializerDeps.powerManager = sharedDeps.powerManager;
    initializerDeps.powerSettings = sharedDeps.powerSettings;
    initializerDeps.shellyService = sharedDeps.shellyService;
    initializerDeps.wifiSensingSettings = sharedDeps.wifiSensingSettings;
    initializerDeps.bleSettings = sharedDeps.bleSettings;
    initializerDeps.compensationSettings = sharedDeps.compensationSettings;
    initializerDeps.alarmService = sharedDeps.alarmService;
    initializerDeps.alarmSettings = sharedDeps.alarmSettings;
    initializerDeps.gpioService = sharedDeps.gpioService;
    initializerDeps.csiService = sharedDeps.csiService;
    initializerDeps.wifiSensingService = sharedDeps.wifiSensingService;
    initializerDeps.keyboardService = sharedDeps.keyboardService;
    initializerDeps.keyboardSettingsService = sharedDeps.keyboardSettingsService;
    initializerDeps.bleService = sharedDeps.bleService;
    initializerDeps.macroService = sharedDeps.macroService;
    initializerDeps.airMouseService = sharedDeps.airMouseService;
    initializerDeps.usbTerminalService = sharedDeps.usbTerminalService;
    initializerDeps.telegramWorker = sharedDeps.telegramWorker;
    initializerDeps.telegramTestSender = sharedDeps.telegramTestSender;
    initializerDeps.webhookTestSender = sharedDeps.webhookTestSender;
    initializerDeps.pushoverTestSender = sharedDeps.pushoverTestSender;
    initializerDeps.udpPusher = sharedDeps.udpPusher;
    initializerDeps.udpSettings = sharedDeps.udpSettings;
    initializerDeps.heartbeatSettings = sharedDeps.heartbeatSettings;
    initializerDeps.usbTerminalSettings = sharedDeps.usbTerminalSettings;
    initializerDeps.heapMonitor = sharedDeps.heapMonitor;
    initializerDeps.imuRuntimeService = sharedDeps.imuRuntimeService;
    initializerDeps.fsMutex = sharedDeps.fsMutex;
    ApiServicesInitializer::initialize(*api, initializerDeps);

    // Phase 2 makes registry-owned API boot explicit. These APIs need extra
    // lifecycle work (`begin`, and for AirMouse also `setFsMutex`) that is easy
    // to miss during refactors and worth testing in isolation. If only macro or
    // airmouse boot order regresses, debug this phase before Phase 1.
    initializeMacroApiService(api->macroApi,
                              sharedDeps.server,
                              sharedDeps.framework->getSecurityManager(),
                              sharedDeps.powerManager,
                              ownedDeps.macroService,
                              ownedDeps.macroSettings);

    initializeAirMouseApiService(api->airMouseApi,
                                 sharedDeps.server,
                                 sharedDeps.framework->getSecurityManager(),
                                 sharedDeps.powerManager,
                                 ownedDeps.airMouseService,
                                 ownedDeps.macroService,
                                 ownedDeps.airMouseSettings,
                                 sharedDeps.fsMutex);
}

}  // namespace SERVICE_REGISTRY_INIT_RUNTIME
