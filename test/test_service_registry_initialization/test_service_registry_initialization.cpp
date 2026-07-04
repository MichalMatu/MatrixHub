#include <unity.h>

#include <cstdarg>
#include <cstring>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "../../src/system/power/PowerManager.h"
#include "../../src/wifisensing/csi/core/CsiService.h"

namespace SHELLY {
class ShellyService {};
}

#define private public
#include "../../src/system/services/ServiceRegistryInitialization.cpp"
#undef private

namespace TEST_SERVICE_REGISTRY_INIT {

struct CallLog {
    std::vector<std::string> entries;

    void clear() {
        entries.clear();
    }

    void push(const char* entry) {
        entries.emplace_back(entry);
    }
};

inline CallLog calls;

inline int coreInitializeCalls = 0;
inline void* coreCompensationRef = nullptr;
inline void* coreBleRef = nullptr;
inline void* coreAlarmRef = nullptr;
inline void* coreSensorRef = nullptr;
inline void* coreCsiRef = nullptr;
inline void* coreWifiSensingRef = nullptr;
inline MATRIX_MANAGER::MatrixManagerService* coreMatrixManager = nullptr;

inline int createPowerSettingsCalls = 0;
inline std::function<bool(const RTC::PowerData&)> powerSettingsApplyCallback;
inline int powerManagerApplyCalls = 0;
inline bool powerManagerApplyResult = true;
inline RTC::PowerData lastPowerConfig{};

inline int createHeartbeatSettingsCalls = 0;
inline FS* lastHeartbeatFs = nullptr;
inline std::function<void()> heartbeatConfigAppliedCallback;
inline int heartbeatCheckStateCalls = 0;

inline int createShellyServiceCalls = 0;
inline FS* lastShellyFs = nullptr;
inline SemaphoreHandle_t lastShellyMutex = nullptr;
inline int shellyLoadConfigCalls = 0;
inline int shellyBeginCalls = 0;
inline size_t shellyDeviceCountValue = 0;
alignas(SHELLY::ShellyService) inline unsigned char shellyStorage[sizeof(SHELLY::ShellyService)];

inline int createGpioServiceCalls = 0;
inline int beginGpioServiceCalls = 0;
inline FS* lastGpioFs = nullptr;

inline int createWifiSensingSettingsCalls = 0;
inline int beginWifiSensingSettingsCalls = 0;
inline FS* lastWifiSensingFs = nullptr;
inline WIFISENSING::WifiSensingService* lastWifiSensingService = nullptr;
inline WIFISENSING::CSI::CsiService* lastWifiSensingCsiService = nullptr;

inline int createNotificationSettingsCalls = 0;
inline int beginNotificationSettingsCalls = 0;
inline PsychicHttpServer* lastNotificationServer = nullptr;
inline FS* lastNotificationFs = nullptr;
inline SecurityManager* lastNotificationSecurity = nullptr;

inline int alarmShellyBridgeBuildCalls = 0;
inline SHELLY::ShellyService* lastAlarmShellyService = nullptr;
inline int setShellyExecutorCalls = 0;
inline ALARMS::ShellyActionExecutor storedShellyExecutor;

inline int createCompensationSettingsCalls = 0;
inline FS* lastCompensationFs = nullptr;
inline COMPENSATION::CompensationService* lastCompensationService = nullptr;

inline int createUdpPusherCalls = 0;
inline int createUdpSettingsCalls = 0;
inline FS* lastUdpSettingsFs = nullptr;

inline int createAlarmSettingsCalls = 0;
inline FS* lastAlarmSettingsFs = nullptr;
inline std::function<bool(const RTC::AlarmRulesData&)> alarmSettingsApplyCallback;
inline int alarmUpdateRulesCalls = 0;
inline uint8_t lastAlarmUpdateRulesCount = 0;

inline int createBleSettingsCalls = 0;
inline int beginBleSettingsCalls = 0;
inline PsychicHttpServer* lastBleServer = nullptr;
inline FS* lastBleFs = nullptr;
inline SecurityManager* lastBleSecurity = nullptr;
inline int bleInitializerCalls = 0;
inline BLE::BleSettingsService* lastBleInitializerSettings = nullptr;
inline BLE::BleService* lastBleInitializerService = nullptr;
inline const std::atomic<bool>* lastBleInitializerIsDying = nullptr;
inline std::size_t bleInitializerAssignedHandlerId = 0;

inline int createMatrixMenuCalls = 0;
inline MatrixService* lastMatrixMenuMatrixService = nullptr;
inline MATRIX_MANAGER::MatrixManagerService* lastMatrixMenuManager = nullptr;
inline int initMatrixSettingsCalls = 0;
inline fs::FS* lastMatrixSettingsFs = nullptr;
inline MatrixService* lastMatrixSettingsMatrixService = nullptr;
inline ALARMS::AlarmService* lastMatrixSettingsAlarmService = nullptr;
inline MATRIX_MANAGER::MatrixManagerService* lastMatrixSettingsManager = nullptr;
inline MATRIX::MatrixMenuService* lastMatrixSettingsMenu = nullptr;
inline int beginMatrixSettingsCalls = 0;
inline int initMatrixApiCalls = 0;
inline PsychicHttpServer* lastMatrixApiServer = nullptr;
inline SecurityManager* lastMatrixApiSecurity = nullptr;
inline POWER::PowerManager* lastMatrixApiPowerManager = nullptr;
inline MATRIX::MatrixSettingsService* lastMatrixApiSettings = nullptr;
inline WIFISENSING::CSI::CsiService* lastMatrixApiCsiService = nullptr;
inline int beginMatrixApiCalls = 0;
inline API::MatrixApiService* fakeMatrixApiPtr = reinterpret_cast<API::MatrixApiService*>(0x42424242);

inline int initializeApiServicesCalls = 0;
inline ApiServices* lastApiServicesApi = nullptr;
inline PsychicHttpServer* lastApiServicesServer = nullptr;
inline ESP32SvelteKit* lastApiServicesFramework = nullptr;
inline POWER::PowerManager* lastApiServicesPowerManager = nullptr;
inline POWER::PowerSettingsService* lastApiServicesPowerSettings = nullptr;
inline SHELLY::ShellyService* lastApiServicesShelly = nullptr;
inline WIFISENSING::WifiSensingSettings* lastApiServicesWifiSensingSettings = nullptr;
inline BLE::BleSettingsService* lastApiServicesBleSettings = nullptr;
inline COMPENSATION::CompensationSettingsService* lastApiServicesCompensationSettings = nullptr;
inline ALARMS::AlarmService* lastApiServicesAlarmService = nullptr;
inline ALARMS::AlarmSettingsService* lastApiServicesAlarmSettings = nullptr;
inline GPIO::GpioService* lastApiServicesGpioService = nullptr;
inline WIFISENSING::CSI::CsiService* lastApiServicesCsiService = nullptr;
inline WIFISENSING::WifiSensingService* lastApiServicesWifiSensingService = nullptr;
inline KEYBOARD::KeyboardService* lastApiServicesKeyboardService = nullptr;
inline KEYBOARD::KeyboardSettingsService* lastApiServicesKeyboardSettings = nullptr;
inline BLE::BleService* lastApiServicesBleService = nullptr;
inline MACROS::MacroService* lastApiServicesMacroService = nullptr;
inline AIRMOUSE::AirMouseService* lastApiServicesAirMouseService = nullptr;
inline USB_TERMINAL::UsbTerminalService* lastApiServicesUsbTerminalService = nullptr;
inline TELEGRAM::TelegramWorker* lastApiServicesTelegramWorker = nullptr;
inline API::TelegramTestSender* lastApiServicesTelegramTestSender = nullptr;
inline API::WebhookTestSender* lastApiServicesWebhookTestSender = nullptr;
inline API::PushoverTestSender* lastApiServicesPushoverTestSender = nullptr;
inline UDPPUSH::UdpPusher* lastApiServicesUdpPusher = nullptr;
inline UDPPUSH::UdpSettingsService* lastApiServicesUdpSettings = nullptr;
inline SYSTEM::HeartbeatSettingsService* lastApiServicesHeartbeatSettings = nullptr;
inline USB_TERMINAL::UsbTerminalSettingsService* lastApiServicesUsbTerminalSettings = nullptr;
inline SYSTEM::HeapMonitor* lastApiServicesHeapMonitor = nullptr;
inline SemaphoreHandle_t lastApiServicesFsMutex = nullptr;
inline MACROS::MacroService* lastApiServicesMacroServiceForApi = nullptr;
inline MACROS::MacroSettingsService* lastApiServicesMacroSettings = nullptr;
inline AIRMOUSE::AirMouseService* lastApiServicesAirMouseServiceForApi = nullptr;
inline AIRMOUSE::AirMouseSettingsService* lastApiServicesAirMouseSettings = nullptr;

template <typename T>
unsigned char* rawStorage() {
    alignas(T) static unsigned char storage[sizeof(T)];
    return storage;
}

template <typename T>
T& rawObject() {
    return *reinterpret_cast<T*>(rawStorage<T>());
}

template <typename T>
T* fakeObjectPtr() {
    return reinterpret_cast<T*>(rawStorage<T>());
}

template <typename T>
void clearRawObject() {
    std::memset(rawStorage<T>(), 0, sizeof(T));
}

template <typename T>
void setUniquePtr(std::unique_ptr<T>& slot, T* ptr) {
    new (&slot) std::unique_ptr<T>(ptr);
}

void reset() {
    calls.clear();

    coreInitializeCalls = 0;
    coreCompensationRef = nullptr;
    coreBleRef = nullptr;
    coreAlarmRef = nullptr;
    coreSensorRef = nullptr;
    coreCsiRef = nullptr;
    coreWifiSensingRef = nullptr;
    coreMatrixManager = nullptr;

    createPowerSettingsCalls = 0;
    powerSettingsApplyCallback = {};
    powerManagerApplyCalls = 0;
    powerManagerApplyResult = true;
    lastPowerConfig = {};

    createHeartbeatSettingsCalls = 0;
    lastHeartbeatFs = nullptr;
    heartbeatConfigAppliedCallback = {};
    heartbeatCheckStateCalls = 0;

    createShellyServiceCalls = 0;
    lastShellyFs = nullptr;
    lastShellyMutex = nullptr;
    shellyLoadConfigCalls = 0;
    shellyBeginCalls = 0;
    shellyDeviceCountValue = 0;
    std::memset(shellyStorage, 0, sizeof(shellyStorage));

    createGpioServiceCalls = 0;
    beginGpioServiceCalls = 0;
    lastGpioFs = nullptr;

    createWifiSensingSettingsCalls = 0;
    beginWifiSensingSettingsCalls = 0;
    lastWifiSensingFs = nullptr;
    lastWifiSensingService = nullptr;
    lastWifiSensingCsiService = nullptr;

    createNotificationSettingsCalls = 0;
    beginNotificationSettingsCalls = 0;
    lastNotificationServer = nullptr;
    lastNotificationFs = nullptr;
    lastNotificationSecurity = nullptr;

    alarmShellyBridgeBuildCalls = 0;
    lastAlarmShellyService = nullptr;
    setShellyExecutorCalls = 0;
    storedShellyExecutor = {};

    createCompensationSettingsCalls = 0;
    lastCompensationFs = nullptr;
    lastCompensationService = nullptr;

    createUdpPusherCalls = 0;
    createUdpSettingsCalls = 0;
    lastUdpSettingsFs = nullptr;

    createAlarmSettingsCalls = 0;
    lastAlarmSettingsFs = nullptr;
    alarmSettingsApplyCallback = {};
    alarmUpdateRulesCalls = 0;
    lastAlarmUpdateRulesCount = 0;

    createBleSettingsCalls = 0;
    beginBleSettingsCalls = 0;
    lastBleServer = nullptr;
    lastBleFs = nullptr;
    lastBleSecurity = nullptr;
    bleInitializerCalls = 0;
    lastBleInitializerSettings = nullptr;
    lastBleInitializerService = nullptr;
    lastBleInitializerIsDying = nullptr;
    bleInitializerAssignedHandlerId = 0;

    createMatrixMenuCalls = 0;
    lastMatrixMenuMatrixService = nullptr;
    lastMatrixMenuManager = nullptr;
    initMatrixSettingsCalls = 0;
    lastMatrixSettingsFs = nullptr;
    lastMatrixSettingsMatrixService = nullptr;
    lastMatrixSettingsAlarmService = nullptr;
    lastMatrixSettingsManager = nullptr;
    lastMatrixSettingsMenu = nullptr;
    beginMatrixSettingsCalls = 0;
    initMatrixApiCalls = 0;
    lastMatrixApiServer = nullptr;
    lastMatrixApiSecurity = nullptr;
    lastMatrixApiPowerManager = nullptr;
    lastMatrixApiSettings = nullptr;
    lastMatrixApiCsiService = nullptr;
    beginMatrixApiCalls = 0;
    fakeMatrixApiPtr = reinterpret_cast<API::MatrixApiService*>(0x42424242);

    initializeApiServicesCalls = 0;
    lastApiServicesApi = nullptr;
    lastApiServicesServer = nullptr;
    lastApiServicesFramework = nullptr;
    lastApiServicesPowerManager = nullptr;
    lastApiServicesPowerSettings = nullptr;
    lastApiServicesShelly = nullptr;
    lastApiServicesWifiSensingSettings = nullptr;
    lastApiServicesBleSettings = nullptr;
    lastApiServicesCompensationSettings = nullptr;
    lastApiServicesAlarmService = nullptr;
    lastApiServicesAlarmSettings = nullptr;
    lastApiServicesGpioService = nullptr;
    lastApiServicesCsiService = nullptr;
    lastApiServicesWifiSensingService = nullptr;
    lastApiServicesKeyboardService = nullptr;
    lastApiServicesKeyboardSettings = nullptr;
    lastApiServicesBleService = nullptr;
    lastApiServicesMacroService = nullptr;
    lastApiServicesAirMouseService = nullptr;
    lastApiServicesUsbTerminalService = nullptr;
    lastApiServicesTelegramWorker = nullptr;
    lastApiServicesTelegramTestSender = nullptr;
    lastApiServicesWebhookTestSender = nullptr;
    lastApiServicesPushoverTestSender = nullptr;
    lastApiServicesUdpPusher = nullptr;
    lastApiServicesUdpSettings = nullptr;
    lastApiServicesHeartbeatSettings = nullptr;
    lastApiServicesUsbTerminalSettings = nullptr;
    lastApiServicesHeapMonitor = nullptr;
    lastApiServicesFsMutex = nullptr;
    lastApiServicesMacroServiceForApi = nullptr;
    lastApiServicesMacroSettings = nullptr;
    lastApiServicesAirMouseServiceForApi = nullptr;
    lastApiServicesAirMouseSettings = nullptr;

    clearRawObject<ServiceRegistry>();
    clearRawObject<POWER::PowerManager>();
    clearRawObject<ALARMS::AlarmService>();
}

void bindFramework(ServiceRegistry& registry, PsychicHttpServer& server, ESP32SvelteKit& framework) {
    registry._server = &server;
    registry._framework = &framework;
    new (&registry._api) std::unique_ptr<ApiServices>(reinterpret_cast<ApiServices*>(0x1));
}

}  // namespace TEST_SERVICE_REGISTRY_INIT

