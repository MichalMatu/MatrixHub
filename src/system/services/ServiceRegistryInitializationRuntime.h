#pragma once

#include <functional>
#include <memory>

#include <Arduino.h>
#include <FS.h>

#include "../rtc/RtcConfig.h"
#include "../utils/StaticService.h"

struct ApiServices;
class MatrixService;
class NotificationSettingsService;
class SecurityManager;
class PsychicHttpServer;
class ESP32SvelteKit;

namespace POWER {
class PowerManager;
class PowerSettingsService;
}

namespace SYSTEM {
class HeartbeatSettingsService;
class HeapMonitor;
}

namespace SHELLY {
class ShellyService;
}

namespace WIFISENSING {
class WifiSensingSettings;
class WifiSensingService;
namespace CSI {
class CsiService;
}
}

namespace BLE {
class BleSettingsService;
class BleService;
}

namespace COMPENSATION {
class CompensationService;
class CompensationSettingsService;
}

namespace UDPPUSH {
class UdpPusher;
class UdpSettingsService;
}

namespace ALARMS {
class AlarmService;
class AlarmSettingsService;
}

namespace GPIO {
class GpioService;
}

namespace MATRIX {
class MatrixMenuService;
class MatrixSettingsService;
}

namespace MATRIX_MANAGER {
class MatrixManagerService;
}

namespace API {
class MatrixApiService;
class PushoverTestSender;
class TelegramTestSender;
class WebhookTestSender;
}

namespace KEYBOARD {
class KeyboardService;
class KeyboardSettingsService;
}

namespace MACROS {
class MacroService;
class MacroSettingsService;
}

namespace AIRMOUSE {
class AirMouseService;
class AirMouseSettingsService;
}

namespace USB_TERMINAL {
class UsbTerminalService;
class UsbTerminalSettingsService;
}

namespace IMU {
class ImuRuntimeService;
}

namespace TELEGRAM {
class TelegramWorker;
}

