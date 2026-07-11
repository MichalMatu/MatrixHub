#include <unity.h>

#include <cstdarg>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

#define private public
#include "../../src/system/services/ServiceRegistryWorkers.cpp"
#undef private

namespace TEST_SERVICE_REGISTRY_WORKER_SHUTDOWN {

inline std::vector<std::string> calls;
inline int reconcilerFailuresRemaining = 0;
inline int wifiFailuresRemaining = 0;

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
    reconcilerFailuresRemaining = 0;
    wifiFailuresRemaining = 0;
    TEST_STUBS::FREERTOS::tickCount = 0;
    clearRawObject<ServiceRegistry>();
    clearRawObject<WIFISENSING::WifiSensingSettings>();
    clearRawObject<WIFISENSING::WifiSensingService>();
    clearRawObject<NOTIFICATIONS::NotificationWorker>();
    clearRawObject<IMU::ImuManager>();
}

void wireWorkers(ServiceRegistry& registry) {
    new (&registry._wifiSensingSettings)
        std::unique_ptr<WIFISENSING::WifiSensingSettings>(
            reinterpret_cast<WIFISENSING::WifiSensingSettings*>(
                rawStorage<WIFISENSING::WifiSensingSettings>()));
    new (&registry._wifiSensingService)
        std::unique_ptr<WIFISENSING::WifiSensingService>(
            reinterpret_cast<WIFISENSING::WifiSensingService*>(
                rawStorage<WIFISENSING::WifiSensingService>()));
    new (&registry._notifications.runtimeWorker)
        std::unique_ptr<NOTIFICATIONS::NotificationWorker>(
            reinterpret_cast<NOTIFICATIONS::NotificationWorker*>(
                rawStorage<NOTIFICATIONS::NotificationWorker>()));
    new (&registry._imuManager)
        std::unique_ptr<IMU::ImuManager>(
            reinterpret_cast<IMU::ImuManager*>(rawStorage<IMU::ImuManager>()));
}

}  // namespace TEST_SERVICE_REGISTRY_WORKER_SHUTDOWN

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

namespace WIFISENSING {

bool WifiSensingSettings::shutdownRuntimeReconciler(TickType_t waitTicks) {
    (void)waitTicks;
    TEST_SERVICE_REGISTRY_WORKER_SHUTDOWN::calls.emplace_back("reconciler.shutdown");
    if (TEST_SERVICE_REGISTRY_WORKER_SHUTDOWN::reconcilerFailuresRemaining > 0) {
        TEST_SERVICE_REGISTRY_WORKER_SHUTDOWN::reconcilerFailuresRemaining--;
        return false;
    }
    return true;
}

bool WifiSensingService::shutdown() {
    TEST_SERVICE_REGISTRY_WORKER_SHUTDOWN::calls.emplace_back("wifi.shutdown");
    if (TEST_SERVICE_REGISTRY_WORKER_SHUTDOWN::wifiFailuresRemaining > 0) {
        TEST_SERVICE_REGISTRY_WORKER_SHUTDOWN::wifiFailuresRemaining--;
        return false;
    }
    return true;
}

}  // namespace WIFISENSING

namespace NOTIFICATIONS {

StopStatus NotificationWorker::stop() {
    TEST_SERVICE_REGISTRY_WORKER_SHUTDOWN::calls.emplace_back("notification.stop");
    return StopStatus::STOPPED;
}

}  // namespace NOTIFICATIONS

namespace IMU {

void ImuManager::clearConsumers() {
    TEST_SERVICE_REGISTRY_WORKER_SHUTDOWN::calls.emplace_back("imu.clearConsumers");
}

}  // namespace IMU

void setUp(void) {
    TEST_SERVICE_REGISTRY_WORKER_SHUTDOWN::reset();
}

void tearDown(void) {}

void test_stopBackgroundWorkers_drains_reconciler_before_dependent_teardown() {
    auto& registry =
        TEST_SERVICE_REGISTRY_WORKER_SHUTDOWN::rawObject<ServiceRegistry>();
    TEST_SERVICE_REGISTRY_WORKER_SHUTDOWN::wireWorkers(registry);
    TEST_SERVICE_REGISTRY_WORKER_SHUTDOWN::reconcilerFailuresRemaining = 2;
    TEST_SERVICE_REGISTRY_WORKER_SHUTDOWN::wifiFailuresRemaining = 2;

    registry.stopBackgroundWorkers();

    const std::vector<std::string> expected = {
        "reconciler.shutdown",
        "reconciler.shutdown",
        "reconciler.shutdown",
        "wifi.shutdown",
        "wifi.shutdown",
        "wifi.shutdown",
        "notification.stop",
        "imu.clearConsumers",
    };
    TEST_ASSERT_EQUAL(expected.size(),
                      TEST_SERVICE_REGISTRY_WORKER_SHUTDOWN::calls.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        TEST_ASSERT_EQUAL_STRING(
            expected[i].c_str(),
            TEST_SERVICE_REGISTRY_WORKER_SHUTDOWN::calls[i].c_str());
    }
    TEST_ASSERT_EQUAL_UINT32(
        4 * TIMEOUT::TASK_SHUTDOWN_POLL_TICKS,
        TEST_STUBS::FREERTOS::tickCount);
}

void test_stopBackgroundWorkers_handles_partial_initialization() {
    auto& registry =
        TEST_SERVICE_REGISTRY_WORKER_SHUTDOWN::rawObject<ServiceRegistry>();

    registry.stopBackgroundWorkers();

    TEST_ASSERT_TRUE(TEST_SERVICE_REGISTRY_WORKER_SHUTDOWN::calls.empty());
    TEST_ASSERT_EQUAL_UINT32(0, TEST_STUBS::FREERTOS::tickCount);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_stopBackgroundWorkers_drains_reconciler_before_dependent_teardown);
    RUN_TEST(test_stopBackgroundWorkers_handles_partial_initialization);
    return UNITY_END();
}
