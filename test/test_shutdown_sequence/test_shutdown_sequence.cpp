#include <unity.h>

#include <cstdarg>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

#define private public
#include "../../src/system/shutdown/ShutdownSequence.cpp"
#undef private

WiFiClass WiFi;

namespace TEST_SHUTDOWN {

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
inline const char* sleepReason = nullptr;
inline SYSTEM::ShutdownReason recordedReason = SYSTEM::ShutdownReason::UNKNOWN;
inline int watchdogUnregisterCalls = 0;
inline int notificationsShutdownCalls = 0;
inline int notificationWorkerStopCalls = 0;
inline int wifiSensingStopCalls = 0;
inline int airMouseStopCalls = 0;
inline int macroStopCalls = 0;
inline int sensorLoggingStopCalls = 0;
inline int heartbeatStopCalls = 0;
inline bool shellyRunning = false;
inline int shellyStopCalls = 0;
inline int blePrepareCalls = 0;
inline int matrixTaskStopCalls = 0;
inline int imuClearConsumerCalls = 0;
inline int thermalStopCalls = 0;
inline int psramLogBufferEndCalls = 0;
inline int ringBufferEndCalls = 0;
inline API::NotificationsApiService* notificationsApi = nullptr;
alignas(void*) inline unsigned char shellyStorage[sizeof(void*)];

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
    sleepReason = nullptr;
    recordedReason = SYSTEM::ShutdownReason::UNKNOWN;
    watchdogUnregisterCalls = 0;
    notificationsShutdownCalls = 0;
    notificationWorkerStopCalls = 0;
    wifiSensingStopCalls = 0;
    airMouseStopCalls = 0;
    macroStopCalls = 0;
    sensorLoggingStopCalls = 0;
    heartbeatStopCalls = 0;
    shellyRunning = false;
    shellyStopCalls = 0;
    blePrepareCalls = 0;
    matrixTaskStopCalls = 0;
    imuClearConsumerCalls = 0;
    thermalStopCalls = 0;
    psramLogBufferEndCalls = 0;
    ringBufferEndCalls = 0;
    notificationsApi = nullptr;

    TEST_STUBS::WIFI::reset();
    TEST_STUBS::SERIAL::reset();
    TEST_STUBS::FREERTOS::resetTaskCreateStub();
    TEST_STUBS::FREERTOS::tickCount = 0;

    clearRawObject<ServiceRegistry>();
    clearRawObject<POWER::PowerManager>();
    clearRawObject<API::NotificationsApiService>();
    clearRawObject<NOTIFICATIONS::NotificationWorker>();
    clearRawObject<WIFISENSING::WifiSensingService>();
    clearRawObject<AIRMOUSE::AirMouseService>();
    clearRawObject<MACROS::MacroService>();
    clearRawObject<BLE::BleService>();
    clearRawObject<IMU::ImuManager>();
    clearRawObject<MatrixService>();
    std::memset(shellyStorage, 0, sizeof(shellyStorage));
}

void wirePowerManager(ServiceRegistry& registry) {
    new (&registry._powerManager)
        std::unique_ptr<POWER::PowerManager>(reinterpret_cast<POWER::PowerManager*>(rawStorage<POWER::PowerManager>()));
}

void wireNotificationsApi(ServiceRegistry& registry) {
    (void)registry;
    notificationsApi = reinterpret_cast<API::NotificationsApiService*>(rawStorage<API::NotificationsApiService>());
}

void wireNotificationWorker(ServiceRegistry& registry) {
    new (&registry._notifications.runtimeWorker)
        std::unique_ptr<NOTIFICATIONS::NotificationWorker>(
            reinterpret_cast<NOTIFICATIONS::NotificationWorker*>(rawStorage<NOTIFICATIONS::NotificationWorker>()));
}

void wireWifiSensing(ServiceRegistry& registry) {
    new (&registry._wifiSensingService)
        std::unique_ptr<WIFISENSING::WifiSensingService>(
            reinterpret_cast<WIFISENSING::WifiSensingService*>(rawStorage<WIFISENSING::WifiSensingService>()));
}

void wireAirMouse(ServiceRegistry& registry) {
    new (&registry._airMouseService)
        std::unique_ptr<AIRMOUSE::AirMouseService>(
            reinterpret_cast<AIRMOUSE::AirMouseService*>(rawStorage<AIRMOUSE::AirMouseService>()));
}