namespace RTC {

ConfigStore mockStore{};

const ConfigStore& getConfig() {
    return mockStore;
}

ConfigStore& getMutableConfig() {
    return mockStore;
}

SemaphoreHandle_t getLock() {
    static SemaphoreHandle_t lock = xSemaphoreCreateMutex();
    return lock;
}

void markValid() {}

}  // namespace RTC

namespace LOG {

void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    (void)level;
    (void)tag;
    (void)fmt;
}

void Logging::logSection(const char* title) {
    (void)title;
}

void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {
    (void)taskName;
    (void)stackSize;
}

}  // namespace LOG

void CoreServicesInitializer::initialize(
    std::unique_ptr<COMPENSATION::CompensationService>& compensationService,
    std::unique_ptr<BLE::BleService>& bleService,
    std::unique_ptr<ALARMS::AlarmService>& alarmService,
    std::unique_ptr<SENSORS::Scd4xSensorService>& sensorService,
    std::unique_ptr<WIFISENSING::CSI::CsiService>& csiService,
    std::unique_ptr<WIFISENSING::WifiSensingService>& wifiSensingService,
    MATRIX_MANAGER::MatrixManagerService* matrixManager) {
    TEST_SERVICE_REGISTRY_INIT::coreInitializeCalls++;
    TEST_SERVICE_REGISTRY_INIT::coreCompensationRef = &compensationService;
    TEST_SERVICE_REGISTRY_INIT::coreBleRef = &bleService;
    TEST_SERVICE_REGISTRY_INIT::coreAlarmRef = &alarmService;
    TEST_SERVICE_REGISTRY_INIT::coreSensorRef = &sensorService;
    TEST_SERVICE_REGISTRY_INIT::coreCsiRef = &csiService;
    TEST_SERVICE_REGISTRY_INIT::coreWifiSensingRef = &wifiSensingService;
    TEST_SERVICE_REGISTRY_INIT::coreMatrixManager = matrixManager;
}

