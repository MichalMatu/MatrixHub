#include <unity.h>

#include <cstdarg>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#define private public
#include "../../src/system/ApplicationRuntime.cpp"
#undef private

namespace TEST_APPLICATION {

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
inline int phase1Calls = 0;
inline int phase2Calls = 0;
inline int phase3Calls = 0;
inline int configureNetworkCalls = 0;
inline int phase4Calls = 0;
inline int redirectCalls = 0;
inline int phase5Calls = 0;
inline int phase6Calls = 0;
inline int phase7Calls = 0;
inline int bootCompleteCalls = 0;

inline ServiceRegistry* lastPhase3Services = nullptr;
inline PsychicHttpsServer* lastConfiguredServer = nullptr;
inline ESP32SvelteKit* lastPhase4Framework = nullptr;
inline SYSTEM::NETWORK::HttpRedirectServer* lastRedirectServer = nullptr;
inline PsychicHttpServer* lastPhase5Server = nullptr;
inline ESP32SvelteKit* lastPhase5Framework = nullptr;
inline ServiceRegistry* lastPhase5Services = nullptr;
inline SemaphoreHandle_t lastPhase5NetworkMutex = nullptr;
inline SemaphoreHandle_t lastPhase5NotifMutex = nullptr;
inline ServiceRegistry* lastPhase6Services = nullptr;
inline ServiceRegistry* lastPhase7Services = nullptr;
inline ButtonHandler* lastPhase7ButtonHandler = nullptr;

inline int buttonUpdateCalls = 0;
inline int powerLoopTickCalls = 0;
inline int bleLoopCalls = 0;
inline int systemHealthUpdateCalls = 0;
inline int alarmProcessPendingCalls = 0;
inline int shellyRuntimeReconcileCalls = 0;
inline uint32_t lastShellyRuntimeReconcileMs = 0;
inline int wifiSensingReconcileCalls = 0;
inline uint32_t lastWifiSensingReconcileMs = 0;
inline int deferredApiReconcileCalls = 0;
inline int imuRuntimeTickCalls = 0;
inline int udpUpdateCalls = 0;
inline int usbTerminalLoopCalls = 0;
inline int maintenanceUpdateCalls = 0;

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
void clearRawObject() {
    std::memset(rawStorage<T>(), 0, sizeof(T));
}

void reset() {
    calls.clear();
    phase1Calls = 0;
    phase2Calls = 0;
    phase3Calls = 0;
    configureNetworkCalls = 0;
    phase4Calls = 0;
    redirectCalls = 0;
    phase5Calls = 0;
    phase6Calls = 0;
    phase7Calls = 0;
    bootCompleteCalls = 0;
    lastPhase3Services = nullptr;
    lastConfiguredServer = nullptr;
    lastPhase4Framework = nullptr;
    lastRedirectServer = nullptr;
    lastPhase5Server = nullptr;
    lastPhase5Framework = nullptr;
    lastPhase5Services = nullptr;
    lastPhase5NetworkMutex = nullptr;
    lastPhase5NotifMutex = nullptr;
    lastPhase6Services = nullptr;
    lastPhase7Services = nullptr;
    lastPhase7ButtonHandler = nullptr;
    buttonUpdateCalls = 0;
    powerLoopTickCalls = 0;
    bleLoopCalls = 0;
    systemHealthUpdateCalls = 0;
    alarmProcessPendingCalls = 0;
    shellyRuntimeReconcileCalls = 0;
    lastShellyRuntimeReconcileMs = 0;
    wifiSensingReconcileCalls = 0;
    lastWifiSensingReconcileMs = 0;
    deferredApiReconcileCalls = 0;
    imuRuntimeTickCalls = 0;
    udpUpdateCalls = 0;
    usbTerminalLoopCalls = 0;
    maintenanceUpdateCalls = 0;
    clearRawObject<ServiceRegistry>();
    clearRawObject<ButtonHandler>();
    clearRawObject<POWER::PowerManager>();
    clearRawObject<BLE::BleService>();
    clearRawObject<ALARMS::AlarmService>();
    clearRawObject<WIFISENSING::WifiSensingSettings>();
    clearRawObject<IMU::ImuRuntimeService>();
    clearRawObject<UDPPUSH::UdpPusher>();
    clearRawObject<USB_TERMINAL::UsbTerminalService>();
}

void wireLoopServices(ServiceRegistry& services) {
    new (&services._powerManager)
        std::unique_ptr<POWER::PowerManager>(reinterpret_cast<POWER::PowerManager*>(rawStorage<POWER::PowerManager>()));
    new (&services._bleService)
        std::unique_ptr<BLE::BleService>(reinterpret_cast<BLE::BleService*>(rawStorage<BLE::BleService>()));
    new (&services._alarmService)
        std::unique_ptr<ALARMS::AlarmService>(reinterpret_cast<ALARMS::AlarmService*>(rawStorage<ALARMS::AlarmService>()));
    new (&services._shellyService)
        std::unique_ptr<SHELLY::ShellyService>(
            reinterpret_cast<SHELLY::ShellyService*>(0x2020));
    new (&services._wifiSensingSettings)
        std::unique_ptr<WIFISENSING::WifiSensingSettings>(
            reinterpret_cast<WIFISENSING::WifiSensingSettings*>(rawStorage<WIFISENSING::WifiSensingSettings>()));
    new (&services._imuRuntimeService)
        std::unique_ptr<IMU::ImuRuntimeService>(
            reinterpret_cast<IMU::ImuRuntimeService*>(rawStorage<IMU::ImuRuntimeService>()));
    new (&services._udpPusher)
        std::unique_ptr<UDPPUSH::UdpPusher>(reinterpret_cast<UDPPUSH::UdpPusher*>(rawStorage<UDPPUSH::UdpPusher>()));
    new (&services._usbTerminalService)
        SYSTEM::PsramStaticService<USB_TERMINAL::UsbTerminalService>();
    services._usbTerminalService._instance =
        reinterpret_cast<USB_TERMINAL::UsbTerminalService*>(rawStorage<USB_TERMINAL::UsbTerminalService>());
}

}  // namespace TEST_APPLICATION

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

