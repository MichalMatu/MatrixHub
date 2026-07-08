#include "ServiceRegistry.h"

#include "ServiceRegistryInitializationRuntime.h"
#include "../../alarms/wiring/AlarmShellyBridge.h"
#include "../../alarms/AlarmService.h"
#include "../../alarms/AlarmSettingsService.h"
#include "../../gpio/GpioService.h"
#include "../../shelly/ShellyRuntimeControl.h"
#include "../../udp/UdpPusher.h"
#include "../health/heap/HeapMonitor.h"
#include "../init/services/ApiServicesInitializer.h"
#include "../init/services/BleServicesInitializer.h"
#include "../init/services/CoreServicesInitializer.h"
#include "../init/services/ImuServicesInitializer.h"
#include "../init/services/InteractionServicesInitializer.h"
#include "../init/services/NotificationServicesInitializer.h"
#include "../../matrix/menu/MatrixMenuService.h"
#include "../watchdog/TaskWatchdog.h"
#include "../../keyboard/KeyboardSettingsService.h"
#include "../../airmouse/AirMouseService.h"
#include "../../airmouse/AirMouseSettingsService.h"
#include "../../ble/settings/BleSettingsService.h"
#include "../../compensation/CompensationSettingsService.h"
#include "../../macros/MacroSettingsService.h"
#include "../../notifications/settings/NotificationSettingsService.h"
#include "../../system/health/heartbeat/Heartbeat.h"
#include "../../system/health/heartbeat/HeartbeatSettingsService.h"
#include "../../system/power/PowerManager.h"
#include "../../system/power/PowerSettingsService.h"
#include "../../udp/UdpSettingsService.h"
#include "../../usb_terminal/UsbTerminalSettingsService.h"
#include "../../wifisensing/WifiSensingSettings.h"

void ServiceRegistry::initializeCoreServices() {
    CoreServicesInitializer::initialize(
        _compensationService,
        _bleService,
        _alarmService,
        _sensorService,
        _csiService,
        _wifiSensingService,
        _matrixManager.get());
}

void ServiceRegistry::initializeBusinessServices(SemaphoreHandle_t networkMutex) {
    // Creation/begin calls below go through small runtime helpers on purpose.
    // That keeps ServiceRegistry focused on orchestration and gives native
    // tests one narrow seam to verify ctor/begin ordering when boot wiring
    // changes. For startup regressions, trace this method first, then the
    // matching SERVICE_REGISTRY_INIT_RUNTIME helper.
    // Small follow-up cleanup: the remaining simple config services also live
    // under registry ownership so their APIs stay transport-only.
    if (!_powerSettings) {
        _powerSettings = SERVICE_REGISTRY_INIT_RUNTIME::createPowerSettings(
            [this](const RTC::PowerData& config) {
                return _powerManager ? _powerManager->applyConfig(config) : false;
            });
    }

    if (!_heartbeatSettings) {
        _heartbeatSettings = SERVICE_REGISTRY_INIT_RUNTIME::createHeartbeatSettings(
            _framework->getFS(),
            [this]() {
                if (_isDying.load(std::memory_order_acquire)) {
                    return;
                }
                SYSTEM::Heartbeat::checkState();
            });
    }

    // Final cleanup: Shelly now follows registry ownership too, so the whole
    // backend service graph shares one lifecycle model.
    if (!_shellyService) {
        SHELLY::ensureOwnedService(_shellyService, *_framework->getFS(), networkMutex);
        SHELLY::loadConfig(_shellyService.get());
        if (SHELLY::deviceCount(_shellyService.get()) > 0) {
            SHELLY::beginService(_shellyService.get());
        }
    }

    // Step 4A of lifecycle cleanup: keep WiFi sensing config under registry
    // ownership so the settings service follows the same boot/shutdown model
    // as the rest of the long-lived config services.
    if (!_wifiSensingSettings) {
        _wifiSensingSettings = SERVICE_REGISTRY_INIT_RUNTIME::createWifiSensingSettings(
            _framework->getFS(), _wifiSensingService.get(), _csiService.get());
    }
    SERVICE_REGISTRY_INIT_RUNTIME::beginWifiSensingSettings(_wifiSensingSettings.get());

    // Step 4B of lifecycle cleanup: notification settings are also owned here
    // so the config service, runtime worker, and registry callbacks share a
    // single lifetime instead of being split across two ownership models.
    if (!_notificationSettings) {
        _notificationSettings = SERVICE_REGISTRY_INIT_RUNTIME::createNotificationSettings(
            _framework->getServer(),
            _framework->getFS(),
            _framework->getSecurityManager());
    }
    SERVICE_REGISTRY_INIT_RUNTIME::beginNotificationSettings(_notificationSettings.get());

    if (_alarmService) {
        _alarmService->setShellyActionExecutor(
            ALARMS::AlarmShellyBridge::build(_shellyService.get()));
    }

    // Continue the registry-ownership cleanup for lightweight RTC-backed
    // config services so their APIs become transport-only adapters.
    if (!_compensationSettings) {
        _compensationSettings = SERVICE_REGISTRY_INIT_RUNTIME::createCompensationSettings(
            _framework->getFS(), _compensationService.get());
    }

    if (!_udpPusher) {
        // UdpPusher used to hide behind a singleton. Keep it registry-owned so
        // boot, API injection, and shutdown all talk to the same explicit
        // instance and any future restart/order bug has one owner to inspect.
        _udpPusher = SERVICE_REGISTRY_INIT_RUNTIME::createUdpPusher();
    }

    if (!_udpSettings) {
        _udpSettings = SERVICE_REGISTRY_INIT_RUNTIME::createUdpSettings(_framework->getFS());
    }

    if (!_alarmSettings) {
        // AlarmSettingsService stays close to AlarmService because save/apply is
        // transactional and rollback-sensitive. The API should only serialize
        // requests around this shared settings object.
        _alarmSettings = SERVICE_REGISTRY_INIT_RUNTIME::createAlarmSettings(
            _framework->getFS(),
            [this](const ALARMS::AlarmRulesSnapshot& rules) {
                return _alarmService
                    ? _alarmService->updateRules(rules.rules, rules.ruleCount)
                    : false;
            });
    }

    if (!_gpioService) {
        _gpioService = std::make_unique<GPIO::GpioService>(_framework->getFS());
        _gpioService->begin();
    }

    if (_alarmService) {
        _alarmService->setGpioService(_gpioService.get());
    }
}

