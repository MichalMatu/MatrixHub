#include "../Application.h"
#include "../boot/BootTracker.h"
#include "../shutdown/ShutdownSequence.h"

#include <Arduino.h>
#include <esp_system.h>

// Application hook used by the embedded web/API framework runtime.
// When the UI or a settings service requests RestartService::restartNow(),
// the framework delegates here (if present) instead of using its older,
// framework-local WiFi teardown path.
//
// We intentionally reuse the same graceful shutdown sequence as hygiene sleep
// and factory reset so planned restarts:
// - persist a clean shutdown marker for BootTracker diagnostics,
// - stop network/background workers in a consistent order,
// - and avoid having one "special" restart path that bypasses runtime cleanup.
//
// NOTE: Application::instance() is acceptable here because this is an extern
// "C" framework callback/entry point. Normal runtime code should continue to
// use explicit DI via ServiceRegistry rather than reaching through Application.
extern "C" void svk_appRestartNow() {
    auto* registry = Application::instance().getServiceRegistry();

    // Keep the small pre-restart delay from the framework path so the HTTP
    // response that triggered the restart has a moment to flush before we start
    // tearing down sockets and workers underneath it.
    vTaskDelay(pdMS_TO_TICKS(250));

    if (registry) {
        SYSTEM::ShutdownSequence::execute(*registry, SYSTEM::ShutdownReason::RESTART_COMMAND);
    } else {
        // Extremely early/late restart requests can arrive before the registry
        // is available. Fall back to recording the reason so BootTracker still
        // classifies this reboot as planned instead of "unexpected".
        SYSTEM::BootTracker::recordShutdown(SYSTEM::ShutdownReason::RESTART_COMMAND);
    }

    esp_restart();
}
