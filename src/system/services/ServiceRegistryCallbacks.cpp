#include "ServiceRegistry.h"

#include "ServiceRegistryApi.h"
#include "../../alarms/AlarmService.h"
#include "../../alarms/types/AlarmInputData.h"
#include "../../ble/settings/BleSettingsService.h"
#include "../../notifications/runtime/NotificationRuntimeReconciler.h"
#include "../../notifications/runtime/NotificationWorker.h"
#include "../../notifications/settings/NotificationSettingsService.h"
#include "../../sensors/imu/ImuManager.h"
#include "../../shelly/ShellyService.h"
#include "../../wifisensing/csi/core/CsiService.h"
#include "../health/network/HttpServerHealthTracker.h"
#include "../logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "ServiceRegistry"

void ServiceRegistry::detachRuntimeCallbacks() {
    // PowerSettingsService and HeartbeatSettingsService do not need explicit
    // detach logic here: their apply hooks are encapsulated inside the settings
    // services and disappear with the registry-owned service instance itself.
    if (_notificationSettings && _notificationSettingsHandlerId != 0) {
        _notificationSettings->removeUpdateHandler(_notificationSettingsHandlerId);
        _notificationSettingsHandlerId = 0;
    }

    if (_bleSettings) {
        _bleSettings->setOnSettingsChanged(nullptr);
        if (_bleWhitelistSettingsHandlerId != 0) {
            _bleSettings->removeUpdateHandler(_bleWhitelistSettingsHandlerId);
            _bleWhitelistSettingsHandlerId = 0;
        }
    }

    if (_server) {
        _server->onClose(nullptr);
    }

    if (_framework) {
        auto* wifiSettings = _framework->getWiFiSettingsService();
        if (wifiSettings) {
            wifiSettings->setActivityCallback(nullptr);
        }
    }

    if (_shellyService) {
        _shellyService->setOnStateChangeCallback(nullptr);
    }

    if (_csiService) {
        _csiService->setCsiCallback(nullptr);
        _csiService->setMotionCallback(nullptr);
    }
}

void ServiceRegistry::stopBackgroundWorkers() {
    if (_notifications.runtimeWorker) {
        _notifications.runtimeWorker->stop();
    }
    if (_imuManager) {
        _imuManager->clearConsumers();
    }
}

void ServiceRegistry::destroyStaticServices() {
    // After the lifecycle cleanup pass, registry-owned services are cleaned up
    // automatically by RAII. This phase only has to tear down the remaining
    // static API wrappers in reverse order before owned services destruct.
    _api->destroyAll();
    _matrixSettings.destroy();
    _usbTerminalService.destroy();
}

void ServiceRegistry::wireRuntimeCallbacks() {
    if (_notificationSettings && _notifications.runtimeWorker) {
        _notificationSettingsHandlerId =
            _notificationSettings->addUpdateHandler([this](std::string_view originId) {
                (void)originId;
                if (_isDying.load(std::memory_order_acquire)) {
                    return StateHandlerResult::success();
                }
                NOTIFICATIONS::NotificationRuntimeReconciler::reconcile(
                    _notificationSettings.get(), _notifications.runtimeWorker.get());
                return StateHandlerResult::success();
            });

        NOTIFICATIONS::NotificationRuntimeReconciler::reconcile(
            _notificationSettings.get(), _notifications.runtimeWorker.get());
    }

    if (_server) {
        _server->onClose([this](PsychicClient* client) {
            if (!client || _isDying.load(std::memory_order_acquire)) {
                return;
            }

            SYSTEM::HEALTH::HttpServerHealthTracker::recordClose();
            const int fd = client->socket();
            if (_api->systemApi) {
                _api->systemApi->cleanupClient(fd);
            }
            if (_api->wifiSensingApi) {
                _api->wifiSensingApi->cleanupClient(fd);
            }
            if (_api->airMouseApi) {
                _api->airMouseApi->cleanupClient(fd);
            }
            if (_api->usbTerminalApi) {
                _api->usbTerminalApi->cleanupClient(fd);
            }
        });
    }

    if (_framework->getWiFiSettingsService()) {
        _framework->getWiFiSettingsService()->setActivityCallback([this]() {
            if (_isDying.load(std::memory_order_acquire)) {
                return;
            }
            _powerManager->notifyActivity("wifi-connect");
        });
    }

    if (_shellyService && _api->systemApi) {
        _shellyService->setOnStateChangeCallback([this](const SHELLY::ShellyDevice& dev) {
            if (_isDying.load(std::memory_order_acquire)) {
                return;
            }
            _api->systemApi->sendShellyEvent(&dev);
        });
    }

    if (_csiService && _alarmService) {
        _csiService->setMotionCallback([this](bool motion) {
            if (_isDying.load(std::memory_order_acquire) || !_alarmService) {
                return;
            }
            ALARMS::AlarmInputData input;
            input.wifiCsiMotion = motion ? 1.0f : 0.0f;
            _alarmService->submitInput(input);
        });
    }

    LOGI("[Registry] Runtime callbacks wired");
}