void ServiceRegistry::initializeBleServices() {
    // Step 4C of lifecycle cleanup: BLE settings now follow registry ownership
    // and expose detachable runtime hooks so shutdown order becomes explicit.
    if (!_bleSettings) {
        _bleSettings = SERVICE_REGISTRY_INIT_RUNTIME::createBleSettings(
            _server,
            _framework->getFS(),
            _framework->getSecurityManager());
    }
    SERVICE_REGISTRY_INIT_RUNTIME::beginBleSettings(_bleSettings.get());

    BleServicesInitializer::initialize(
        _bleSettings.get(), _bleService.get(), &_isDying, _bleWhitelistSettingsHandlerId);
}

void ServiceRegistry::initializeMatrixServices() {
    _matrixMenu = SERVICE_REGISTRY_INIT_RUNTIME::createMatrixMenu(_matrixService.get(),
                                                                  _matrixManager.get());

    auto* matrixSettings = SERVICE_REGISTRY_INIT_RUNTIME::initMatrixSettings(_matrixSettings,
                                                                             _framework->getFS(),
                                                                             _matrixService.get(),
                                                                             _alarmService.get(),
                                                                             _matrixManager.get(),
                                                                             _matrixMenu.get());
    SERVICE_REGISTRY_INIT_RUNTIME::beginMatrixSettings(matrixSettings);

    auto* matrixApi = SERVICE_REGISTRY_INIT_RUNTIME::initMatrixApi(_api.get(),
                                                                   _server,
                                                                   _framework->getSecurityManager(),
                                                                   _powerManager.get(),
                                                                   _matrixSettings.get(),
                                                                   _matrixService.get(),
                                                                   _csiService.get());
    SERVICE_REGISTRY_INIT_RUNTIME::beginMatrixApi(matrixApi);
}

void ServiceRegistry::initializeImuServices() {
    ImuServicesInitializer::initialize({
        _imuService,
        _imuManager,
        _imuRuntimeService,
        _alarmService.get(),
        _framework ? _framework->getFS() : nullptr,
    });
}

void ServiceRegistry::initializeInteractionServices() {
    InteractionServicesInitializer::initialize(
        {
            _keyboardService,
            _imuService,
            _imuManager,
            _airMouseService,
            _macroService,
            _usbTerminalService,
        },
        {
            _matrixManager.get(),
            _fsMutex,
            &SYSTEM::TaskWatchdog::instance(),
        });

    // Part 1 of API/settings lifecycle cleanup: keep keyboard settings under
    // registry ownership so API only exposes endpoints instead of owning
    // persistent config state directly.
    if (!_keyboardSettings) {
        _keyboardSettings = std::make_unique<KEYBOARD::KeyboardSettingsService>(_framework->getFS());
    }

    // AirMouse still needs a live apply callback, but the config service itself
    // should follow registry ownership just like keyboard settings.
    if (!_airMouseSettings) {
        _airMouseSettings = std::make_unique<AIRMOUSE::AirMouseSettingsService>(
            _framework->getFS(),
            [this]() {
                if (_isDying.load(std::memory_order_acquire)) {
                    return;
                }
                if (_airMouseService) {
                    _airMouseService->applySettings();
                }
            });
    }

    if (!_macroSettings) {
        // Macro settings drive boot-time ownership and live apply behavior, so
        // registry ownership makes restart/apply debugging much clearer than
        // rebuilding this service ad-hoc inside the API wrapper.
        _macroSettings =
            std::make_unique<MACROS::MacroSettingsService>(_framework->getFS(), _macroService.get());
    }

    if (!_usbTerminalSettings) {
        _usbTerminalSettings = std::make_unique<USB_TERMINAL::UsbTerminalSettingsService>(
            _framework->getFS());
    }
}

