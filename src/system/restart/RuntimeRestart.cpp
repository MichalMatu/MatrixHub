#include "RuntimeRestart.h"

#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace SYSTEM::RESTART {

void emergencyRestart(SYSTEM::ShutdownReason reason,
                      const char* logTag,
                      const char* message,
                      uint32_t delayMs) {
    const char* safeTag = (logTag && logTag[0] != '\0') ? logTag : "RuntimeRestart";
    const char* safeMessage = (message && message[0] != '\0')
                                  ? message
                                  : "Emergency restart requested";

    ESP_LOGE(safeTag, "%s", safeMessage);

    // Keep the emergency restart policy centralized: runtime fault paths do
    // record a clean BootTracker marker, but they intentionally skip the full
    // shutdown orchestration because the process may already be unhealthy.
    SYSTEM::BootTracker::recordShutdown(reason);

    if (delayMs > 0) {
        vTaskDelay(pdMS_TO_TICKS(delayMs));
    }

    esp_restart();
}

}  // namespace SYSTEM::RESTART
