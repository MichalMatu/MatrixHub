#include "ServiceRegistry.h"

#include "../../notifications/runtime/NotificationWorker.h"
#include "../../sensors/imu/ImuManager.h"
#include "../../wifisensing/WifiSensingSettings.h"
#include "../../wifisensing/WifiSensingService.h"
#include "../logging/Logging.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#undef LOG_TAG
#define LOG_TAG "ServiceRegistry"

void ServiceRegistry::stopBackgroundWorkers() {
    // WifiSensingSettings owns a persistent reconciler task which dereferences
    // both _wifiSensingService and _csiService. Those dependencies are declared
    // after the settings owner and would therefore be destroyed first during
    // normal member teardown. Fence and drain the task while every dependency
    // is still alive, including partial-initialization/early-failure teardown.
    if (_wifiSensingSettings) {
        bool retryLogged = false;
        while (!_wifiSensingSettings->shutdownRuntimeReconciler(
            TIMEOUT::TASK_SHUTDOWN_POLL_TICKS)) {
            if (!retryLogged) {
                LOGW("Waiting for WiFi sensing reconciler before registry teardown");
                retryLogged = true;
            }
            vTaskDelay(TIMEOUT::TASK_SHUTDOWN_POLL_TICKS);
        }
    }

    // SensorBroadcaster and AlarmService are both raw callback consumers of the
    // RSSI task. Stop and reap that task before API wrappers or AlarmService can
    // be destroyed; a bounded best-effort stop would leave copied callbacks
    // capable of dereferencing their former owners.
    if (_wifiSensingService) {
        bool retryLogged = false;
        while (!_wifiSensingService->shutdown()) {
            if (!retryLogged) {
                LOGW("Waiting for WiFi sensing worker before registry teardown");
                retryLogged = true;
            }
            vTaskDelay(TIMEOUT::TASK_SHUTDOWN_POLL_TICKS);
        }
    }

    if (_notifications.runtimeWorker) {
        _notifications.runtimeWorker->stop();
    }
    if (_imuManager) {
        _imuManager->clearConsumers();
    }
}
