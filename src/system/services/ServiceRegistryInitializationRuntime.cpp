#include "ServiceRegistryInitializationRuntime.h"

#include "ServiceRegistryApi.h"
#include "../../api/matrix/MatrixApiService.h"
#include "../../alarms/AlarmSettingsService.h"
#include "../../ble/settings/BleSettingsService.h"
#include "../../compensation/CompensationSettingsService.h"
#include "../../matrix/MatrixSettingsService.h"
#include "../../notifications/settings/NotificationSettingsService.h"
#include "../../udp/UdpPusher.h"
#include "../../udp/UdpSettingsService.h"
#include "../../wifisensing/WifiSensingSettings.h"
#include "../health/heartbeat/HeartbeatSettingsService.h"
#include "../power/PowerSettingsService.h"
#include "../../matrix/menu/MatrixMenuService.h"

namespace SERVICE_REGISTRY_INIT_RUNTIME {

// Keep these wrappers behaviorally trivial. Their job is to expose a test seam
// for orchestration code, not to add another policy layer on top of services.

std::unique_ptr<POWER::PowerSettingsService> createPowerSettings(
    std::function<bool(const RTC::PowerData&)> applyConfig) {
    return std::make_unique<POWER::PowerSettingsService>(std::move(applyConfig));
}

std::unique_ptr<SYSTEM::HeartbeatSettingsService> createHeartbeatSettings(
    FS* fs,
    std::function<void()> onConfigApplied) {
    return std::make_unique<SYSTEM::HeartbeatSettingsService>(fs, std::move(onConfigApplied));
}

std::unique_ptr<WIFISENSING::WifiSensingSettings> createWifiSensingSettings(
    FS* fs,
    WIFISENSING::WifiSensingService* service,
    WIFISENSING::CSI::CsiService* csiService) {
    return std::make_unique<WIFISENSING::WifiSensingSettings>(fs, service, csiService);
}

void beginWifiSensingSettings(WIFISENSING::WifiSensingSettings* settings) {
    if (settings) {
        settings->begin();
    }
}

std::unique_ptr<NotificationSettingsService> createNotificationSettings(
    PsychicHttpServer* server,
    FS* fs,
    SecurityManager* securityManager) {
    return std::make_unique<NotificationSettingsService>(server, fs, securityManager);
}

void beginNotificationSettings(NotificationSettingsService* settings) {
    if (settings) {
        settings->begin();
    }
}

std::unique_ptr<COMPENSATION::CompensationSettingsService> createCompensationSettings(
    FS* fs,
    COMPENSATION::CompensationService* service) {
    return std::make_unique<COMPENSATION::CompensationSettingsService>(fs, service);
}

std::unique_ptr<UDPPUSH::UdpPusher> createUdpPusher() {
    return std::make_unique<UDPPUSH::UdpPusher>();
}

std::unique_ptr<UDPPUSH::UdpSettingsService> createUdpSettings(FS* fs) {
    return std::make_unique<UDPPUSH::UdpSettingsService>(fs);
}

std::unique_ptr<ALARMS::AlarmSettingsService> createAlarmSettings(
    FS* fs,
    std::function<bool(const ALARMS::AlarmRulesSnapshot&)> applyRules) {
    return std::make_unique<ALARMS::AlarmSettingsService>(fs, std::move(applyRules));
}

std::unique_ptr<BLE::BleSettingsService> createBleSettings(
    PsychicHttpServer* server,
    FS* fs,
    SecurityManager* securityManager) {
    return std::make_unique<BLE::BleSettingsService>(server, fs, securityManager);
}

void beginBleSettings(BLE::BleSettingsService* settings) {
    if (settings) {
        settings->begin();
    }
}

std::unique_ptr<MATRIX::MatrixMenuService> createMatrixMenu(
    MatrixService* matrixService,
    MATRIX_MANAGER::MatrixManagerService* matrixManager) {
    return std::make_unique<MATRIX::MatrixMenuService>(matrixService, matrixManager);
}

MATRIX::MatrixSettingsService* initMatrixSettings(
    SYSTEM::PsramStaticService<MATRIX::MatrixSettingsService>& storage,
    fs::FS* fs,
    MatrixService* matrixService,
    ALARMS::AlarmService* alarmService,
    MATRIX_MANAGER::MatrixManagerService* matrixManager,
    MATRIX::MatrixMenuService* menuService) {
    return storage.init(fs, matrixService, alarmService, matrixManager, menuService);
}

void beginMatrixSettings(MATRIX::MatrixSettingsService* settings) {
    if (settings) {
        settings->begin();
    }
}

API::MatrixApiService* initMatrixApi(
    ApiServices* api,
    PsychicHttpServer* server,
    SecurityManager* securityManager,
    POWER::PowerManager* powerManager,
    MATRIX::MatrixSettingsService* matrixSettings,
    WIFISENSING::CSI::CsiService* csiService) {
    // Matrix API still lives inside ApiServices storage, but routing init
    // through here keeps the init/begin hand-off visible in tests and logs.
    return api ? api->matrixApi.init(server, securityManager, powerManager, matrixSettings, csiService) : nullptr;
}

void beginMatrixApi(API::MatrixApiService* api) {
    if (api) {
        api->begin();
    }
}

}  // namespace SERVICE_REGISTRY_INIT_RUNTIME
