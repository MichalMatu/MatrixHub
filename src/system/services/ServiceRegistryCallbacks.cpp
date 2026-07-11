#include "ServiceRegistry.h"

#include "ServiceRegistryApi.h"
#include "../../alarms/AlarmService.h"
#include "../../alarms/types/AlarmInputData.h"
#include "../../ble/settings/BleSettingsService.h"
#include "../../gpio/GpioService.h"
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
        _shellyService->setOnConfigChangeCallback(nullptr);
        _shellyService->setOnStateChangeCallback(nullptr);
    }

    if (_gpioService) {
        _gpioService->setInputChangeCallback(nullptr);
    }

    if (_csiService) {
        _csiService->setCsiCallback(nullptr);
        _csiService->setMotionCallback(nullptr);
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

bool ServiceRegistry::wireCsiAlarmCallback() {
    if (!_csiService || !_alarmService) {
        return false;
    }

    // This bridge is installed before persisted CSI settings can activate the
    // producer. Otherwise a complete motion pulse during the rest of boot would
    // collapse to the latest level and never reach AlarmService's edge latch.
    const bool retainedMotion =
        _alarmService->isSourceTriggered(ALARMS::AlarmSource::WifiCsiMotion);
    if (!_csiService->prepareMotionCallbackBootstrap(retainedMotion)) {
        return false;
    }
    _csiService->setMotionCallback([this](bool motion) {
        if (_isDying.load(std::memory_order_acquire) || !_alarmService) {
            return;
        }
        ALARMS::AlarmInputData input;
        input.wifiCsiMotion = motion ? 1.0f : 0.0f;
        (void)_alarmService->submitInput(input);
    });
    return true;
}

bool ServiceRegistry::wireGpioAlarmCallback() {
    if (!_gpioService || !_alarmService) {
        return false;
    }

    // Install the bridge before GpioService::begin() publishes its initial
    // levels and starts the sampler, so no boot-time edge can be lost.
    return _gpioService->setInputChangeCallback(
        [this](const char* gpioId, bool logicalValue) {
            if (_isDying.load(std::memory_order_acquire) || !_alarmService) {
                return;
            }
            (void)_alarmService->submitGpioInput(gpioId, logicalValue);
        });
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

    LOGI("[Registry] Runtime callbacks wired");
}