namespace SERVICE_REGISTRY_INIT_RUNTIME {

std::unique_ptr<POWER::PowerSettingsService> createPowerSettings(
    std::function<bool(const RTC::PowerData&)> applyConfig) {
    TEST_SERVICE_REGISTRY_INIT::createPowerSettingsCalls++;
    TEST_SERVICE_REGISTRY_INIT::powerSettingsApplyCallback = std::move(applyConfig);
    return std::unique_ptr<POWER::PowerSettingsService>(
        TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<POWER::PowerSettingsService>());
}

std::unique_ptr<SYSTEM::HeartbeatSettingsService> createHeartbeatSettings(
    FS* fs,
    std::function<void()> onConfigApplied) {
    TEST_SERVICE_REGISTRY_INIT::createHeartbeatSettingsCalls++;
    TEST_SERVICE_REGISTRY_INIT::lastHeartbeatFs = fs;
    TEST_SERVICE_REGISTRY_INIT::heartbeatConfigAppliedCallback = std::move(onConfigApplied);
    return std::unique_ptr<SYSTEM::HeartbeatSettingsService>(
        TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<SYSTEM::HeartbeatSettingsService>());
}

std::unique_ptr<WIFISENSING::WifiSensingSettings> createWifiSensingSettings(
    FS* fs,
    WIFISENSING::WifiSensingService* service,
    WIFISENSING::CSI::CsiService* csiService) {
    TEST_SERVICE_REGISTRY_INIT::createWifiSensingSettingsCalls++;
    TEST_SERVICE_REGISTRY_INIT::lastWifiSensingFs = fs;
    TEST_SERVICE_REGISTRY_INIT::lastWifiSensingService = service;
    TEST_SERVICE_REGISTRY_INIT::lastWifiSensingCsiService = csiService;
    return std::unique_ptr<WIFISENSING::WifiSensingSettings>(
        TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<WIFISENSING::WifiSensingSettings>());
}

void beginWifiSensingSettings(WIFISENSING::WifiSensingSettings* settings) {
    TEST_SERVICE_REGISTRY_INIT::beginWifiSensingSettingsCalls++;
    TEST_ASSERT_EQUAL_PTR(TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<WIFISENSING::WifiSensingSettings>(),
                          settings);
}

std::unique_ptr<NotificationSettingsService> createNotificationSettings(
    PsychicHttpServer* server,
    FS* fs,
    SecurityManager* securityManager) {
    TEST_SERVICE_REGISTRY_INIT::createNotificationSettingsCalls++;
    TEST_SERVICE_REGISTRY_INIT::lastNotificationServer = server;
    TEST_SERVICE_REGISTRY_INIT::lastNotificationFs = fs;
    TEST_SERVICE_REGISTRY_INIT::lastNotificationSecurity = securityManager;
    return std::unique_ptr<NotificationSettingsService>(
        TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<NotificationSettingsService>());
}

void beginNotificationSettings(NotificationSettingsService* settings) {
    TEST_SERVICE_REGISTRY_INIT::beginNotificationSettingsCalls++;
    TEST_ASSERT_EQUAL_PTR(TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<NotificationSettingsService>(),
                          settings);
}

std::unique_ptr<COMPENSATION::CompensationSettingsService> createCompensationSettings(
    FS* fs,
    COMPENSATION::CompensationService* service) {
    TEST_SERVICE_REGISTRY_INIT::createCompensationSettingsCalls++;
    TEST_SERVICE_REGISTRY_INIT::lastCompensationFs = fs;
    TEST_SERVICE_REGISTRY_INIT::lastCompensationService = service;
    return std::unique_ptr<COMPENSATION::CompensationSettingsService>(
        TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<COMPENSATION::CompensationSettingsService>());
}

std::unique_ptr<UDPPUSH::UdpPusher> createUdpPusher() {
    TEST_SERVICE_REGISTRY_INIT::createUdpPusherCalls++;
    return std::unique_ptr<UDPPUSH::UdpPusher>(
        TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<UDPPUSH::UdpPusher>());
}

std::unique_ptr<UDPPUSH::UdpSettingsService> createUdpSettings(FS* fs) {
    TEST_SERVICE_REGISTRY_INIT::createUdpSettingsCalls++;
    TEST_SERVICE_REGISTRY_INIT::lastUdpSettingsFs = fs;
    return std::unique_ptr<UDPPUSH::UdpSettingsService>(
        TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<UDPPUSH::UdpSettingsService>());
}

std::unique_ptr<ALARMS::AlarmSettingsService> createAlarmSettings(
    FS* fs,
    std::function<bool(const RTC::AlarmRulesData&)> applyRules) {
    TEST_SERVICE_REGISTRY_INIT::createAlarmSettingsCalls++;
    TEST_SERVICE_REGISTRY_INIT::lastAlarmSettingsFs = fs;
    TEST_SERVICE_REGISTRY_INIT::alarmSettingsApplyCallback = std::move(applyRules);
    return std::unique_ptr<ALARMS::AlarmSettingsService>(
        TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<ALARMS::AlarmSettingsService>());
}

std::unique_ptr<BLE::BleSettingsService> createBleSettings(
    PsychicHttpServer* server,
    FS* fs,
    SecurityManager* securityManager) {
    TEST_SERVICE_REGISTRY_INIT::createBleSettingsCalls++;
    TEST_SERVICE_REGISTRY_INIT::lastBleServer = server;
    TEST_SERVICE_REGISTRY_INIT::lastBleFs = fs;
    TEST_SERVICE_REGISTRY_INIT::lastBleSecurity = securityManager;
    return std::unique_ptr<BLE::BleSettingsService>(
        TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<BLE::BleSettingsService>());
}

void beginBleSettings(BLE::BleSettingsService* settings) {
    TEST_SERVICE_REGISTRY_INIT::beginBleSettingsCalls++;
    TEST_ASSERT_EQUAL_PTR(TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<BLE::BleSettingsService>(),
                          settings);
}

std::unique_ptr<MATRIX::MatrixMenuService> createMatrixMenu(
    MatrixService* matrixService,
    MATRIX_MANAGER::MatrixManagerService* matrixManager) {
    TEST_SERVICE_REGISTRY_INIT::calls.push("createMatrixMenu");
    TEST_SERVICE_REGISTRY_INIT::createMatrixMenuCalls++;
    TEST_SERVICE_REGISTRY_INIT::lastMatrixMenuMatrixService = matrixService;
    TEST_SERVICE_REGISTRY_INIT::lastMatrixMenuManager = matrixManager;
    return std::unique_ptr<MATRIX::MatrixMenuService>(
        TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<MATRIX::MatrixMenuService>());
}

MATRIX::MatrixSettingsService* initMatrixSettings(
    SYSTEM::PsramStaticService<MATRIX::MatrixSettingsService>& storage,
    fs::FS* fs,
    MatrixService* matrixService,
    ALARMS::AlarmService* alarmService,
    MATRIX_MANAGER::MatrixManagerService* matrixManager,
    MATRIX::MatrixMenuService* menuService) {
    TEST_SERVICE_REGISTRY_INIT::calls.push("initMatrixSettings");
    TEST_SERVICE_REGISTRY_INIT::initMatrixSettingsCalls++;
    TEST_SERVICE_REGISTRY_INIT::lastMatrixSettingsFs = fs;
    TEST_SERVICE_REGISTRY_INIT::lastMatrixSettingsMatrixService = matrixService;
    TEST_SERVICE_REGISTRY_INIT::lastMatrixSettingsAlarmService = alarmService;
    TEST_SERVICE_REGISTRY_INIT::lastMatrixSettingsManager = matrixManager;
    TEST_SERVICE_REGISTRY_INIT::lastMatrixSettingsMenu = menuService;
    storage._instance = TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<MATRIX::MatrixSettingsService>();
    return storage._instance;
}

void beginMatrixSettings(MATRIX::MatrixSettingsService* settings) {
    TEST_SERVICE_REGISTRY_INIT::calls.push("beginMatrixSettings");
    TEST_SERVICE_REGISTRY_INIT::beginMatrixSettingsCalls++;
    TEST_ASSERT_EQUAL_PTR(TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<MATRIX::MatrixSettingsService>(),
                          settings);
}

API::MatrixApiService* initMatrixApi(
    ApiServices* api,
    PsychicHttpServer* server,
    SecurityManager* securityManager,
    POWER::PowerManager* powerManager,
    MATRIX::MatrixSettingsService* matrixSettings,
    WIFISENSING::CSI::CsiService* csiService) {
    TEST_SERVICE_REGISTRY_INIT::calls.push("initMatrixApi");
    TEST_SERVICE_REGISTRY_INIT::initMatrixApiCalls++;
    TEST_ASSERT_NOT_NULL(api);
    TEST_SERVICE_REGISTRY_INIT::lastMatrixApiServer = server;
    TEST_SERVICE_REGISTRY_INIT::lastMatrixApiSecurity = securityManager;
    TEST_SERVICE_REGISTRY_INIT::lastMatrixApiPowerManager = powerManager;
    TEST_SERVICE_REGISTRY_INIT::lastMatrixApiSettings = matrixSettings;
    TEST_SERVICE_REGISTRY_INIT::lastMatrixApiCsiService = csiService;
    return TEST_SERVICE_REGISTRY_INIT::fakeMatrixApiPtr;
}

void beginMatrixApi(API::MatrixApiService* api) {
    TEST_SERVICE_REGISTRY_INIT::calls.push("beginMatrixApi");
    TEST_SERVICE_REGISTRY_INIT::beginMatrixApiCalls++;
    TEST_ASSERT_EQUAL_PTR(TEST_SERVICE_REGISTRY_INIT::fakeMatrixApiPtr, api);
}

void initializeApiServices(
    ApiServices* api,
    const SharedApiServicesDeps& sharedDeps,
    const OwnedApiServicesDeps& ownedDeps) {
    TEST_SERVICE_REGISTRY_INIT::initializeApiServicesCalls++;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesApi = api;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesServer = sharedDeps.server;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesFramework = sharedDeps.framework;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesPowerManager = sharedDeps.powerManager;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesPowerSettings = sharedDeps.powerSettings;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesShelly = sharedDeps.shellyService;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesWifiSensingSettings = sharedDeps.wifiSensingSettings;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesBleSettings = sharedDeps.bleSettings;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesCompensationSettings = sharedDeps.compensationSettings;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesAlarmService = sharedDeps.alarmService;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesAlarmSettings = sharedDeps.alarmSettings;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesGpioService = sharedDeps.gpioService;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesCsiService = sharedDeps.csiService;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesWifiSensingService = sharedDeps.wifiSensingService;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesKeyboardService = sharedDeps.keyboardService;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesKeyboardSettings = sharedDeps.keyboardSettingsService;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesBleService = sharedDeps.bleService;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesMacroService = sharedDeps.macroService;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesAirMouseService = sharedDeps.airMouseService;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesUsbTerminalService = sharedDeps.usbTerminalService;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesTelegramWorker = sharedDeps.telegramWorker;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesTelegramTestSender = sharedDeps.telegramTestSender;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesWebhookTestSender = sharedDeps.webhookTestSender;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesPushoverTestSender = sharedDeps.pushoverTestSender;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesUdpPusher = sharedDeps.udpPusher;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesUdpSettings = sharedDeps.udpSettings;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesHeartbeatSettings = sharedDeps.heartbeatSettings;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesUsbTerminalSettings = sharedDeps.usbTerminalSettings;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesHeapMonitor = sharedDeps.heapMonitor;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesFsMutex = sharedDeps.fsMutex;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesMacroServiceForApi = ownedDeps.macroService;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesMacroSettings = ownedDeps.macroSettings;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesAirMouseServiceForApi = ownedDeps.airMouseService;
    TEST_SERVICE_REGISTRY_INIT::lastApiServicesAirMouseSettings = ownedDeps.airMouseSettings;
}

}  // namespace SERVICE_REGISTRY_INIT_RUNTIME