namespace SYSTEM {

void InitSequence::phase1_Storage() {
    TEST_APPLICATION::calls.push("phase1");
    TEST_APPLICATION::phase1Calls++;
}

void InitSequence::phase2_Logging() {
    TEST_APPLICATION::calls.push("phase2");
    TEST_APPLICATION::phase2Calls++;
}

void InitSequence::phase3_Power(ServiceRegistry& services) {
    TEST_APPLICATION::calls.push("phase3");
    TEST_APPLICATION::phase3Calls++;
    TEST_APPLICATION::lastPhase3Services = &services;
}

void InitSequence::configureNetwork(PsychicHttpsServer& server) {
    TEST_APPLICATION::calls.push("configureNetwork");
    TEST_APPLICATION::configureNetworkCalls++;
    TEST_APPLICATION::lastConfiguredServer = &server;
}

void InitSequence::phase4_Framework(ESP32SvelteKit& framework) {
    TEST_APPLICATION::calls.push("phase4");
    TEST_APPLICATION::phase4Calls++;
    TEST_APPLICATION::lastPhase4Framework = &framework;
}

void InitSequence::phase4_HttpRedirect(NETWORK::HttpRedirectServer& redirectServer,
                                       const httpd_config_t& baseConfig) {
    (void)baseConfig;
    TEST_APPLICATION::calls.push("phase4Redirect");
    TEST_APPLICATION::redirectCalls++;
    TEST_APPLICATION::lastRedirectServer = &redirectServer;
}

void InitSequence::phase5_Services(PsychicHttpServer& server,
                                   ESP32SvelteKit& framework,
                                   ServiceRegistry& services,
                                   SemaphoreHandle_t networkMutex,
                                   SemaphoreHandle_t notifMutex) {
    TEST_APPLICATION::calls.push("phase5");
    TEST_APPLICATION::phase5Calls++;
    TEST_APPLICATION::lastPhase5Server = &server;
    TEST_APPLICATION::lastPhase5Framework = &framework;
    TEST_APPLICATION::lastPhase5Services = &services;
    TEST_APPLICATION::lastPhase5NetworkMutex = networkMutex;
    TEST_APPLICATION::lastPhase5NotifMutex = notifMutex;
}

void InitSequence::phase6_Tasks(ServiceRegistry& services) {
    TEST_APPLICATION::calls.push("phase6");
    TEST_APPLICATION::phase6Calls++;
    TEST_APPLICATION::lastPhase6Services = &services;
}

void InitSequence::phase7_Monitoring(ServiceRegistry& services, ButtonHandler& buttonHandler) {
    TEST_APPLICATION::calls.push("phase7");
    TEST_APPLICATION::phase7Calls++;
    TEST_APPLICATION::lastPhase7Services = &services;
    TEST_APPLICATION::lastPhase7ButtonHandler = &buttonHandler;
}

void InitSequence::signalBootComplete() {
    TEST_APPLICATION::calls.push("signalBootComplete");
    TEST_APPLICATION::bootCompleteCalls++;
}

void SystemHealth::update() {
    TEST_APPLICATION::systemHealthUpdateCalls++;
}

void HealthMaintenancePulse::update() {
    TEST_APPLICATION::maintenanceUpdateCalls++;
}

}  // namespace SYSTEM