void wireMacroService(ServiceRegistry& registry) {
    new (&registry._macroService)
        std::unique_ptr<MACROS::MacroService>(
            reinterpret_cast<MACROS::MacroService*>(rawStorage<MACROS::MacroService>()));
}

void wireShellyService(ServiceRegistry& registry) {
    new (&registry._shellyService) std::unique_ptr<SHELLY::ShellyService>(
        reinterpret_cast<SHELLY::ShellyService*>(shellyStorage));
}

void wireBleService(ServiceRegistry& registry) {
    new (&registry._bleService)
        std::unique_ptr<BLE::BleService>(reinterpret_cast<BLE::BleService*>(rawStorage<BLE::BleService>()));
}

void wireMatrixService(ServiceRegistry& registry) {
    new (&registry._matrixService)
        std::unique_ptr<MatrixService>(reinterpret_cast<MatrixService*>(rawStorage<MatrixService>()));
}

void wireImuManager(ServiceRegistry& registry) {
    new (&registry._imuManager)
        std::unique_ptr<IMU::ImuManager>(
            reinterpret_cast<IMU::ImuManager*>(rawStorage<IMU::ImuManager>()));
}

size_t indexOf(const char* value) {
    for (size_t i = 0; i < calls.entries.size(); ++i) {
        if (calls.entries[i] == value) {
            return i;
        }
    }
    return static_cast<size_t>(-1);
}

}  // namespace TEST_SHUTDOWN

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

void RingBuffer::end() {
    TEST_SHUTDOWN::calls.push("ringBuffer.end");
    TEST_SHUTDOWN::ringBufferEndCalls++;
}

}  // namespace LOG

namespace SYSTEM {

void BootTracker::recordShutdown(ShutdownReason reason) {
    TEST_SHUTDOWN::calls.push("bootTracker.recordShutdown");
    TEST_SHUTDOWN::recordedReason = reason;
}

bool TaskWatchdog::unregisterCurrentTask() {
    TEST_SHUTDOWN::calls.push("watchdog.unregister");
    TEST_SHUTDOWN::watchdogUnregisterCalls++;
    return true;
}

ThermalMonitor& ThermalMonitor::instance() {
    static ThermalMonitor monitor;
    return monitor;
}

void ThermalMonitor::stop() {
    TEST_SHUTDOWN::calls.push("thermal.stop");
    TEST_SHUTDOWN::thermalStopCalls++;
}

namespace HEARTBEAT_DETAIL {

HeartbeatWorker& heartbeatWorker() {
    alignas(HeartbeatWorker) static unsigned char storage[sizeof(HeartbeatWorker)];
    return *reinterpret_cast<HeartbeatWorker*>(storage);
}

void HeartbeatWorker::stop() {
    TEST_SHUTDOWN::calls.push("heartbeat.stop");
    TEST_SHUTDOWN::heartbeatStopCalls++;
}

}  // namespace HEARTBEAT_DETAIL

}  // namespace SYSTEM

namespace SENSORS {

void PsramLogBuffer::end() {
    TEST_SHUTDOWN::calls.push("psramLogBuffer.end");
    TEST_SHUTDOWN::psramLogBufferEndCalls++;
}

}  // namespace SENSORS

API::NotificationsApiService* ServiceRegistry::getNotificationsApiService() const {
    return TEST_SHUTDOWN::notificationsApi;
}

namespace API {

void NotificationsApiService::shutdown() {
    TEST_SHUTDOWN::calls.push("notificationsApi.shutdown");
    TEST_SHUTDOWN::notificationsShutdownCalls++;
}

}  // namespace API

namespace NOTIFICATIONS {

StopStatus NotificationWorker::stop() {
    TEST_SHUTDOWN::calls.push("notificationWorker.stop");
    TEST_SHUTDOWN::notificationWorkerStopCalls++;
    return StopStatus::STOPPED;
}

}  // namespace NOTIFICATIONS

namespace WIFISENSING {

bool WifiSensingService::stop() {
    TEST_SHUTDOWN::calls.push("wifiSensing.stop");
    TEST_SHUTDOWN::wifiSensingStopCalls++;
    return true;
}

}  // namespace WIFISENSING

namespace AIRMOUSE {

void AirMouseService::stop() {
    TEST_SHUTDOWN::calls.push("airMouse.stop");
    TEST_SHUTDOWN::airMouseStopCalls++;
}

}  // namespace AIRMOUSE

namespace MACROS {

void MacroService::stop() {
    TEST_SHUTDOWN::calls.push("macro.stop");
    TEST_SHUTDOWN::macroStopCalls++;
}

}  // namespace MACROS

