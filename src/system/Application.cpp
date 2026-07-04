#include "Application.h"
#include "ApplicationRuntime.h"
#include "init/core/InitSequence.h"
#include "init/core/MemoryConfig.h"
#include "memory/SystemAllocator.h"

#include "../config/App.h"
#include "../config/System.h"
#include "../alarms/AlarmService.h"
#include "../system/power/PowerManager.h"
#include "../ble/BleService.h"
#include "../udp/UdpPusher.h"
#include "../usb_terminal/UsbTerminalService.h"

#include "logging/Logging.h"
#include "boot/BootTracker.h"
#include "health/SystemHealth.h"
#include "health/maintenance/HealthMaintenancePulse.h"
#include "restart/RuntimeRestart.h"
#include "watchdog/TaskWatchdog.h"

#include <cstdlib>

#undef LOG_TAG
#define LOG_TAG "App"

Application &Application::instance() {
  // Keep this singleton as a framework/entry-point boundary for now. The main
  // cleanup goal is NOT to inject Application everywhere, but to keep
  // Application::instance() confined to boot hooks, main.cpp, and similar
  // framework-owned entry points.
  //
  // Leave this in place unless the framework starts passing explicit user
  // context into those hooks. Refactoring it away prematurely would mostly add
  // plumbing while moving the same global entry-point responsibility around.
  // TODO(di): Shrink the remaining Application::instance() reach-through call
  // sites instead of expanding them. If future framework hooks can pass user
  // context directly, prefer wiring ServiceRegistry explicitly there.
  static Application* _instance = nullptr;
  if (!_instance) {
      _instance = SYSTEM::MEMORY::allocInPsram<Application>();
      if (!_instance) {
          LOGE("Failed to allocate Application in PSRAM");
          std::abort();
      }
  }
  return *_instance;
}

Application::Application() = default;

const char* Application::stateName(LifecycleState state) {
  switch (state) {
    case LifecycleState::NotInitialized:
      return "not_initialized";
    case LifecycleState::Booting:
      return "booting";
    case LifecycleState::Ready:
      return "ready";
  }

  return "unknown";
}

void Application::createMutexes() {
  if (_networkMutex || _notifMutex) {
    LOGE("Application mutexes already initialized");
    std::abort();
  }

  _networkMutex = xSemaphoreCreateMutex();
  _notifMutex = xSemaphoreCreateMutex();
  if (!_networkMutex || !_notifMutex) {
    LOGE("Failed to create application mutexes");
    std::abort();
  }
}

void Application::setup(PsychicHttpsServer &server, ESP32SvelteKit &framework) {
  if (_state != LifecycleState::NotInitialized) {
    LOGE("Application::setup() called in invalid state: %s", stateName(_state));
    std::abort();
  }

  _state = LifecycleState::Booting;

  // Ensure strict memory strategy first (moved from main.cpp)
  SYSTEM::MemoryConfig::applyAggressivePsramStrategy();
  createMutexes();

  APP_INTERNAL::runBootSequence(server,
                                framework,
                                _httpRedirectServer,
                                _services,
                                _buttonHandler,
                                _networkMutex,
                                _notifMutex);
  _state = LifecycleState::Ready;
}

void Application::loop() {
  auto* framework = _services.getFramework();
  if (_state != LifecycleState::Ready || !framework) {
    LOGE("Application::loop() called before application is ready (state=%s)",
         stateName(_state));
    std::abort();
  }

  LOG_PROFILE_START(mainLoopUs);
  APP_INTERNAL::runLoopCore(_services, _buttonHandler, *framework);

  // Keep a periodic micro-timing sample without duplicating slow-loop warnings.
  LOG_PROFILE_END_PERIODIC(
      mainLoopUs,
      "Main loop",
      TASK_MONITOR::INTERVAL_MAIN_LOOP_MS);

  auto& watchdog = SYSTEM::TaskWatchdog::instance();
  if (watchdog.isInitialized() && !watchdog.reset()) {
    LOGW("Main loop watchdog reset failed, attempting re-registration");
    const bool recovered = watchdog.registerCurrentTask() && watchdog.reset();
    if (!recovered) {
      APP_INTERNAL::restartRuntimeFault("main loop watchdog heartbeat lost",
                                        SYSTEM::ShutdownReason::WATCHDOG_RESTART);
    }
  }

  vTaskDelay(pdMS_TO_TICKS(APP::MAIN_LOOP_DELAY_MS));
}