namespace SHELLY {

void ensureOwnedService(std::unique_ptr<ShellyService>& slot,
                        FS& fs,
                        SemaphoreHandle_t networkMutex) {
    TEST_SERVICE_REGISTRY_INIT::createShellyServiceCalls++;
    TEST_SERVICE_REGISTRY_INIT::lastShellyFs = &fs;
    TEST_SERVICE_REGISTRY_INIT::lastShellyMutex = networkMutex;
    slot = std::unique_ptr<ShellyService>(
        reinterpret_cast<ShellyService*>(TEST_SERVICE_REGISTRY_INIT::shellyStorage));
}

void loadConfig(ShellyService* service) {
    (void)service;
    TEST_SERVICE_REGISTRY_INIT::shellyLoadConfigCalls++;
}

size_t deviceCount(const ShellyService* service) {
    (void)service;
    return TEST_SERVICE_REGISTRY_INIT::shellyDeviceCountValue;
}

void beginService(ShellyService* service) {
    (void)service;
    TEST_SERVICE_REGISTRY_INIT::shellyBeginCalls++;
}

}  // namespace SHELLY

namespace GPIO {

GpioChannelConfig::GpioChannelConfig()
    : pin(0),
      mode(GpioMode::Disabled),
      pull(GpioPull::None),
      inverted(false),
      debounceMs(kDefaultDebounceMs),
      initialOutput(false) {
    std::memset(id, 0, sizeof(id));
    std::memset(name, 0, sizeof(name));
}

GpioService::GpioService(FS* fs)
    : _fs(fs),
      _mutex(nullptr) {
    TEST_SERVICE_REGISTRY_INIT::createGpioServiceCalls++;
    TEST_SERVICE_REGISTRY_INIT::lastGpioFs = fs;
}

GpioService::~GpioService() = default;

bool GpioService::begin() {
    TEST_SERVICE_REGISTRY_INIT::beginGpioServiceCalls++;
    _running = true;
    return true;
}

}  // namespace GPIO