namespace SYSTEM::RESTART {

void emergencyRestart(SYSTEM::ShutdownReason reason,
                      const char* logTag,
                      const char* message,
                      uint32_t delayMs) {
    (void)reason;
    (void)logTag;
    (void)message;
    (void)delayMs;
}

}  // namespace SYSTEM::RESTART

void ButtonHandler::update() {
    TEST_APPLICATION::buttonUpdateCalls++;
}

void POWER::PowerManager::loopTick() {
    TEST_APPLICATION::powerLoopTickCalls++;
}

void BLE::BleService::loop() {
    TEST_APPLICATION::bleLoopCalls++;
}

uint8_t ALARMS::AlarmService::processPending() {
    TEST_APPLICATION::alarmProcessPendingCalls++;
    return 0;
}

void SHELLY::reconcileRuntimeIfDue(ShellyService* service, uint32_t nowMs) {
    TEST_ASSERT_NOT_NULL(service);
    TEST_APPLICATION::shellyRuntimeReconcileCalls++;
    TEST_APPLICATION::lastShellyRuntimeReconcileMs = nowMs;
}

void WIFISENSING::WifiSensingSettings::reconcileRuntimeIfDue(uint32_t nowMs) {
    TEST_APPLICATION::wifiSensingReconcileCalls++;
    TEST_APPLICATION::lastWifiSensingReconcileMs = nowMs;
}

void ServiceRegistry::reconcileDeferredApiLifecycle() {
    TEST_APPLICATION::deferredApiReconcileCalls++;
}

void IMU::ImuRuntimeService::tick() {
    TEST_APPLICATION::imuRuntimeTickCalls++;
}

void UDPPUSH::UdpPusher::update() {
    TEST_APPLICATION::udpUpdateCalls++;
}

void USB_TERMINAL::UsbTerminalService::loop() {
    TEST_APPLICATION::usbTerminalLoopCalls++;
}

void setUp(void) {
    TEST_APPLICATION::reset();
}

void tearDown(void) {}