namespace SERVICE_REGISTRY_INIT_RUNTIME {

// These helpers are intentionally thin. They keep ServiceRegistry readable
// while giving host tests a narrow seam around ctor/init/begin ordering.
// When a service boots with the wrong dependency or in the wrong order, the
// debug path is usually ServiceRegistry::initialize* -> this namespace.

struct SharedApiServicesDeps {
    // Phase-1 transport/API wrappers still need a broad set of registry-owned
    // services. Grouping them here shrinks the fragile forwarded-argument list
    // without changing ownership. If an API comes up with the wrong dependency,
    // diff this struct population against ServiceRegistry::initializeApiServices().
    PsychicHttpServer* server = nullptr;
    ESP32SvelteKit* framework = nullptr;
    POWER::PowerManager* powerManager = nullptr;
    POWER::PowerSettingsService* powerSettings = nullptr;
    SHELLY::ShellyService* shellyService = nullptr;
    WIFISENSING::WifiSensingSettings* wifiSensingSettings = nullptr;
    BLE::BleSettingsService* bleSettings = nullptr;
    COMPENSATION::CompensationSettingsService* compensationSettings = nullptr;
    ALARMS::AlarmService* alarmService = nullptr;
    ALARMS::AlarmSettingsService* alarmSettings = nullptr;
    GPIO::GpioService* gpioService = nullptr;
    WIFISENSING::CSI::CsiService* csiService = nullptr;
    WIFISENSING::WifiSensingService* wifiSensingService = nullptr;
    KEYBOARD::KeyboardService* keyboardService = nullptr;
    KEYBOARD::KeyboardSettingsService* keyboardSettingsService = nullptr;
    BLE::BleService* bleService = nullptr;
    MACROS::MacroService* macroService = nullptr;
    AIRMOUSE::AirMouseService* airMouseService = nullptr;
    USB_TERMINAL::UsbTerminalService* usbTerminalService = nullptr;
    TELEGRAM::TelegramWorker* telegramWorker = nullptr;
    API::TelegramTestSender* telegramTestSender = nullptr;
    API::WebhookTestSender* webhookTestSender = nullptr;
    API::PushoverTestSender* pushoverTestSender = nullptr;
    UDPPUSH::UdpPusher* udpPusher = nullptr;
    UDPPUSH::UdpSettingsService* udpSettings = nullptr;
    SYSTEM::HeartbeatSettingsService* heartbeatSettings = nullptr;
    USB_TERMINAL::UsbTerminalSettingsService* usbTerminalSettings = nullptr;
    SYSTEM::HeapMonitor* heapMonitor = nullptr;
    IMU::ImuRuntimeService* imuRuntimeService = nullptr;
    SemaphoreHandle_t fsMutex = nullptr;
};

struct OwnedApiServicesDeps {
    // Macro/AirMouse are kept separate because Phase 2 has custom lifecycle
    // work (`begin`, plus AirMouse fs mutex wiring) that historically regressed
    // when buried in the larger initializer call. If only these APIs fail to
    // boot, inspect this owned-deps bundle and ServiceRegistryOwnedApiRuntime.
    MACROS::MacroService* macroService = nullptr;
    MACROS::MacroSettingsService* macroSettings = nullptr;
    AIRMOUSE::AirMouseService* airMouseService = nullptr;
    AIRMOUSE::AirMouseSettingsService* airMouseSettings = nullptr;
};

std::unique_ptr<POWER::PowerSettingsService> createPowerSettings(
    std::function<bool(const RTC::PowerData&)> applyConfig);
std::unique_ptr<SYSTEM::HeartbeatSettingsService> createHeartbeatSettings(
    FS* fs,
    std::function<void()> onConfigApplied);
std::unique_ptr<WIFISENSING::WifiSensingSettings> createWifiSensingSettings(
    FS* fs,
    WIFISENSING::WifiSensingService* service,
    WIFISENSING::CSI::CsiService* csiService);
void beginWifiSensingSettings(WIFISENSING::WifiSensingSettings* settings);
std::unique_ptr<NotificationSettingsService> createNotificationSettings(
    PsychicHttpServer* server,
    FS* fs,
    SecurityManager* securityManager);
void beginNotificationSettings(NotificationSettingsService* settings);
std::unique_ptr<COMPENSATION::CompensationSettingsService> createCompensationSettings(
    FS* fs,
    COMPENSATION::CompensationService* service);
std::unique_ptr<UDPPUSH::UdpPusher> createUdpPusher();
std::unique_ptr<UDPPUSH::UdpSettingsService> createUdpSettings(FS* fs);
std::unique_ptr<ALARMS::AlarmSettingsService> createAlarmSettings(
    FS* fs,
    std::function<bool(const ALARMS::AlarmRulesSnapshot&)> applyRules);
std::unique_ptr<BLE::BleSettingsService> createBleSettings(
    PsychicHttpServer* server,
    FS* fs,
    SecurityManager* securityManager);
void beginBleSettings(BLE::BleSettingsService* settings);
std::unique_ptr<MATRIX::MatrixMenuService> createMatrixMenu(
    MatrixService* matrixService,
    MATRIX_MANAGER::MatrixManagerService* matrixManager);
MATRIX::MatrixSettingsService* initMatrixSettings(
    SYSTEM::PsramStaticService<MATRIX::MatrixSettingsService>& storage,
    fs::FS* fs,
    MatrixService* matrixService,
    ALARMS::AlarmService* alarmService,
    MATRIX_MANAGER::MatrixManagerService* matrixManager,
    MATRIX::MatrixMenuService* menuService);
void beginMatrixSettings(MATRIX::MatrixSettingsService* settings);
API::MatrixApiService* initMatrixApi(
    ApiServices* api,
    PsychicHttpServer* server,
    SecurityManager* securityManager,
    POWER::PowerManager* powerManager,
    MATRIX::MatrixSettingsService* matrixSettings,
    WIFISENSING::CSI::CsiService* csiService);
void beginMatrixApi(API::MatrixApiService* api);
// Kept separate from ApiServicesInitializer because macro/airmouse regressions
// were hidden in forwarded arguments and begin sequencing, so we want one
// stable place to inspect and test that wiring.
void initializeApiServices(
    ApiServices* api,
    const SharedApiServicesDeps& sharedDeps,
    const OwnedApiServicesDeps& ownedDeps);

}  // namespace SERVICE_REGISTRY_INIT_RUNTIME