namespace ALARMS {

ShellyActionExecutor AlarmShellyBridge::build(SHELLY::ShellyService* shellyService) {
    TEST_SERVICE_REGISTRY_INIT::alarmShellyBridgeBuildCalls++;
    TEST_SERVICE_REGISTRY_INIT::lastAlarmShellyService = shellyService;
    return [](const AlarmRule& rule, bool state) -> uint8_t {
        (void)rule;
        (void)state;
        return 77;
    };
}

void AlarmService::setShellyActionExecutor(ShellyActionExecutor executor) {
    TEST_SERVICE_REGISTRY_INIT::setShellyExecutorCalls++;
    TEST_SERVICE_REGISTRY_INIT::storedShellyExecutor = std::move(executor);
}

bool AlarmService::updateRules(const AlarmRule* rules, uint8_t count) {
    (void)rules;
    TEST_SERVICE_REGISTRY_INIT::alarmUpdateRulesCalls++;
    TEST_SERVICE_REGISTRY_INIT::lastAlarmUpdateRulesCount = count;
    return true;
}

}  // namespace ALARMS

namespace POWER {

bool PowerManager::applyConfig(const RTC::PowerData& config) {
    TEST_SERVICE_REGISTRY_INIT::powerManagerApplyCalls++;
    TEST_SERVICE_REGISTRY_INIT::lastPowerConfig = config;
    return TEST_SERVICE_REGISTRY_INIT::powerManagerApplyResult;
}

}  // namespace POWER

namespace SYSTEM::HEARTBEAT_DETAIL {

HeartbeatWorker& heartbeatWorker() {
    return *reinterpret_cast<HeartbeatWorker*>(TEST_SERVICE_REGISTRY_INIT::rawStorage<HeartbeatWorker>());
}

void HeartbeatWorker::checkState() {
    TEST_SERVICE_REGISTRY_INIT::heartbeatCheckStateCalls++;
}

}  // namespace SYSTEM::HEARTBEAT_DETAIL

void ImuServicesInitializer::initialize(State state) {
    (void)state;
}

void InteractionServicesInitializer::initialize(const State& state, const Deps& deps) {
    (void)state;
    (void)deps;
}

void NotificationServicesInitializer::initialize(const State& state, const Deps& deps) {
    (void)state;
    (void)deps;
}

namespace KEYBOARD {

KeyboardSettingsService::KeyboardSettingsService(FS* fs)
    : RtcStatefulService(static_cast<RTC::KeyboardData RTC::ConfigStore::*>(nullptr)) {
    (void)fs;
}

}  // namespace KEYBOARD

namespace AIRMOUSE {

AirMouseSettingsService::AirMouseSettingsService(FS* fs, std::function<void()> onConfigApplied)
    : RtcStatefulService(static_cast<RTC::AirMouseData RTC::ConfigStore::*>(nullptr)) {
    (void)fs;
    (void)onConfigApplied;
}

void AirMouseService::applySettings() {}

}  // namespace AIRMOUSE

namespace MACROS {

MacroSettingsService::MacroSettingsService(FS* fs, MacroService* service)
    : RtcStatefulService(static_cast<RTC::MacroData RTC::ConfigStore::*>(nullptr)) {
    (void)fs;
    (void)service;
}

}  // namespace MACROS

namespace USB_TERMINAL {

UsbTerminalSettingsService::UsbTerminalSettingsService(FS* fs)
    : RtcStatefulService(static_cast<RTC::UsbTerminalData RTC::ConfigStore::*>(nullptr)) {
    (void)fs;
}

}  // namespace USB_TERMINAL

namespace UDPPUSH {

UdpPusher::~UdpPusher() = default;

}  // namespace UDPPUSH

void BleServicesInitializer::initialize(
    BLE::BleSettingsService* bleSettings,
    BLE::BleService* bleService,
    const std::atomic<bool>* isDying,
    std::size_t& whitelistHandlerId) {
    TEST_SERVICE_REGISTRY_INIT::bleInitializerCalls++;
    TEST_SERVICE_REGISTRY_INIT::lastBleInitializerSettings = bleSettings;
    TEST_SERVICE_REGISTRY_INIT::lastBleInitializerService = bleService;
    TEST_SERVICE_REGISTRY_INIT::lastBleInitializerIsDying = isDying;
    whitelistHandlerId = TEST_SERVICE_REGISTRY_INIT::bleInitializerAssignedHandlerId;
}

void setUp(void) {
    TEST_SERVICE_REGISTRY_INIT::reset();
}

void tearDown(void) {}

void test_initializeCoreServices_delegates_to_core_initializer_with_registry_refs() {
    auto& registry = TEST_SERVICE_REGISTRY_INIT::rawObject<ServiceRegistry>();
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._matrixManager,
        reinterpret_cast<MATRIX_MANAGER::MatrixManagerService*>(0x1234));

    registry.initializeCoreServices();

    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::coreInitializeCalls);
    TEST_ASSERT_EQUAL_PTR(&registry._compensationService, TEST_SERVICE_REGISTRY_INIT::coreCompensationRef);
    TEST_ASSERT_EQUAL_PTR(&registry._bleService, TEST_SERVICE_REGISTRY_INIT::coreBleRef);
    TEST_ASSERT_EQUAL_PTR(&registry._alarmService, TEST_SERVICE_REGISTRY_INIT::coreAlarmRef);
    TEST_ASSERT_EQUAL_PTR(&registry._sensorService, TEST_SERVICE_REGISTRY_INIT::coreSensorRef);
    TEST_ASSERT_EQUAL_PTR(&registry._csiService, TEST_SERVICE_REGISTRY_INIT::coreCsiRef);
    TEST_ASSERT_EQUAL_PTR(&registry._wifiSensingService, TEST_SERVICE_REGISTRY_INIT::coreWifiSensingRef);
    TEST_ASSERT_EQUAL_PTR(registry._matrixManager.get(), TEST_SERVICE_REGISTRY_INIT::coreMatrixManager);
}

