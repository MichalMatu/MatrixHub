#include "ServiceRegistry.h"

#include "ServiceRegistryApi.h"
#include "core/config/ConfigCommon.h"
#include "../../config/System.h"
#include "../../airmouse/AirMouseService.h"
#include "../../alarms/AlarmService.h"
#include "../../alarms/AlarmSettingsService.h"
#include "../../ble/BleService.h"
#include "../../ble/settings/BleSettingsService.h"
#include "../../compensation/CompensationService.h"
#include "../../compensation/CompensationSettingsService.h"
#include "../../gpio/GpioService.h"
#include "../../macros/MacroService.h"
#include "../../macros/MacroSettingsService.h"
#include "../../notifications/pushover/PushoverNotifier.h"
#include "../../notifications/pushover/PushoverWorker.h"
#include "../../notifications/runtime/NotificationWorker.h"
#include "../../notifications/settings/NotificationSettingsService.h"
#include "../../notifications/telegram/TelegramNotifier.h"
#include "../../notifications/telegram/client/TelegramClient.h"
#include "../../notifications/telegram/queue/MessageQueue.h"
#include "../../notifications/telegram/runtime/TelegramCommandRuntimeState.h"
#include "../../notifications/telegram/runtime/TelegramWorker.h"
#include "../../notifications/webhook/WebhookNotifier.h"
#include "../../notifications/webhook/WebhookWorker.h"
#include "../../sensors/imu/ImuManager.h"
#include "../../sensors/imu/ImuRuntimeService.h"
#include "../../sensors/imu/ImuService.h"
#include "../../shelly/ShellyService.h"
#include "../../sensors/scd4x/Scd4xSensorService.h"
#include "../../udp/UdpPusher.h"
#include "../../udp/UdpSettingsService.h"
#include "../../usb_terminal/UsbTerminalSettingsService.h"
#include "../../wifisensing/WifiSensingSettings.h"
#include "../../wifisensing/WifiSensingService.h"
#include "../../wifisensing/csi/core/CsiService.h"
#include "../../api/notifications/senders/PushoverTestSender.h"
#include "../../api/notifications/senders/TelegramTestSender.h"
#include "../../api/notifications/senders/WebhookTestSender.h"
#include "../health/heartbeat/HeartbeatSettingsService.h"
#include "../logging/Logging.h"
#include "../matrix/menu/MatrixMenuService.h"
#include "../matrix_manager/MatrixManagerService.h"
#include "../power/PowerManager.h"
#include "../power/PowerSettingsService.h"
#include "MatrixService.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdlib>

#undef LOG_TAG
#define LOG_TAG "ServiceRegistry"

namespace {

template <typename Callback>
void runRegistryPhase(const char* label, Callback&& callback) {
    LOG::PhaseTimer timer("Registry");
    callback();
    timer.logDone(ESP_LOG_INFO, LOG_TAG, label);
}

}  // namespace

ServiceRegistry::NotificationServicesState::~NotificationServicesState() = default;

ServiceRegistry::ServiceRegistry()
    : _server(nullptr)
    , _framework(nullptr)
    , _api(std::make_unique<ApiServices>())
    , _powerManager(std::make_unique<POWER::PowerManager>()) {
    _matrixService = std::make_unique<MatrixService>();
    _matrixManager =
        std::make_unique<MATRIX_MANAGER::MatrixManagerService>(_matrixService.get());
}

ServiceRegistry::~ServiceRegistry() {
    // Signal shutdown FIRST - callbacks check this flag.
    _isDying.store(true, std::memory_order_release);

    detachRuntimeCallbacks();

    // Synchronization point for dual-core ESP32.
    LOGI("ServiceRegistry shutdown: waiting for in-flight callbacks...");
    vTaskDelay(pdMS_TO_TICKS(SHUTDOWN::REGISTRY_CALLBACK_SETTLE_DELAY_MS));

    stopBackgroundWorkers();
    destroyStaticServices();

    _fsMutex = nullptr;

    LOGI("ServiceRegistry destroyed - RAII cleanup complete");
}

void ServiceRegistry::begin(PsychicHttpServer& server,
                            ESP32SvelteKit& framework,
                            SemaphoreHandle_t networkMutex,
                            SemaphoreHandle_t notifMutex) {
    _fsMutex = g_fsMutex;
    if (!_fsMutex) {
        LOGE("Global FS mutex not initialized before ServiceRegistry::begin");
        std::abort();
    }

    bindRuntimeContext(server, framework);

    LOGI("Initializing services...");
    runInitializationSequence(networkMutex, notifMutex);
    LOGI("All services initialized successfully");
}

void ServiceRegistry::bindRuntimeContext(PsychicHttpServer& server,
                                         ESP32SvelteKit& framework) {
    _server = &server;
    _framework = &framework;
}

API::NotificationsApiService* ServiceRegistry::getNotificationsApiService() const {
    return _api ? _api->notificationsApi.get() : nullptr;
}

void ServiceRegistry::runInitializationSequence(SemaphoreHandle_t networkMutex,
                                                SemaphoreHandle_t notifMutex) {
    initializeCoreServices();
    runRegistryPhase("Business services", [&]() {
        initializeBusinessServices(networkMutex);
    });
    runRegistryPhase("BLE services", [&]() { initializeBleServices(); });
    runRegistryPhase("Matrix services", [&]() { initializeMatrixServices(); });
    runRegistryPhase("IMU services", [&]() { initializeImuServices(); });
    runRegistryPhase("Interaction services", [&]() { initializeInteractionServices(); });
    runRegistryPhase("Notification services", [&]() {
        initializeNotificationServices(notifMutex);
    });
    runRegistryPhase("API services", [&]() { initializeApiServices(); });
    runRegistryPhase("Runtime callbacks", [&]() { wireRuntimeCallbacks(); });
}