void SensorLoggingTask::stop() {
    TEST_SHUTDOWN::calls.push("sensorLogging.stop");
    TEST_SHUTDOWN::sensorLoggingStopCalls++;
}

namespace SHELLY {

bool runtimeIsRunning(const ShellyService* service) {
    (void)service;
    return TEST_SHUTDOWN::shellyRunning;
}

void runtimeStop(ShellyService* service) {
    (void)service;
    TEST_SHUTDOWN::calls.push("shelly.stop");
    TEST_SHUTDOWN::shellyStopCalls++;
}

}  // namespace SHELLY

namespace BLE {

void BleService::prepareForSleep() {
    TEST_SHUTDOWN::calls.push("ble.prepareForSleep");
    TEST_SHUTDOWN::blePrepareCalls++;
}

}  // namespace BLE

namespace IMU {

void ImuManager::clearConsumers() {
    TEST_SHUTDOWN::calls.push("imu.clearConsumers");
    TEST_SHUTDOWN::imuClearConsumerCalls++;
}

}  // namespace IMU

namespace MATRIX {

void MatrixTask::stop() {
    TEST_SHUTDOWN::calls.push("matrixTask.stop");
    TEST_SHUTDOWN::matrixTaskStopCalls++;
}

}  // namespace MATRIX

namespace POWER {

const char* PowerManager::getSleepReason() {
    return TEST_SHUTDOWN::sleepReason;
}

}  // namespace POWER

void setUp(void) {
    TEST_SHUTDOWN::reset();
}

void tearDown(void) {}

void test_execute_maps_normal_sleep_reason_to_clean_sleep_and_flushes_runtime() {
    auto& registry = TEST_SHUTDOWN::rawObject<ServiceRegistry>();
    TEST_SHUTDOWN::wirePowerManager(registry);
    TEST_SHUTDOWN::sleepReason = "scheduled";

    SYSTEM::ShutdownSequence::execute(registry);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SYSTEM::ShutdownReason::CLEAN_SLEEP),
                            static_cast<uint8_t>(TEST_SHUTDOWN::recordedReason));
    TEST_ASSERT_EQUAL_INT(1, TEST_SHUTDOWN::watchdogUnregisterCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SHUTDOWN::psramLogBufferEndCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SHUTDOWN::ringBufferEndCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_STUBS::SERIAL::flushCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_STUBS::SERIAL::endCalls);
    TEST_ASSERT_EQUAL_UINT32(SHUTDOWN::HARDWARE_SETTLE_DELAY_MS +
                                 SHUTDOWN::SERIAL_FLUSH_DELAY_MS +
                                 SHUTDOWN::SERIAL_END_DELAY_MS,
                             TEST_STUBS::FREERTOS::tickCount);
}

void test_execute_maps_maintenance_sleep_reason_to_hygiene_sleep() {
    auto& registry = TEST_SHUTDOWN::rawObject<ServiceRegistry>();
    TEST_SHUTDOWN::wirePowerManager(registry);
    TEST_SHUTDOWN::sleepReason = "manual-hygiene";

    SYSTEM::ShutdownSequence::execute(registry);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SYSTEM::ShutdownReason::HYGIENE_SLEEP),
                            static_cast<uint8_t>(TEST_SHUTDOWN::recordedReason));
}

void test_execute_prefers_explicit_reason_over_power_manager_reason() {
    auto& registry = TEST_SHUTDOWN::rawObject<ServiceRegistry>();
    TEST_SHUTDOWN::wirePowerManager(registry);
    TEST_SHUTDOWN::sleepReason = "hygiene";

    SYSTEM::ShutdownSequence::execute(registry, SYSTEM::ShutdownReason::RESTART_COMMAND);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SYSTEM::ShutdownReason::RESTART_COMMAND),
                            static_cast<uint8_t>(TEST_SHUTDOWN::recordedReason));
}