void test_initializeBusinessServices_creates_owned_services_and_starts_shelly_when_configured() {
    PsychicHttpServer server;
    ESP32SvelteKit framework(&server);
    auto& registry = TEST_SERVICE_REGISTRY_INIT::rawObject<ServiceRegistry>();
    TEST_SERVICE_REGISTRY_INIT::bindFramework(registry, server, framework);
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(registry._powerManager,
                                             TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<POWER::PowerManager>());
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(registry._alarmService,
                                             TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<ALARMS::AlarmService>());
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(registry._csiService,
                                             TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<WIFISENSING::CSI::CsiService>());
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._wifiSensingService,
        reinterpret_cast<WIFISENSING::WifiSensingService*>(0x2222));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._compensationService,
        reinterpret_cast<COMPENSATION::CompensationService*>(0x3333));
    TEST_SERVICE_REGISTRY_INIT::shellyDeviceCountValue = 2;

    const auto networkMutex = reinterpret_cast<SemaphoreHandle_t>(0x44);
    registry.initializeBusinessServices(networkMutex);

    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createPowerSettingsCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createHeartbeatSettingsCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createShellyServiceCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createWifiSensingSettingsCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::beginWifiSensingSettingsCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createNotificationSettingsCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::beginNotificationSettingsCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createCompensationSettingsCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createUdpPusherCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createUdpSettingsCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createAlarmSettingsCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createGpioServiceCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::beginGpioServiceCalls);

    TEST_ASSERT_EQUAL_PTR(framework.getFS(), TEST_SERVICE_REGISTRY_INIT::lastShellyFs);
    TEST_ASSERT_EQUAL_PTR(networkMutex, TEST_SERVICE_REGISTRY_INIT::lastShellyMutex);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::shellyLoadConfigCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::shellyBeginCalls);
    TEST_ASSERT_EQUAL_PTR(framework.getFS(), TEST_SERVICE_REGISTRY_INIT::lastGpioFs);
    TEST_ASSERT_NOT_NULL(registry._gpioService.get());

    TEST_ASSERT_EQUAL_PTR(framework.getFS(), TEST_SERVICE_REGISTRY_INIT::lastWifiSensingFs);
    TEST_ASSERT_EQUAL_PTR(registry._wifiSensingService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastWifiSensingService);
    TEST_ASSERT_EQUAL_PTR(registry._csiService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastWifiSensingCsiService);
    TEST_ASSERT_EQUAL_PTR(&server, TEST_SERVICE_REGISTRY_INIT::lastNotificationServer);
    TEST_ASSERT_EQUAL_PTR(framework.getFS(), TEST_SERVICE_REGISTRY_INIT::lastNotificationFs);
    TEST_ASSERT_EQUAL_PTR(framework.getSecurityManager(),
                          TEST_SERVICE_REGISTRY_INIT::lastNotificationSecurity);
    TEST_ASSERT_EQUAL_PTR(framework.getFS(), TEST_SERVICE_REGISTRY_INIT::lastCompensationFs);
    TEST_ASSERT_EQUAL_PTR(registry._compensationService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastCompensationService);
    TEST_ASSERT_EQUAL_PTR(framework.getFS(), TEST_SERVICE_REGISTRY_INIT::lastUdpSettingsFs);
    TEST_ASSERT_EQUAL_PTR(framework.getFS(), TEST_SERVICE_REGISTRY_INIT::lastAlarmSettingsFs);

    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::alarmShellyBridgeBuildCalls);
    TEST_ASSERT_EQUAL_PTR(registry.getShellyService(),
                          TEST_SERVICE_REGISTRY_INIT::lastAlarmShellyService);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::setShellyExecutorCalls);
    TEST_ASSERT_TRUE(static_cast<bool>(TEST_SERVICE_REGISTRY_INIT::storedShellyExecutor));

    RTC::AlarmRulesData rules{};
    rules.ruleCount = 3;
    TEST_ASSERT_TRUE(TEST_SERVICE_REGISTRY_INIT::alarmSettingsApplyCallback(rules));
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::alarmUpdateRulesCalls);
    TEST_ASSERT_EQUAL_UINT8(3, TEST_SERVICE_REGISTRY_INIT::lastAlarmUpdateRulesCount);
}

void test_initializeBusinessServices_is_idempotent_for_creation_and_only_rebegins_settings() {
    PsychicHttpServer server;
    ESP32SvelteKit framework(&server);
    auto& registry = TEST_SERVICE_REGISTRY_INIT::rawObject<ServiceRegistry>();
    TEST_SERVICE_REGISTRY_INIT::bindFramework(registry, server, framework);
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(registry._powerManager,
                                             TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<POWER::PowerManager>());
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._wifiSensingService,
        reinterpret_cast<WIFISENSING::WifiSensingService*>(0x5555));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._compensationService,
        reinterpret_cast<COMPENSATION::CompensationService*>(0x6666));
    TEST_SERVICE_REGISTRY_INIT::shellyDeviceCountValue = 0;

    registry.initializeBusinessServices(reinterpret_cast<SemaphoreHandle_t>(0x77));
    registry.initializeBusinessServices(reinterpret_cast<SemaphoreHandle_t>(0x88));

    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createPowerSettingsCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createHeartbeatSettingsCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createShellyServiceCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createWifiSensingSettingsCalls);
    TEST_ASSERT_EQUAL_INT(2, TEST_SERVICE_REGISTRY_INIT::beginWifiSensingSettingsCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createNotificationSettingsCalls);
    TEST_ASSERT_EQUAL_INT(2, TEST_SERVICE_REGISTRY_INIT::beginNotificationSettingsCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createCompensationSettingsCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createUdpPusherCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createUdpSettingsCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createAlarmSettingsCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createGpioServiceCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::beginGpioServiceCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::shellyLoadConfigCalls);
    TEST_ASSERT_EQUAL_INT(0, TEST_SERVICE_REGISTRY_INIT::shellyBeginCalls);
}

void test_initializeBusinessServices_callbacks_delegate_to_power_manager_and_guard_heartbeat() {
    PsychicHttpServer server;
    ESP32SvelteKit framework(&server);
    auto& registry = TEST_SERVICE_REGISTRY_INIT::rawObject<ServiceRegistry>();
    TEST_SERVICE_REGISTRY_INIT::bindFramework(registry, server, framework);
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(registry._powerManager,
                                             TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<POWER::PowerManager>());
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._wifiSensingService,
        reinterpret_cast<WIFISENSING::WifiSensingService*>(0x7777));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._compensationService,
        reinterpret_cast<COMPENSATION::CompensationService*>(0x8888));

    registry.initializeBusinessServices(reinterpret_cast<SemaphoreHandle_t>(0x99));

    TEST_ASSERT_TRUE(static_cast<bool>(TEST_SERVICE_REGISTRY_INIT::powerSettingsApplyCallback));
    TEST_ASSERT_TRUE(static_cast<bool>(TEST_SERVICE_REGISTRY_INIT::heartbeatConfigAppliedCallback));

    RTC::PowerData powerConfig{};
    powerConfig.inactivityTimeoutMs = 12345;
    TEST_SERVICE_REGISTRY_INIT::powerManagerApplyResult = false;
    TEST_ASSERT_FALSE(TEST_SERVICE_REGISTRY_INIT::powerSettingsApplyCallback(powerConfig));
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::powerManagerApplyCalls);
    TEST_ASSERT_EQUAL_UINT32(12345, TEST_SERVICE_REGISTRY_INIT::lastPowerConfig.inactivityTimeoutMs);

    TEST_SERVICE_REGISTRY_INIT::heartbeatConfigAppliedCallback();
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::heartbeatCheckStateCalls);

    registry._isDying.store(true, std::memory_order_release);
    TEST_SERVICE_REGISTRY_INIT::heartbeatConfigAppliedCallback();
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::heartbeatCheckStateCalls);
}

