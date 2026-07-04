#include "ShutdownSequence.h"

// Hardware and System
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Wire.h>

// Modules
#include "../logging/Logging.h"
#include "../logging/LogRingBuffer.h"
#include "../../sensors/logging/PsramLogBuffer.h"
#include "../boot/BootTracker.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../ble/BleService.h"
#include <MatrixService.h> 
#include "../matrix/MatrixTask.h"
#include "../../sensors/SensorLoggingTask.h"
#include "../services/ServiceRegistry.h"
#include "../../config/System.h"
#include "../../config/Network.h"
#include "../../shelly/ShellyRuntimeControl.h"
#include "../watchdog/TaskWatchdog.h"
#include "../../system/power/PowerManager.h"
#include "../health/heartbeat/Heartbeat.h"
#include "../thermal/ThermalMonitor.h"

// Additional workers for graceful shutdown
#include "../../notifications/runtime/NotificationWorker.h"
#include "../../wifisensing/WifiSensingService.h"
#include "../../airmouse/AirMouseService.h"
#include "../../sensors/imu/ImuManager.h"
#include "../../macros/MacroService.h"
#include "../../api/notifications/NotificationsApiService.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#undef LOG_TAG
#define LOG_TAG "Shutdown"

namespace SYSTEM {
namespace {

bool isMaintenanceSleepReason(const char* reason) {
    if (!reason || reason[0] == '\0') {
        return false;
    }

    return strcmp(reason, "hygiene") == 0 ||
           strcmp(reason, "manual-hygiene") == 0 ||
           strcmp(reason, "thermal-critical") == 0;
}

ShutdownReason resolveShutdownReason(const char* sleepReason) {
    return isMaintenanceSleepReason(sleepReason)
               ? ShutdownReason::HYGIENE_SLEEP
               : ShutdownReason::CLEAN_SLEEP;
}

// Keep the string form local to shutdown logging. BootTracker has its own
// logging, but this helper makes the pre-restart/pre-sleep logs readable
// without duplicating the enum decoding inline at each call site.
const char* describeShutdownReason(ShutdownReason reason) {
    switch (reason) {
        case ShutdownReason::CLEAN_SLEEP: return "clean_sleep";
        case ShutdownReason::RESTART_COMMAND: return "restart_cmd";
        case ShutdownReason::OTA_UPDATE: return "ota";
        case ShutdownReason::FACTORY_RESET: return "factory_reset";
        case ShutdownReason::WATCHDOG_RESTART: return "watchdog";
        case ShutdownReason::LOW_MEMORY: return "low_memory";
        case ShutdownReason::HYGIENE_SLEEP: return "hygiene_sleep";
        default: return "unknown";
    }
}

bool isMaintenanceShutdown(ShutdownReason reason) {
    return reason == ShutdownReason::HYGIENE_SLEEP;
}

void requestHttpClientClose(PsychicHttpServer* server) {
    if (!server) {
        return;
    }

    // Shutdown is one of the worst moments to depend on fresh heap
    // allocations: low-memory hygiene sleep and runtime recovery paths can land
    // here precisely because the system is already degraded. Keep the HTTP
    // client snapshot fixed-size and bounded by the configured server socket
    // limit instead of allocating a std::vector on the fly.
    int clientSockets[NET::HTTP::MAX_OPEN_SOCKETS] = {0};
    size_t clientCount = 0;
    bool truncated = false;

    // Snapshot sockets first. applyToAllClients() iterates under the server's
    // client mutex, while actual close callbacks also touch that list. Closing
    // from the snapshot keeps the lock hold short and avoids re-entering the
    // list mutation path from inside the iteration callback.
    server->applyToAllClients([&](PsychicClient* client) {
        if (client) {
            if (clientCount < NET::HTTP::MAX_OPEN_SOCKETS) {
                clientSockets[clientCount++] = client->socket();
            } else {
                truncated = true;
            }
        }
    });

    if (truncated) {
        // This should not happen under the current appliance-style workload,
        // but keep the warning so later "turn it into a generic web server"
        // experiments do not silently hide close requests during shutdown.
        LOGW("HTTP client close snapshot truncated at %u sockets",
             static_cast<unsigned>(NET::HTTP::MAX_OPEN_SOCKETS));
    }

    if (clientCount == 0) {
        return;
    }

    LOGI("Requesting close for %u active HTTP clients",
         static_cast<unsigned>(clientCount));

    for (size_t i = 0; i < clientCount; i++) {
        const int fd = clientSockets[i];
        PsychicClient* client = server->getClient(fd);
        if (client) {
            (void)client->close();
        }
    }
}

}  // namespace

void ShutdownSequence::execute(ServiceRegistry& registry) {
    execute(registry, ShutdownReason::UNKNOWN);
}

void ShutdownSequence::execute(ServiceRegistry& registry, ShutdownReason explicitReason) {
    LOGI("Executing shutdown sequence...");

    persistState(registry, explicitReason);
    stopBackgroundTasks(registry);  // Must stop tasks BEFORE killing network!
    stopNetworkServices(registry);
    stopHardware(registry);

    LOGI("Shutdown complete. Entering sleep/halt.");
    
    // Clean up singleton/static memory allocations
    SENSORS::PsramLogBuffer::end();
    LOG::RingBuffer::end();
    
    // Final serial flush before death
    Serial.flush();
    vTaskDelay(pdMS_TO_TICKS(SHUTDOWN::SERIAL_FLUSH_DELAY_MS));
    Serial.end();
    vTaskDelay(pdMS_TO_TICKS(SHUTDOWN::SERIAL_END_DELAY_MS));
}


void ShutdownSequence::persistState(ServiceRegistry& registry, ShutdownReason explicitReason) {
    const char* reason = nullptr;
    if (registry.getPowerManager()) {
        reason = registry.getPowerManager()->getSleepReason();
    }

    // There are two possible sources for the final shutdown marker:
    // - PowerManager sleep reason for normal sleep paths,
    // - explicitReason for callers such as restart/factory reset.
    // Resolve that once here so BootTracker and the log line both describe the
    // same final outcome.
    const ShutdownReason shutdownReason =
        explicitReason != ShutdownReason::UNKNOWN ? explicitReason : resolveShutdownReason(reason);
    const bool isMaintenance = isMaintenanceShutdown(shutdownReason);

    // Log the raw PowerManager sleep reason and the resolved shutdown marker
    // separately. That keeps restart/sleep debugging readable without forcing
    // callers to mentally reconstruct which source "won" after explicitReason
    // overrides the PowerManager string.
    LOGI("Shutdown persistState: sourceReason=%s, resolved=%s, maintenance=%d",
         reason ? reason : "(null)",
         describeShutdownReason(shutdownReason),
         isMaintenance);

    BootTracker::recordShutdown(shutdownReason);
}

void ShutdownSequence::stopBackgroundTasks(ServiceRegistry& registry) {
    LOGI("Stopping background tasks...");
    
    // Unregister loopTask from watchdog FIRST to prevent WDT trigger during shutdown
    TaskWatchdog::instance().unregisterCurrentTask();

    // Detached notification test jobs are outside NotificationWorker. Drain
    // them first so "send test + immediate restart" does not leave an API test
    // task racing the later WiFi shutdown path.
    if (auto* notificationsApi = registry.getNotificationsApiService()) {
        notificationsApi->shutdown();
    }
    
    // Stop notification worker (network I/O)
    auto* worker = registry.getNotificationWorker();
    if (worker) {
        worker->stop();
    }
    
    // Stop WiFi sensing (if running)
    auto* wifiSensing = registry.getWifiSensingService();
    if (wifiSensing) {
        wifiSensing->stop();
    }
    
    // Stop AirMouse service (USB HID)
    auto* amService = registry.getAirMouseService();
    if (amService) {
        amService->stop();
    }
    
    // Stop MacroEngine (may use keyboard/USB)
    auto* macroService = registry.getMacroService();
    if (macroService) {
        macroService->stop();
    }
    
    // Stop sensor logging task (uses I2C, safe to stop early)
    SensorLoggingTask::stop();

    // Stop Heartbeat worker before WiFi goes down.
    Heartbeat::stop();
    
    // Stop Shelly service (uses HTTP - MUST stop before WiFi off!)
    auto* shellyService = registry.getShellyService();
    if (SHELLY::runtimeIsRunning(shellyService)) {
        SHELLY::runtimeStop(shellyService);
    }
    
    // Stop HTTP server to prevent httpd task from blocking
    auto* server = registry.getServer();
    if (server) {
        LOGI("Stopping HTTP server... (Closing sockets only)");
        // [FIX] server->stop() is blocking and can hang if clients are connected (e.g. SSE).
        // Since we are entering Deep Sleep, the network hardware will be reset anyway.
        // We therefore avoid httpd_stop(), but still ask every active socket to
        // close first so onClose cleanup runs and live clients are not simply
        // torn out from under the network stack when WiFi.mode(WIFI_OFF) lands.
        requestHttpClientClose(server);
        vTaskDelay(pdMS_TO_TICKS(SHUTDOWN::HTTP_CLIENT_CLOSE_GRACE_MS));
        // server->stop();
    }
    
    // [FIX] Explicitly stop background tasks that might span logs
    LOGI("Stopping MatrixTask...");
    MATRIX::MatrixTask::stop();

    LOGI("Stopping ThermalMonitor...");
    ThermalMonitor::instance().stop();
    
    LOGI("Background tasks stopped");
}

void ShutdownSequence::stopNetworkServices(ServiceRegistry& registry) {
    LOGI("Stopping network services...");
    // [FIX] MDNS.end() REMOVED — it deadlocks when httpd_thread is still active.
    // mdns_free() internally sends a stop command via queue to the mDNS task,
    // but httpd_thread blocks the scheduler while processing the sleep HTTP request.
    // WiFi.mode(WIFI_OFF) below tears down the entire network stack, which
    // implicitly cleans up mDNS resources.
    // MDNS.end();
    
    // Stop BLE operations (soft stop)
    if (auto* ble = registry.getBleService()) {
        ble->prepareForSleep();
    }
    
    // Disable WiFi radio - non-blocking preferred for speed
    WiFi.mode(WIFI_OFF);
}

void ShutdownSequence::stopHardware(ServiceRegistry& registry) {
    LOGI("Stopping hardware peripherals...");
    
    // Failsafe: Ensure Matrix is OFF even if ThermalMonitor logic failed
    if (registry.getMatrixService()) {
        registry.getMatrixService()->setThermalBrightnessLimit(UI::MATRIX::BRIGHTNESS_OFF);
        registry.getMatrixService()->clear(true); // Stop bg effects
    }
    // Send empty frame to hardware immediately if possible - rely on task delay below
    
    // Give tasks a moment to realize they should stop before killing I2C
    vTaskDelay(pdMS_TO_TICKS(SHUTDOWN::HARDWARE_SETTLE_DELAY_MS));

    if (auto* imuManager = registry.getImuManager()) {
        imuManager->clearConsumers();
    }

    // [FIX] Wire.end() REMOVED
    // Reason: Even though SensorLoggingTask::stop() is blocking (waits for task exit),
    // other background tasks or interrupts may still access I2C.
    // ESP32 automatically resets all peripherals on deep sleep and soft reset,
    // so manual Wire.end() provides no benefit but risks I2C driver crash.
    // Wire.end();  // DISABLED - see above
}

} // namespace SYSTEM
