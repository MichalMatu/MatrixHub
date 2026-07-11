#include "ApplicationRuntime.h"

#include <PsychicHttpsServer.h>

#include "button/ButtonHandler.h"
#include "health/SystemHealth.h"
#include "health/maintenance/HealthMaintenancePulse.h"
#include "init/core/InitSequence.h"
#include "logging/Logging.h"
#include "network/HttpRedirectServer.h"
#include "power/PowerManager.h"
#include "restart/RuntimeRestart.h"
#include "services/ServiceRegistry.h"
#include "../alarms/AlarmService.h"
#include "../ble/BleService.h"
#include "../sensors/imu/ImuRuntimeService.h"
#include "../shelly/ShellyRuntimeControl.h"
#include "../udp/UdpPusher.h"
#include "../usb_terminal/UsbTerminalService.h"
#include "../wifisensing/WifiSensingSettings.h"

#include <cstdlib>

#undef LOG_TAG
#define LOG_TAG "App"

namespace APP_INTERNAL {

void restartRuntimeFault(const char* logReason, SYSTEM::ShutdownReason shutdownReason) {
  // Main-loop/runtime faults intentionally stay on the emergency restart path
  // instead of reusing ShutdownSequence. Keep that policy centralized so other
  // fault sites do not drift into slightly different "record + restart" flows.
  SYSTEM::RESTART::emergencyRestart(
      shutdownReason,
      LOG_TAG,
      logReason ? logReason : "Runtime fault. Restarting immediately.");
}

void runBootSequence(PsychicHttpsServer& server,
                     ESP32SvelteKit& framework,
                     SYSTEM::NETWORK::HttpRedirectServer& httpRedirectServer,
                     ServiceRegistry& services,
                     ButtonHandler& buttonHandler,
                     SemaphoreHandle_t networkMutex,
                     SemaphoreHandle_t notifMutex) {
  // Keep the end-to-end boot sequence visible in one place. InitSequence owns
  // the per-phase details, while this wrapper preserves the exact ordering the
  // product depends on during startup and gives PhaseTimer one readable view.
  LOG::PhaseTimer bootTimer("Boot");

  SYSTEM::InitSequence::phase1_Storage();
  LOG_PHASE_STEP(bootTimer, "Phase1 Storage");

  SYSTEM::InitSequence::phase2_Logging();
  LOG_PHASE_STEP(bootTimer, "Phase2 Logging");

  SYSTEM::InitSequence::phase3_Power(services);
  LOG_PHASE_STEP(bootTimer, "Phase3 Power");

  SYSTEM::InitSequence::configureNetwork(server);
  LOG_PHASE_STEP(bootTimer, "Network config");

  SYSTEM::InitSequence::phase4_Framework(framework);
  LOG_PHASE_STEP(bootTimer, "Phase4 Framework");

  SYSTEM::InitSequence::phase4_HttpRedirect(httpRedirectServer, server.config);
  LOG_PHASE_STEP(bootTimer, "Phase4 HTTP redirect");

  SYSTEM::InitSequence::phase5_Services(
      static_cast<PsychicHttpServer&>(server), framework, services, networkMutex, notifMutex);
  LOG_PHASE_STEP(bootTimer, "Phase5 Services");

  SYSTEM::InitSequence::phase6_Tasks(services);
  LOG_PHASE_STEP(bootTimer, "Phase6 Tasks");

  // Phase 7 owns button routing because by then all services that provide
  // button side effects (menu, AirMouse, factory reset path, power activity)
  // already exist and can be wired explicitly.
  SYSTEM::InitSequence::phase7_Monitoring(services, buttonHandler);
  LOG_PHASE_STEP(bootTimer, "Phase7 Monitoring");

  SYSTEM::InitSequence::signalBootComplete();
  LOG_PHASE_DONE(bootTimer, "Setup");
}

void runLoopCore(ServiceRegistry& services,
                 ButtonHandler& buttonHandler,
                 ESP32SvelteKit& framework) {
  // The main loop intentionally stays thin: drive the registry-owned runtime
  // services, let async tasks produce data elsewhere, and centralize only the
  // few cross-cutting operations that must happen on every pass.
  auto* powerManager = services.getPowerManager();
  if (!powerManager) {
    LOGE("PowerManager unavailable in main loop");
    std::abort();
  }
  powerManager->loopTick();

  // The main loop drives the same Application-owned handler that Phase 7
  // configured. There is no singleton fallback path anymore.
  buttonHandler.update();

  framework.loop();

  if (auto* ble = services.getBleService()) {
    ble->loop();
  }

  SYSTEM::SystemHealth::update();

  if (auto* imuRuntime = services.getImuRuntimeService()) {
    imuRuntime->tick();
  }

  if (auto* alarmService = services.getAlarmService()) {
    // Alarm producers only publish the newest merged snapshot. We intentionally
    // centralize actual rule execution here so sensor/WiFi tasks do not block
    // on alarm-side work and multiple fast updates naturally coalesce.
    alarmService->processPending();
  }

  if (auto* shelly = services.getShellyService()) {
    // A desired relay state lives independently of the worker task. If task
    // stack/control-block allocation failed, recreate the runtime from the
    // retained in-memory ledger without waiting for another alarm edge.
    SHELLY::reconcileRuntimeIfDue(shelly, millis());
  }

  if (auto* wifiSensingSettings = services.getWifiSensingSettings()) {
    // Repair desired/runtime drift after transient CSI/RSSI lifecycle failures.
    // The settings service owns a bounded exponential backoff.
    wifiSensingSettings->reconcileRuntimeIfDue(millis());
  }

  // Deferred CSI frontend/capture lifecycle requests are generation-backed.
  // This poll only does work when task creation previously failed, closing the
  // recovery path without blocking the CSI processing callback.
  services.reconcileDeferredApiLifecycle();

  if (auto* udpPusher = services.getUdpPusher()) {
    // UdpPusher is no longer a hidden singleton. The main loop updates the
    // exact registry-owned instance that API test endpoints and boot code use.
    udpPusher->update();
  }

  if (auto* usbTerminal = services.getUsbTerminalService()) {
    usbTerminal->loop();
  }

  // ImuService sampling stays off the main loop; AirMouse consumes cached samples.
  SYSTEM::HealthMaintenancePulse::update();
}

}  // namespace APP_INTERNAL