void test_initializeBleServices_creates_settings_begins_and_forwards_registry_state() {
    PsychicHttpServer server;
    ESP32SvelteKit framework(&server);
    auto& registry = TEST_SERVICE_REGISTRY_INIT::rawObject<ServiceRegistry>();
    TEST_SERVICE_REGISTRY_INIT::bindFramework(registry, server, framework);
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(registry._bleService,
                                             reinterpret_cast<BLE::BleService*>(0xaaaa));
    TEST_SERVICE_REGISTRY_INIT::bleInitializerAssignedHandlerId = 41;

    registry.initializeBleServices();

    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createBleSettingsCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::beginBleSettingsCalls);
    TEST_ASSERT_EQUAL_PTR(&server, TEST_SERVICE_REGISTRY_INIT::lastBleServer);
    TEST_ASSERT_EQUAL_PTR(framework.getFS(), TEST_SERVICE_REGISTRY_INIT::lastBleFs);
    TEST_ASSERT_EQUAL_PTR(framework.getSecurityManager(), TEST_SERVICE_REGISTRY_INIT::lastBleSecurity);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::bleInitializerCalls);
    TEST_ASSERT_EQUAL_PTR(registry._bleSettings.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastBleInitializerSettings);
    TEST_ASSERT_EQUAL_PTR(registry._bleService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastBleInitializerService);
    TEST_ASSERT_EQUAL_PTR(&registry._isDying,
                          TEST_SERVICE_REGISTRY_INIT::lastBleInitializerIsDying);
    TEST_ASSERT_EQUAL_UINT32(41, registry._bleWhitelistSettingsHandlerId);
}

void test_initializeBleServices_reuses_existing_settings_and_rebegins_them() {
    PsychicHttpServer server;
    ESP32SvelteKit framework(&server);
    auto& registry = TEST_SERVICE_REGISTRY_INIT::rawObject<ServiceRegistry>();
    TEST_SERVICE_REGISTRY_INIT::bindFramework(registry, server, framework);
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(registry._bleService,
                                             reinterpret_cast<BLE::BleService*>(0xbbbb));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(registry._bleSettings,
        TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<BLE::BleSettingsService>());

    registry.initializeBleServices();

    TEST_ASSERT_EQUAL_INT(0, TEST_SERVICE_REGISTRY_INIT::createBleSettingsCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::beginBleSettingsCalls);
    TEST_ASSERT_EQUAL_PTR(registry._bleSettings.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastBleInitializerSettings);
}

void test_initializeMatrixServices_wires_menu_settings_and_api_in_order() {
    PsychicHttpServer server;
    ESP32SvelteKit framework(&server);
    auto& registry = TEST_SERVICE_REGISTRY_INIT::rawObject<ServiceRegistry>();
    TEST_SERVICE_REGISTRY_INIT::bindFramework(registry, server, framework);
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(registry._powerManager,
                                             TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<POWER::PowerManager>());
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(registry._matrixService,
                                             reinterpret_cast<MatrixService*>(0x1111));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(registry._matrixManager,
        reinterpret_cast<MATRIX_MANAGER::MatrixManagerService*>(0x2222));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(registry._alarmService,
                                             TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<ALARMS::AlarmService>());

    registry.initializeMatrixServices();

    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::createMatrixMenuCalls);
    TEST_ASSERT_EQUAL_PTR(registry._matrixService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastMatrixMenuMatrixService);
    TEST_ASSERT_EQUAL_PTR(registry._matrixManager.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastMatrixMenuManager);
    TEST_ASSERT_EQUAL_PTR(TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<MATRIX::MatrixMenuService>(),
                          registry._matrixMenu.get());

    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::initMatrixSettingsCalls);
    TEST_ASSERT_EQUAL_PTR(framework.getFS(), TEST_SERVICE_REGISTRY_INIT::lastMatrixSettingsFs);
    TEST_ASSERT_EQUAL_PTR(registry._matrixService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastMatrixSettingsMatrixService);
    TEST_ASSERT_EQUAL_PTR(registry._alarmService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastMatrixSettingsAlarmService);
    TEST_ASSERT_EQUAL_PTR(registry._matrixManager.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastMatrixSettingsManager);
    TEST_ASSERT_EQUAL_PTR(registry._matrixMenu.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastMatrixSettingsMenu);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::beginMatrixSettingsCalls);
    TEST_ASSERT_EQUAL_PTR(TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<MATRIX::MatrixSettingsService>(),
                          registry._matrixSettings.get());

    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::initMatrixApiCalls);
    TEST_ASSERT_EQUAL_PTR(&server, TEST_SERVICE_REGISTRY_INIT::lastMatrixApiServer);
    TEST_ASSERT_EQUAL_PTR(framework.getSecurityManager(),
                          TEST_SERVICE_REGISTRY_INIT::lastMatrixApiSecurity);
    TEST_ASSERT_EQUAL_PTR(registry._powerManager.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastMatrixApiPowerManager);
    TEST_ASSERT_EQUAL_PTR(registry._matrixSettings.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastMatrixApiSettings);
    TEST_ASSERT_EQUAL_PTR(registry._csiService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastMatrixApiCsiService);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::beginMatrixApiCalls);

    const std::vector<std::string> expected = {
        "createMatrixMenu",
        "initMatrixSettings",
        "beginMatrixSettings",
        "initMatrixApi",
        "beginMatrixApi",
    };
    TEST_ASSERT_EQUAL(expected.size(), TEST_SERVICE_REGISTRY_INIT::calls.entries.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        TEST_ASSERT_EQUAL_STRING(expected[i].c_str(),
                                 TEST_SERVICE_REGISTRY_INIT::calls.entries[i].c_str());
    }
}

