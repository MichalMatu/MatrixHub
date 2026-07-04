#include "FactoryReset.h"

#include <Arduino.h>
#include <array>
#include <LittleFS.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#if __has_include(<esp_task_wdt.h>)
#include <esp_task_wdt.h>
#define FACTORY_HAS_WDT 1
#else
#define FACTORY_HAS_WDT 0
#endif
#include "../logging/Logging.h"
#include "../../config/App.h"
#include "../watchdog/TaskWatchdog.h"
#include "../shutdown/ShutdownSequence.h"
#include "../services/ServiceRegistry.h"

#undef LOG_TAG
#define LOG_TAG "Factory"

namespace {
constexpr std::array<const char *, 4> kPrefsNamespaces = {"log_cfg", "power_cfg", "sensor_cal", "scd4x_comp"};
}

namespace SYSTEM {

void performFactoryReset(ServiceRegistry& registry) {
    LOGW("Starting factory reset: executing shutdown, clearing preferences and formatting FS...");

    // Perform graceful shutdown of background services so they unregister
    // from the watchdog and stop network/hardware operations before we
    // touch storage. This prevents tasks from calling esp_task_wdt_reset()
    // while we are in the middle of formatting.
    SYSTEM::ShutdownSequence::execute(registry, SYSTEM::ShutdownReason::FACTORY_RESET);

#if FACTORY_HAS_WDT
    // After shutdown, attempt to deinitialize the TWDT. If other tasks
    // still remain subscribed, deinit will fail - that's OK; we proceed
    // with format and restart regardless.
    esp_err_t deinitErr = esp_task_wdt_deinit();
    if (deinitErr == ESP_OK) {
        LOGI("TWDT deinitialized for factory reset");
    } else if (deinitErr == ESP_ERR_INVALID_STATE) {
        LOGW("TWDT not initialized (nothing to deinit)");
    } else {
        LOGW("esp_task_wdt_deinit() returned: %s", esp_err_to_name(deinitErr));
    }
#endif

        // Heartbeat log every ~2s while we work
        auto logProgress = [](const char* step, uint32_t startMs) {
            uint32_t elapsed = millis() - startMs;
            if ((elapsed / FACTORY::PROGRESS_LOG_INTERVAL_MS) != ((elapsed - 1) / FACTORY::PROGRESS_LOG_INTERVAL_MS)) {
                LOGI("[FactoryReset] %s (t=%lu ms)", step, static_cast<unsigned long>(elapsed));
            }
        };

        uint32_t startMs = millis();

    // Clear NVS preferences (small runtime config)
    Preferences prefs;
    for (const char *ns : kPrefsNamespaces) {
        if (prefs.begin(ns, false)) {
            prefs.clear();
            prefs.end();
            LOGI("Preferences cleared: %s", ns);
        } else {
            LOGW("Unable to open prefs namespace: %s", ns);
        }
    }

        logProgress("after prefs", startMs);

    // Format LittleFS (logs/config stored there)
    // Unmount first to avoid "Already Mounted" warnings.
    LittleFS.end();
    if (!LittleFS.begin()) {
        LOGW("LittleFS mount failed before format; attempting format anyway");
    }
    if (LittleFS.format()) {
        LOGI("LittleFS formatted");
    } else {
        LOGE("LittleFS format failed");
    }

        logProgress("after format", startMs);

    LOGW("Restarting...");
    vTaskDelay(pdMS_TO_TICKS(FACTORY::PRE_RESTART_DELAY_MS));
    esp_restart();
}

}  // namespace SYSTEM