void test_runBootSequence_orders_all_phases_and_passes_dependencies() {
    PsychicHttpsServer server;
    ESP32SvelteKit framework(&server);
    SYSTEM::NETWORK::HttpRedirectServer redirectServer;
    auto& services = TEST_APPLICATION::rawObject<ServiceRegistry>();
    auto& buttonHandler = TEST_APPLICATION::rawObject<ButtonHandler>();
    auto networkMutex = reinterpret_cast<SemaphoreHandle_t>(0x11);
    auto notifMutex = reinterpret_cast<SemaphoreHandle_t>(0x22);

    APP_INTERNAL::runBootSequence(server,
                                  framework,
                                  redirectServer,
                                  services,
                                  buttonHandler,
                                  networkMutex,
                                  notifMutex);

    const std::vector<std::string> expected = {
        "phase1",
        "phase2",
        "phase3",
        "configureNetwork",
        "phase4",
        "phase4Redirect",
        "phase5",
        "phase6",
        "phase7",
        "signalBootComplete",
    };
    TEST_ASSERT_EQUAL(expected.size(), TEST_APPLICATION::calls.entries.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        TEST_ASSERT_EQUAL_STRING(expected[i].c_str(), TEST_APPLICATION::calls.entries[i].c_str());
    }

    TEST_ASSERT_EQUAL_PTR(&services, TEST_APPLICATION::lastPhase3Services);
    TEST_ASSERT_EQUAL_PTR(&server, TEST_APPLICATION::lastConfiguredServer);
    TEST_ASSERT_EQUAL_PTR(&framework, TEST_APPLICATION::lastPhase4Framework);
    TEST_ASSERT_EQUAL_PTR(&redirectServer, TEST_APPLICATION::lastRedirectServer);
    TEST_ASSERT_EQUAL_PTR(&server, TEST_APPLICATION::lastPhase5Server);
    TEST_ASSERT_EQUAL_PTR(&framework, TEST_APPLICATION::lastPhase5Framework);
    TEST_ASSERT_EQUAL_PTR(&services, TEST_APPLICATION::lastPhase5Services);
    TEST_ASSERT_EQUAL_PTR(networkMutex, TEST_APPLICATION::lastPhase5NetworkMutex);
    TEST_ASSERT_EQUAL_PTR(notifMutex, TEST_APPLICATION::lastPhase5NotifMutex);
    TEST_ASSERT_EQUAL_PTR(&services, TEST_APPLICATION::lastPhase6Services);
    TEST_ASSERT_EQUAL_PTR(&services, TEST_APPLICATION::lastPhase7Services);
    TEST_ASSERT_EQUAL_PTR(&buttonHandler, TEST_APPLICATION::lastPhase7ButtonHandler);
}

void test_runLoopCore_drives_registry_owned_runtime_services() {
    auto& services = TEST_APPLICATION::rawObject<ServiceRegistry>();
    auto& buttonHandler = TEST_APPLICATION::rawObject<ButtonHandler>();
    PsychicHttpsServer server;
    ESP32SvelteKit framework(&server);
    TEST_APPLICATION::wireLoopServices(services);
    TEST_STUBS::ARDUINO::millisValue = 1234;

    APP_INTERNAL::runLoopCore(services, buttonHandler, framework);

    TEST_ASSERT_EQUAL_INT(1, TEST_APPLICATION::powerLoopTickCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_APPLICATION::buttonUpdateCalls);
    TEST_ASSERT_EQUAL_INT(1, framework.loopCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_APPLICATION::bleLoopCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_APPLICATION::systemHealthUpdateCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_APPLICATION::imuRuntimeTickCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_APPLICATION::alarmProcessPendingCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_APPLICATION::shellyRuntimeReconcileCalls);
    TEST_ASSERT_EQUAL_UINT32(1234, TEST_APPLICATION::lastShellyRuntimeReconcileMs);
    TEST_ASSERT_EQUAL_INT(1, TEST_APPLICATION::wifiSensingReconcileCalls);
    TEST_ASSERT_EQUAL_UINT32(1234, TEST_APPLICATION::lastWifiSensingReconcileMs);
    TEST_ASSERT_EQUAL_INT(1, TEST_APPLICATION::deferredApiReconcileCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_APPLICATION::udpUpdateCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_APPLICATION::usbTerminalLoopCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_APPLICATION::maintenanceUpdateCalls);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_runBootSequence_orders_all_phases_and_passes_dependencies);
    RUN_TEST(test_runLoopCore_drives_registry_owned_runtime_services);
    return UNITY_END();
}
