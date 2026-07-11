#include "ApiServicesInitializer.h"
#include "../../services/ServiceRegistryApi.h"
#include "../../../macros/MacroService.h"
#include "../../../keyboard/KeyboardService.h"
#include "../../../shelly/ShellyService.h"
#include "../../../system/health/heartbeat/HeartbeatSettingsService.h"
#include "../../../system/power/PowerSettingsService.h"
#include "../../../wifisensing/WifiSensingSettings.h"
#include "../../../ble/settings/BleSettingsService.h"
#include "../../../compensation/CompensationSettingsService.h"
#include "../../rtc/RtcConfig.h"
#include "../../../notifications/telegram/runtime/TelegramWorker.h"
#include "../../../udp/UdpSettingsService.h"
#include "../../../usb_terminal/UsbTerminalService.h"
#include "../../../usb_terminal/UsbTerminalSettingsService.h"

// File Manager
#include "api/filemanager/FileManagerApiService.h"

// System Dependencies
#include "../../../system/health/SystemHealth.h"
#include "../../../system/boot/BootTracker.h"
#include "../../../system/watchdog/TaskWatchdog.h"
#include "../../../system/logging/Logging.h"
#include <core/ESPFS.h>

namespace {

struct SystemApiInitBundles {
  // System API is the noisiest part of the initializer because it mixes plain
  // lambdas, route deps and websocket/broadcast deps. Keep those pieces
  // together here so regressions in `/api/system/*` can be debugged from one
  // bundle instead of chasing assignments across the whole initializer.
  API::SystemApiDependencies systemDeps;
  API::SystemApiRouteDeps routeDeps;
  API::SystemApiBroadcastDeps broadcastDeps;
};

API::DiagnosticsApiDeps buildDiagnosticsApiDeps(
    const ApiServicesInitializerDeps& deps,
    const API::SystemApiRouteDeps& routeDeps) {
  API::DiagnosticsApiDeps diagnosticsDeps;
  diagnosticsDeps.tasks = routeDeps.tasks;
  diagnosticsDeps.wifiSensingSettings = deps.wifiSensingSettings;
  diagnosticsDeps.wifiSensingService = deps.wifiSensingService;
  diagnosticsDeps.csiService = deps.csiService;
  diagnosticsDeps.bleSettings = deps.bleSettings;
  diagnosticsDeps.bleService = deps.bleService;
  diagnosticsDeps.alarmService = deps.alarmService;
  diagnosticsDeps.alarmSettings = deps.alarmSettings;
  diagnosticsDeps.keyboardService = deps.keyboardService;
  diagnosticsDeps.keyboardSettingsService = deps.keyboardSettingsService;
  diagnosticsDeps.macroService = deps.macroService;
  diagnosticsDeps.airMouseService = deps.airMouseService;
  diagnosticsDeps.usbTerminalService = deps.usbTerminalService;
  diagnosticsDeps.usbTerminalSettings = deps.usbTerminalSettings;
  diagnosticsDeps.udpPusher = deps.udpPusher;
  diagnosticsDeps.udpSettings = deps.udpSettings;
  diagnosticsDeps.heartbeatSettings = deps.heartbeatSettings;
  diagnosticsDeps.compensationSettings = deps.compensationSettings;
  return diagnosticsDeps;
}

API::SystemApiDependencies buildSystemApiDependencies() {
  // These callbacks intentionally stay close to the initializer rather than
  // being spread across the call site. If `/api/system/info|tasks|network`
  // changes behavior after a refactor, inspect this builder before the route
  // handlers themselves.
  API::SystemApiDependencies sysDeps;
  sysDeps.getDiagnostics = []() { return SYSTEM::SystemHealth::getDiagnostics(); };
  // Expose only the async/coordinated recovery path here. The old blocking
  // facade was removed so API wiring matches the runtime ownership model.
  sysDeps.requestWiFiRecovery = [](const char* reason) {
      return SYSTEM::SystemHealth::requestWiFiRecovery(reason);
  };
  sysDeps.isWatchdogInitialized = []() { return SYSTEM::TaskWatchdog::instance().isInitialized(); };
  sysDeps.getWatchdogTimeoutSec = []() { return SYSTEM::TaskWatchdog::instance().getTimeoutSec(); };
  sysDeps.getFsTotalBytes = []() { return ESPFS.totalBytes(); };
  sysDeps.getFsUsedBytes = []() { return ESPFS.usedBytes(); };
  return sysDeps;
}

SystemApiInitBundles buildSystemApiInitBundles(const ApiServicesInitializerDeps& deps) {
  SystemApiInitBundles bundles;
  bundles.systemDeps = buildSystemApiDependencies();

  auto* wifiSettingsService = deps.framework->getWiFiSettingsService();
  // Route deps back the synchronous HTTP handlers.
  bundles.routeDeps.info.getFsTotalBytes = bundles.systemDeps.getFsTotalBytes;
  bundles.routeDeps.info.getFsUsedBytes = bundles.systemDeps.getFsUsedBytes;
  bundles.routeDeps.tasks.isWatchdogInitialized = bundles.systemDeps.isWatchdogInitialized;
  bundles.routeDeps.tasks.getWatchdogTimeoutSec = bundles.systemDeps.getWatchdogTimeoutSec;
  bundles.routeDeps.network.wifiSettingsService = wifiSettingsService;
  bundles.routeDeps.network.requestWiFiRecovery = bundles.systemDeps.requestWiFiRecovery;

  // Broadcast deps feed the websocket/broadcast side of SystemApiService.
  // Keeping them adjacent to route deps makes it obvious whether a regression
  // belongs to request/response flow or live push flow.
  bundles.broadcastDeps.powerManager = deps.powerManager;
  bundles.broadcastDeps.alarmService = deps.alarmService;
  bundles.broadcastDeps.wifiSensingService = deps.wifiSensingService;
  bundles.broadcastDeps.bleService = deps.bleService;
  bundles.broadcastDeps.shellyService = deps.shellyService;
  bundles.broadcastDeps.macroService = deps.macroService;
  bundles.broadcastDeps.airMouseService = deps.airMouseService;
  bundles.broadcastDeps.telegramWorker = deps.telegramWorker;
  bundles.broadcastDeps.wifiSettingsService = wifiSettingsService;
  bundles.broadcastDeps.systemDeps = bundles.systemDeps;

  return bundles;
}

void initializeCoreApis(ApiServices& api,
                        SecurityManager* securityManager,
                        const ApiServicesInitializerDeps& deps) {
  // These APIs are mostly transport/config facades and do not depend on the
  // larger runtime graph. If boot order regresses for "basic" endpoints,
  // start debugging in this block before the runtime-heavy helpers below.
  api.powerApi.init(deps.server, securityManager, deps.powerManager, deps.powerSettings);
  api.powerApi->begin();

  api.logFilesApi.init(deps.server, securityManager, deps.powerManager, deps.heapMonitor);
  api.logFilesApi->setFsMutex(deps.fsMutex);
  api.logFilesApi->begin();

  api.liveTailApi.init(deps.server, securityManager, deps.powerManager);
  api.liveTailApi->begin();

  api.configApi.init(
      deps.server,
      securityManager,
      deps.powerManager,
      []() { return RTC::copyConfigSection(&RTC::ConfigStore::logging); },
      [](uint8_t level) { return LoggingConfig::setLevel(static_cast<esp_log_level_t>(level)); });
  api.configApi->begin();

  api.notificationsApi.init(
      deps.server,
      securityManager,
      deps.powerManager,
      deps.telegramTestSender,
      deps.webhookTestSender,
      deps.pushoverTestSender);
  api.notificationsApi->begin();
}

void initializeRuntimeApis(ApiServices& api,
                           SecurityManager* securityManager,
                           const ApiServicesInitializerDeps& deps) {
  // Runtime-heavy APIs depend on live services, snapshots and cross-service
  // broadcasters. Keeping them together makes it obvious where to look when
  // boot failures affect telemetry, alarms or device-state endpoints.
  api.wifiSensingApi.init(deps.server, securityManager, deps.powerManager);
  api.wifiSensingApi->injectComponents(
      deps.wifiSensingSettings, deps.csiService, deps.wifiSensingService);
  api.wifiSensingApi->begin();

  const SystemApiInitBundles systemBundles = buildSystemApiInitBundles(deps);
  // System API is initialized from a prebuilt bundle so the "what feeds the
  // HTTP routes" and "what feeds websocket broadcasts" split stays explicit.
  api.systemApi.init(
      deps.server,
      securityManager,
      deps.powerManager,
      systemBundles.routeDeps,
      systemBundles.broadcastDeps);
  api.systemApi->begin();

  api.diagnosticsApi.init(
      deps.server,
      securityManager,
      deps.powerManager,
      buildDiagnosticsApiDeps(deps, systemBundles.routeDeps));
  api.diagnosticsApi->begin();

  api.alarmsApi.init(
      deps.server,
      securityManager,
      deps.powerManager,
      deps.alarmService,
      deps.alarmSettings,
      deps.gpioService,
      deps.wifiSensingService,
      deps.bleService,
      deps.heapMonitor);
  api.alarmsApi->begin();

  api.shellyApi.init(deps.server, securityManager, deps.powerManager, deps.shellyService);
  api.shellyApi->begin();

  api.bleApi.init(
      deps.server, securityManager, deps.powerManager, deps.bleSettings, deps.bleService);
  api.bleApi->begin();

  api.imuApi.init(deps.server, securityManager, deps.powerManager, deps.imuRuntimeService);
  api.imuApi->begin();

  api.heartbeatApi.init(
      deps.server, securityManager, deps.powerManager, deps.heartbeatSettings);
  api.heartbeatApi->begin();

  api.gpioApi.init(deps.server, securityManager, deps.powerManager, deps.gpioService);
  api.gpioApi->begin();

  // Both the runtime worker and its RTC-backed settings are injected so the
  // API test endpoint cannot accidentally drift to a different instance.
  api.udpApi.init(deps.server, securityManager, deps.powerManager, deps.udpPusher, deps.udpSettings);
  api.udpApi->begin();
}

void initializeInteractionApis(ApiServices& api,
                               SecurityManager* securityManager,
                               const ApiServicesInitializerDeps& deps) {
  // User-interaction APIs sit here because they wire live input/control paths
  // rather than pure config or system status. If keyboard/USB behavior regresses
  // after init changes, compare this block before touching runtime services.
  api.keyboardApi.init(
      deps.server,
      securityManager,
      deps.powerManager,
      deps.keyboardService,
      deps.keyboardSettingsService);
  api.keyboardApi->begin();

  api.compensationApi.init(
      deps.server, securityManager, deps.powerManager, deps.compensationSettings);
  api.compensationApi->begin();

  if (deps.usbTerminalService) {
      auto* worker = deps.telegramWorker;
      // The callback is rebound during API boot so USB terminal forwarding
      // always targets the current shared Telegram worker. If terminal-originated
      // Telegram messages stop flowing after a refactor, inspect this bridge.
      deps.usbTerminalService->setTelegramMessageCallback(
          [worker](const char* chatId, const char* msg) {
              if (worker) {
                  worker->enqueue(chatId, msg);
              }
          });
  }

  api.usbTerminalApi.init(
      deps.server,
      securityManager,
      deps.powerManager,
      deps.usbTerminalService,
      deps.usbTerminalSettings);
  api.usbTerminalApi->begin();
}

void initializeStorageApis(ApiServices& api,
                           SecurityManager* securityManager,
                           const ApiServicesInitializerDeps& deps) {
  // Storage/file endpoints are kept last because they depend on LittleFS-facing
  // service wiring, but not on the broader runtime/broadcast graph above.
  api.storageService.init(&LittleFS);
  api.storageService->begin();
  api.fileManager.init(
      deps.server, securityManager, deps.powerManager, api.storageService.get(), deps.fsMutex);
  api.fileManager->begin();
}

}  // namespace

void ApiServicesInitializer::initialize(ApiServices &api, const ApiServicesInitializerDeps& deps) {
  // Resolve the shared SecurityManager once here so all Phase-1 API wrappers
  // use the same auth source. If auth behavior diverges between APIs after boot
  // refactors, start from this handoff and then inspect the relevant section.
  auto *securityManager = deps.framework->getSecurityManager();

  // The order below is the production boot order. The helpers only slice the
  // initializer into readable chunks; if one section regresses, compare its
  // position here before reordering anything locally.
  initializeCoreApis(api, securityManager, deps);
  initializeRuntimeApis(api, securityManager, deps);
  initializeInteractionApis(api, securityManager, deps);
  initializeStorageApis(api, securityManager, deps);
}