void test_execute_stops_runtime_services_before_network_and_hardware_shutdown() {
    auto& registry = TEST_SHUTDOWN::rawObject<ServiceRegistry>();
    TEST_SHUTDOWN::wirePowerManager(registry);
    TEST_SHUTDOWN::wireNotificationsApi(registry);
    TEST_SHUTDOWN::wireNotificationWorker(registry);
    TEST_SHUTDOWN::wireWifiSensing(registry);
    TEST_SHUTDOWN::wireAirMouse(registry);
    TEST_SHUTDOWN::wireMacroService(registry);
    TEST_SHUTDOWN::wireShellyService(registry);
    TEST_SHUTDOWN::wireBleService(registry);
    TEST_SHUTDOWN::wireMatrixService(registry);
    TEST_SHUTDOWN::wireImuManager(registry);
    TEST_SHUTDOWN::sleepReason = "scheduled";
    TEST_SHUTDOWN::shellyRunning = true;

    PsychicHttpServer server;
    PsychicClient firstClient;
    PsychicClient secondClient;
    firstClient.setSocket(41);
    secondClient.setSocket(99);
    server.registerClient(&firstClient);
    server.registerClient(&secondClient);
    registry._server = &server;

    SYSTEM::ShutdownSequence::execute(registry);

    TEST_ASSERT_EQUAL_INT(1, TEST_SHUTDOWN::notificationsShutdownCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SHUTDOWN::notificationWorkerStopCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SHUTDOWN::wifiSensingStopCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SHUTDOWN::airMouseStopCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SHUTDOWN::macroStopCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SHUTDOWN::sensorLoggingStopCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SHUTDOWN::heartbeatStopCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SHUTDOWN::shellyStopCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SHUTDOWN::blePrepareCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SHUTDOWN::matrixTaskStopCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SHUTDOWN::imuClearConsumerCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SHUTDOWN::thermalStopCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_STUBS::WIFI::modeCalls);
    TEST_ASSERT_EQUAL_INT(WIFI_MODE_NULL, TEST_STUBS::WIFI::mode);
    TEST_ASSERT_TRUE(firstClient.closed());
    TEST_ASSERT_TRUE(secondClient.closed());
    TEST_ASSERT_EQUAL_UINT8(UI::MATRIX::BRIGHTNESS_OFF, registry.getMatrixService()->lastLimit);
    TEST_ASSERT_EQUAL_UINT32(1, registry.getMatrixService()->clearCalls);
    TEST_ASSERT_TRUE(registry.getMatrixService()->lastClearStopBackground);

    TEST_ASSERT_EQUAL_UINT32(SHUTDOWN::HTTP_CLIENT_CLOSE_GRACE_MS +
                                 SHUTDOWN::HARDWARE_SETTLE_DELAY_MS +
                                 SHUTDOWN::SERIAL_FLUSH_DELAY_MS +
                                 SHUTDOWN::SERIAL_END_DELAY_MS,
                             TEST_STUBS::FREERTOS::tickCount);

    const size_t notificationsIdx = TEST_SHUTDOWN::indexOf("notificationsApi.shutdown");
    const size_t wifiIdx = TEST_SHUTDOWN::indexOf("wifiSensing.stop");
    const size_t bleIdx = TEST_SHUTDOWN::indexOf("ble.prepareForSleep");
    const size_t matrixIdx = TEST_SHUTDOWN::indexOf("matrixTask.stop");
    const size_t thermalIdx = TEST_SHUTDOWN::indexOf("thermal.stop");
    const size_t bufferIdx = TEST_SHUTDOWN::indexOf("psramLogBuffer.end");
    const size_t flushIdx = TEST_SHUTDOWN::indexOf("ringBuffer.end");

    TEST_ASSERT_TRUE(notificationsIdx != static_cast<size_t>(-1));
    TEST_ASSERT_TRUE(wifiIdx != static_cast<size_t>(-1));
    TEST_ASSERT_TRUE(bleIdx != static_cast<size_t>(-1));
    TEST_ASSERT_TRUE(matrixIdx != static_cast<size_t>(-1));
    TEST_ASSERT_TRUE(thermalIdx != static_cast<size_t>(-1));
    TEST_ASSERT_TRUE(bufferIdx != static_cast<size_t>(-1));
    TEST_ASSERT_TRUE(flushIdx != static_cast<size_t>(-1));
    TEST_ASSERT_TRUE(wifiIdx < bleIdx);
    TEST_ASSERT_TRUE(matrixIdx < bleIdx);
    TEST_ASSERT_TRUE(thermalIdx < bleIdx);
    TEST_ASSERT_TRUE(thermalIdx < bufferIdx);
    TEST_ASSERT_TRUE(bufferIdx < flushIdx);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_execute_maps_normal_sleep_reason_to_clean_sleep_and_flushes_runtime);
    RUN_TEST(test_execute_maps_maintenance_sleep_reason_to_hygiene_sleep);
    RUN_TEST(test_execute_prefers_explicit_reason_over_power_manager_reason);
    RUN_TEST(test_execute_stops_runtime_services_before_network_and_hardware_shutdown);
    return UNITY_END();
}