void ServiceRegistry::initializeNotificationServices(SemaphoreHandle_t notifMutex) {
    NotificationServicesInitializer::initialize(
        {
            {
                _notifications.telegramClient,
                _notifications.telegramQueue,
                _notifications.telegramCommandRuntime,
                _notifications.telegramWorker,
                _notifications.telegramNotifier,
            },
            {
                _notifications.webhookTransport,
                _notifications.webhookWorker,
                _notifications.webhookNotifier,
            },
            {
                _notifications.pushoverTransport,
                _notifications.pushoverWorker,
                _notifications.pushoverNotifier,
            },
            _notifications.runtimeWorker,
            {
                _notifications.telegramTestSender,
                _notifications.webhookTestSender,
                _notifications.pushoverTestSender,
            },
            _notifications.runtimeCancelToken,
        },
        {
            _notificationSettings.get(),
            _alarmService.get(),
            _macroService.get(),
            _matrixManager.get(),
            _framework->getSecurityManager(),
            _usbTerminalService.get(),
            _shellyService.get(),
            this,
            notifMutex,
        });
}

void ServiceRegistry::initializeApiServices() {
    // API boot is split out because the fragile part here is dependency
    // forwarding and the begin-order for registry-owned APIs, not the endpoint
    // handlers themselves. If macro/airmouse APIs stop coming up, debug this
    // wrapper and SERVICE_REGISTRY_INIT_RUNTIME::initializeApiServices() first.
    // Build explicit dependency bundles before crossing into the runtime helper
    // so call-site drift is visible in one block instead of a 30+ argument list.
    // If a future refactor forwards the wrong registry service, this assembly is
    // the first place to diff against SERVICE_REGISTRY_INIT_RUNTIME::Shared...
    SERVICE_REGISTRY_INIT_RUNTIME::SharedApiServicesDeps sharedDeps;
    sharedDeps.server = _server;
    sharedDeps.framework = _framework;
    sharedDeps.powerManager = _powerManager.get();
    sharedDeps.powerSettings = _powerSettings.get();
    sharedDeps.shellyService = _shellyService.get();
    sharedDeps.wifiSensingSettings = _wifiSensingSettings.get();
    sharedDeps.bleSettings = _bleSettings.get();
    sharedDeps.compensationSettings = _compensationSettings.get();
    sharedDeps.alarmService = _alarmService.get();
    sharedDeps.alarmSettings = _alarmSettings.get();
    sharedDeps.gpioService = _gpioService.get();
    sharedDeps.csiService = _csiService.get();
    sharedDeps.wifiSensingService = _wifiSensingService.get();
    sharedDeps.keyboardService = _keyboardService.get();
    sharedDeps.keyboardSettingsService = _keyboardSettings.get();
    sharedDeps.bleService = _bleService.get();
    sharedDeps.macroService = _macroService.get();
    sharedDeps.airMouseService = _airMouseService.get();
    sharedDeps.usbTerminalService = _usbTerminalService.get();
    sharedDeps.telegramWorker = _notifications.telegramWorker.get();
    sharedDeps.telegramTestSender = _notifications.telegramTestSender.get();
    sharedDeps.webhookTestSender = _notifications.webhookTestSender.get();
    sharedDeps.pushoverTestSender = _notifications.pushoverTestSender.get();
    sharedDeps.udpPusher = _udpPusher.get();
    sharedDeps.udpSettings = _udpSettings.get();
    sharedDeps.heartbeatSettings = _heartbeatSettings.get();
    sharedDeps.usbTerminalSettings = _usbTerminalSettings.get();
    sharedDeps.heapMonitor = &SYSTEM::HeapMonitor::instance();
    sharedDeps.imuRuntimeService = _imuRuntimeService.get();
    sharedDeps.fsMutex = _fsMutex;

    // Phase-2 registry-owned APIs are split on purpose: their lifecycle has a
    // different failure mode than the shared transport wrappers above.
    SERVICE_REGISTRY_INIT_RUNTIME::OwnedApiServicesDeps ownedDeps;
    ownedDeps.macroService = _macroService.get();
    ownedDeps.macroSettings = _macroSettings.get();
    ownedDeps.airMouseService = _airMouseService.get();
    ownedDeps.airMouseSettings = _airMouseSettings.get();

    SERVICE_REGISTRY_INIT_RUNTIME::initializeApiServices(_api.get(), sharedDeps, ownedDeps);
}
