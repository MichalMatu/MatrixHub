/**
 * @file TaskWatchdog.cpp
 * @brief Implementation of FreeRTOS Task Watchdog integration
 */

#include "TaskWatchdog.h"
#include "../logging/Logging.h"
#include "../boot/BootTracker.h"
#include <esp_log.h>

#undef LOG_TAG
#define LOG_TAG "TaskWDT"

namespace SYSTEM {

namespace {
constexpr const char* kEspTaskWdtTag = "task_wdt";
}

#ifdef NATIVE_BUILD

void TaskWatchdog::begin(uint32_t timeoutSec, bool panicOnTimeout) {
    (void)panicOnTimeout;
    _initialized = true;
    _timeoutSec = timeoutSec;
}

bool TaskWatchdog::registerCurrentTask() {
    return true;
}

bool TaskWatchdog::registerTask(TaskHandle_t taskHandle) {
    (void)taskHandle;
    return true;
}

bool TaskWatchdog::unregisterCurrentTask() {
    return true;
}

bool TaskWatchdog::unregisterTask(TaskHandle_t taskHandle) {
    (void)taskHandle;
    return true;
}

bool TaskWatchdog::reset() {
    return true;
}

#else

void TaskWatchdog::begin(uint32_t timeoutSec, bool panicOnTimeout) {
    if (_initialized) return;

    bool setupSuccess = false;

    esp_task_wdt_config_t config = {
        .timeout_ms = timeoutSec * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,    // Watch idle tasks on all cores
        .trigger_panic = panicOnTimeout
    };

    // Arduino/ESP-IDF sometimes pre-initializes TWDT before our Phase3 boot path.
    // esp_task_wdt_init() logs "TWDT already initialized" internally before
    // returning ESP_ERR_INVALID_STATE, which makes production boot logs look like
    // a real failure even though the following reconfigure path is expected.
    //
    // Mute only the low-level task_wdt tag around this probe; our own warnings stay
    // visible and we immediately restore the previous log level afterwards.
    const esp_log_level_t previousTaskWdtLevel = esp_log_level_get(kEspTaskWdtTag);
    esp_log_level_set(kEspTaskWdtTag, ESP_LOG_NONE);
    esp_err_t err = esp_task_wdt_init(&config);
    esp_log_level_set(kEspTaskWdtTag, previousTaskWdtLevel);

    // Initialize or reconfigure the Task WDT so our timeout/panic settings are applied.
    // Some frameworks pre-init TWDT with their defaults; we still want to override.
    if (err == ESP_OK) {
        setupSuccess = true;
    } else if (err == ESP_ERR_INVALID_STATE) {
        // Already initialized - reconfigure to enforce our settings.
        err = esp_task_wdt_reconfigure(&config);
        if (err == ESP_OK) {
            setupSuccess = true;
        } else {
            LOGW("esp_task_wdt_reconfigure() failed: %s", esp_err_to_name(err));
        }
    } else {
        LOGW("esp_task_wdt_init() returned: %s", esp_err_to_name(err));
    }

    if (setupSuccess) {
        _initialized = true;
        _timeoutSec = timeoutSec;

        TaskHandle_t cur = xTaskGetCurrentTaskHandle();
        esp_err_t addErr = esp_task_wdt_add(NULL);
        if (addErr == ESP_OK || addErr == ESP_ERR_INVALID_ARG) {
            _mainTaskRegistered = true;
            _mainTaskHandle = cur;
        }
    }
}

bool TaskWatchdog::registerCurrentTask() {
    TaskHandle_t cur = xTaskGetCurrentTaskHandle();
    if (_mainTaskRegistered && cur == _mainTaskHandle) {
        // Already registered the main/setup task during begin(); avoid re-adding.
        return true;
    }
    return registerTask(cur);
}

bool TaskWatchdog::registerTask(TaskHandle_t taskHandle) {
    if (!_initialized) {
        LOGW("Task WDT not initialized, skipping registration");
        return false;
    }
    
    if (!taskHandle) {
        LOGE("Cannot register null task handle");
        return false;
    }
    
    esp_err_t err = esp_task_wdt_add(taskHandle);
    if (err == ESP_ERR_INVALID_ARG) {
        // Task already registered
        return true;
    }
    if (err != ESP_OK) {
        LOGE("Failed to register task with WDT: %s", esp_err_to_name(err));
        return false;
    }
    
    const char* name = pcTaskGetName(taskHandle);
    LOGI("Task '%s' registered with watchdog", name ? name : "?");
    return true;
}

bool TaskWatchdog::unregisterCurrentTask() {
    if (!_initialized) {
        return false;
    }
    TaskHandle_t cur = xTaskGetCurrentTaskHandle();

    // Check if task is actually registered before trying to delete
    // This avoids "E (x) task_wdt: delete_entry(x): task not found" system logs
    esp_err_t status = esp_task_wdt_status(cur);
    if (status == ESP_ERR_NOT_FOUND) {
        // Not registered, so nothing to do. Consider success.
        return true;
    }

    esp_err_t err = esp_task_wdt_delete(nullptr);  // nullptr = current task
    if (err == ESP_ERR_INVALID_ARG || err == ESP_ERR_NOT_FOUND) {
        // Task not registered - OK
        return true;
    }
    if (err != ESP_OK) {
        LOGE("Failed to unregister task from WDT: %s", esp_err_to_name(err));
        return false;
    }
    if (_mainTaskRegistered && cur == _mainTaskHandle) {
        _mainTaskRegistered = false;
        _mainTaskHandle = nullptr;
    }

    const char* name = pcTaskGetName(cur);
    LOGD("Task '%s' unregistered from watchdog", name ? name : "?");
    return true;
}

bool TaskWatchdog::unregisterTask(TaskHandle_t taskHandle) {
    if (!_initialized) {
        return false;
    }
    
    if (!taskHandle) {
        return false;
    }
    
    esp_err_t err = esp_task_wdt_delete(taskHandle);
    if (err == ESP_ERR_INVALID_ARG) {
        // Task not registered - OK
        return true;
    }
    if (err != ESP_OK) {
        LOGE("Failed to unregister task from WDT: %s", esp_err_to_name(err));
        return false;
    }
    
    const char* name = pcTaskGetName(taskHandle);
    LOGD("Task '%s' unregistered from watchdog", name ? name : "?");
    return true;
}

bool TaskWatchdog::reset() {
    if (!_initialized) {
        return false;
    }

    TaskHandle_t cur = xTaskGetCurrentTaskHandle();
    if (!cur) {
        return false;
    }

    // March 20, 2026 regression note:
    // NetworkClientSecure gained a TLS wait hook that periodically calls
    // TaskWatchdog::reset() from deep inside handshake/read wait loops.
    // Some callers (most visibly notif_worker and the later Heartbeat worker)
    // were not registered with TWDT, so esp_task_wdt_reset() produced a noisy
    // "task not found" error on every retry/yield step.
    //
    // Guard the low-level reset with esp_task_wdt_status() first so the wrapper
    // becomes safe for mixed callers: registered tasks still feed TWDT, while
    // intentionally unregistered contexts fail quietly instead of spamming logs.
    esp_err_t status = esp_task_wdt_status(cur);
    if (status == ESP_ERR_NOT_FOUND || status == ESP_ERR_INVALID_ARG) {
        return false;
    }
    if (status != ESP_OK) {
        return false;
    }

    esp_err_t err = esp_task_wdt_reset();
    if (err != ESP_OK) {
        // Task might not be registered - this is not always an error
        return false;
    }

    return true;
}

#endif

}  // namespace SYSTEM