void test_initializeApiServices_forwards_registry_dependencies_to_runtime_helper() {
    PsychicHttpServer server;
    ESP32SvelteKit framework(&server);
    auto& registry = TEST_SERVICE_REGISTRY_INIT::rawObject<ServiceRegistry>();
    TEST_SERVICE_REGISTRY_INIT::bindFramework(registry, server, framework);

    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._powerManager,
        TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<POWER::PowerManager>());
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._powerSettings,
        reinterpret_cast<POWER::PowerSettingsService*>(0x1010));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._shellyService,
        reinterpret_cast<SHELLY::ShellyService*>(0x1020));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._wifiSensingSettings,
        reinterpret_cast<WIFISENSING::WifiSensingSettings*>(0x1030));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._bleSettings,
        reinterpret_cast<BLE::BleSettingsService*>(0x1040));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._compensationSettings,
        reinterpret_cast<COMPENSATION::CompensationSettingsService*>(0x1050));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._alarmService,
        TEST_SERVICE_REGISTRY_INIT::fakeObjectPtr<ALARMS::AlarmService>());
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._alarmSettings,
        reinterpret_cast<ALARMS::AlarmSettingsService*>(0x1060));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._gpioService,
        reinterpret_cast<GPIO::GpioService*>(0x1068));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._csiService,
        reinterpret_cast<WIFISENSING::CSI::CsiService*>(0x1070));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._wifiSensingService,
        reinterpret_cast<WIFISENSING::WifiSensingService*>(0x1080));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._keyboardService,
        reinterpret_cast<KEYBOARD::KeyboardService*>(0x1090));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._keyboardSettings,
        reinterpret_cast<KEYBOARD::KeyboardSettingsService*>(0x10a0));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._bleService,
        reinterpret_cast<BLE::BleService*>(0x10b0));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._macroService,
        reinterpret_cast<MACROS::MacroService*>(0x10c0));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._airMouseService,
        reinterpret_cast<AIRMOUSE::AirMouseService*>(0x10d0));
    registry._usbTerminalService._instance =
        reinterpret_cast<USB_TERMINAL::UsbTerminalService*>(0x10e0);
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._notifications.telegramWorker,
        reinterpret_cast<TELEGRAM::TelegramWorker*>(0x10f0));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._notifications.telegramTestSender,
        reinterpret_cast<API::TelegramTestSender*>(0x1100));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._notifications.webhookTestSender,
        reinterpret_cast<API::WebhookTestSender*>(0x1110));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._notifications.pushoverTestSender,
        reinterpret_cast<API::PushoverTestSender*>(0x1120));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._udpPusher,
        reinterpret_cast<UDPPUSH::UdpPusher*>(0x1130));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._udpSettings,
        reinterpret_cast<UDPPUSH::UdpSettingsService*>(0x1140));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._heartbeatSettings,
        reinterpret_cast<SYSTEM::HeartbeatSettingsService*>(0x1150));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._usbTerminalSettings,
        reinterpret_cast<USB_TERMINAL::UsbTerminalSettingsService*>(0x1160));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._macroSettings,
        reinterpret_cast<MACROS::MacroSettingsService*>(0x1170));
    TEST_SERVICE_REGISTRY_INIT::setUniquePtr(
        registry._airMouseSettings,
        reinterpret_cast<AIRMOUSE::AirMouseSettingsService*>(0x1180));
    registry._fsMutex = reinterpret_cast<SemaphoreHandle_t>(0x1190);

    registry.initializeApiServices();

    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_INIT::initializeApiServicesCalls);
    TEST_ASSERT_EQUAL_PTR(registry._api.get(), TEST_SERVICE_REGISTRY_INIT::lastApiServicesApi);
    TEST_ASSERT_EQUAL_PTR(&server, TEST_SERVICE_REGISTRY_INIT::lastApiServicesServer);
    TEST_ASSERT_EQUAL_PTR(&framework, TEST_SERVICE_REGISTRY_INIT::lastApiServicesFramework);
    TEST_ASSERT_EQUAL_PTR(registry._powerManager.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesPowerManager);
    TEST_ASSERT_EQUAL_PTR(registry._powerSettings.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesPowerSettings);
    TEST_ASSERT_EQUAL_PTR(registry._shellyService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesShelly);
    TEST_ASSERT_EQUAL_PTR(registry._wifiSensingSettings.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesWifiSensingSettings);
    TEST_ASSERT_EQUAL_PTR(registry._bleSettings.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesBleSettings);
    TEST_ASSERT_EQUAL_PTR(registry._compensationSettings.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesCompensationSettings);
    TEST_ASSERT_EQUAL_PTR(registry._alarmService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesAlarmService);
    TEST_ASSERT_EQUAL_PTR(registry._alarmSettings.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesAlarmSettings);
    TEST_ASSERT_EQUAL_PTR(registry._gpioService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesGpioService);
    TEST_ASSERT_EQUAL_PTR(registry._csiService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesCsiService);
    TEST_ASSERT_EQUAL_PTR(registry._wifiSensingService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesWifiSensingService);
    TEST_ASSERT_EQUAL_PTR(registry._keyboardService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesKeyboardService);
    TEST_ASSERT_EQUAL_PTR(registry._keyboardSettings.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesKeyboardSettings);
    TEST_ASSERT_EQUAL_PTR(registry._bleService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesBleService);
    TEST_ASSERT_EQUAL_PTR(registry._macroService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesMacroService);
    TEST_ASSERT_EQUAL_PTR(registry._airMouseService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesAirMouseService);
    TEST_ASSERT_EQUAL_PTR(registry._usbTerminalService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesUsbTerminalService);
    TEST_ASSERT_EQUAL_PTR(registry._notifications.telegramWorker.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesTelegramWorker);
    TEST_ASSERT_EQUAL_PTR(registry._notifications.telegramTestSender.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesTelegramTestSender);
    TEST_ASSERT_EQUAL_PTR(registry._notifications.webhookTestSender.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesWebhookTestSender);
    TEST_ASSERT_EQUAL_PTR(registry._notifications.pushoverTestSender.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesPushoverTestSender);
    TEST_ASSERT_EQUAL_PTR(registry._udpPusher.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesUdpPusher);
    TEST_ASSERT_EQUAL_PTR(registry._udpSettings.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesUdpSettings);
    TEST_ASSERT_EQUAL_PTR(registry._heartbeatSettings.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesHeartbeatSettings);
    TEST_ASSERT_EQUAL_PTR(registry._usbTerminalSettings.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesUsbTerminalSettings);
    TEST_ASSERT_EQUAL_PTR(registry._macroService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesMacroServiceForApi);
    TEST_ASSERT_EQUAL_PTR(registry._macroSettings.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesMacroSettings);
    TEST_ASSERT_EQUAL_PTR(registry._airMouseService.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesAirMouseServiceForApi);
    TEST_ASSERT_EQUAL_PTR(registry._airMouseSettings.get(),
                          TEST_SERVICE_REGISTRY_INIT::lastApiServicesAirMouseSettings);
    TEST_ASSERT_EQUAL_PTR(registry._fsMutex, TEST_SERVICE_REGISTRY_INIT::lastApiServicesFsMutex);
    TEST_ASSERT_NOT_NULL(TEST_SERVICE_REGISTRY_INIT::lastApiServicesHeapMonitor);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_initializeCoreServices_delegates_to_core_initializer_with_registry_refs);
    RUN_TEST(test_initializeBusinessServices_creates_owned_services_and_starts_shelly_when_configured);
    RUN_TEST(test_initializeBusinessServices_is_idempotent_for_creation_and_only_rebegins_settings);
    RUN_TEST(test_initializeBusinessServices_callbacks_delegate_to_power_manager_and_guard_heartbeat);
    RUN_TEST(test_initializeBleServices_creates_settings_begins_and_forwards_registry_state);
    RUN_TEST(test_initializeBleServices_reuses_existing_settings_and_rebegins_them);
    RUN_TEST(test_initializeMatrixServices_wires_menu_settings_and_api_in_order);
    RUN_TEST(test_initializeApiServices_forwards_registry_dependencies_to_runtime_helper);
    return UNITY_END();
}
