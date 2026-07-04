#include <unity.h>

#include "../../src/system/logging/Logging.h"
#include "../../src/sensors/imu/ImuService.h"

namespace LOG {

Settings Logging::_settings{ESP_LOG_NONE};

void Logging::begin(const Settings& settings) { _settings = settings; }
void Logging::setLevel(esp_log_level_t level) { _settings.level = level; }
Settings Logging::settings() { return _settings; }
bool Logging::isEnabled(esp_log_level_t level) { return level <= _settings.level; }
void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    (void)level;
    (void)tag;
    (void)fmt;
}
void Logging::clearBuffer() {}
const char* Logging::levelToString(esp_log_level_t level) {
    (void)level;
    return "none";
}
esp_log_level_t Logging::stringToLevel(std::string_view name, esp_log_level_t fallback) {
    (void)name;
    return fallback;
}
void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {
    (void)taskName;
    (void)stackSize;
}
void Logging::logSection(const char* title) { (void)title; }
void Logging::suppressNoisyModules() {}

}  // namespace LOG

bool ImuService::begin() { return true; }
void ImuService::stop() {}

#include "../../src/sensors/imu/ImuManager.cpp"

namespace {

struct BackendProbe {
    bool initialized = false;
    bool startResult = true;
    uint32_t startCalls = 0;
    uint32_t stopCalls = 0;
};

uint32_t bit(IMU::Consumer consumer) {
    return 1u << static_cast<uint8_t>(consumer);
}

IMU::ImuManager makeManager(BackendProbe& probe) {
    return IMU::ImuManager(IMU::ImuManager::Backend{
        [&probe]() {
            probe.startCalls++;
            probe.initialized = probe.startResult;
            return probe.startResult;
        },
        [&probe]() {
            probe.stopCalls++;
            probe.initialized = false;
        },
        [&probe]() {
            return probe.initialized;
        }});
}

}  // namespace

void setUp(void) {
    TEST_STUBS::ARDUINO::millisValue = 0;
}

void tearDown(void) {}

void test_manager_starts_once_for_multiple_consumers_and_stops_when_clear() {
    BackendProbe probe;
    auto manager = makeManager(probe);

    manager.setConsumerActive(IMU::Consumer::AirMouseMovement, true);
    TEST_ASSERT_EQUAL_UINT32(1, probe.startCalls);
    TEST_ASSERT_EQUAL_UINT32(bit(IMU::Consumer::AirMouseMovement), manager.desiredMask());
    TEST_ASSERT_EQUAL_UINT32(bit(IMU::Consumer::AirMouseMovement), manager.runningMask());

    manager.setConsumerActive(IMU::Consumer::Alarm, true);
    TEST_ASSERT_EQUAL_UINT32(1, probe.startCalls);
    TEST_ASSERT_EQUAL_UINT32(
        bit(IMU::Consumer::AirMouseMovement) | bit(IMU::Consumer::Alarm),
        manager.desiredMask());
    TEST_ASSERT_EQUAL_UINT32(manager.desiredMask(), manager.runningMask());

    manager.clearConsumers();
    TEST_ASSERT_EQUAL_UINT32(1, probe.stopCalls);
    TEST_ASSERT_EQUAL_UINT32(0, manager.desiredMask());
    TEST_ASSERT_EQUAL_UINT32(0, manager.runningMask());
    TEST_ASSERT_FALSE(manager.isRunning());
}

void test_failed_start_keeps_desired_mask_and_retries_after_delay() {
    BackendProbe probe;
    probe.startResult = false;
    auto manager = makeManager(probe);

    manager.setConsumerActive(IMU::Consumer::Alarm, true);
    IMU::ManagerStatus status = manager.getStatus();

    TEST_ASSERT_EQUAL_UINT32(1, probe.startCalls);
    TEST_ASSERT_EQUAL_UINT32(bit(IMU::Consumer::Alarm), status.desiredMask);
    TEST_ASSERT_EQUAL_UINT32(0, status.runningMask);
    TEST_ASSERT_EQUAL(IMU::StartError::StartFailed, status.lastStartError);
    TEST_ASSERT_GREATER_THAN_UINT32(0, status.nextRetryMs);

    manager.setConsumerActive(IMU::Consumer::UiMonitor, true);
    status = manager.getStatus();

    TEST_ASSERT_EQUAL_UINT32(1, probe.startCalls);
    TEST_ASSERT_EQUAL_UINT32(
        bit(IMU::Consumer::Alarm) | bit(IMU::Consumer::UiMonitor),
        status.desiredMask);
    TEST_ASSERT_EQUAL(IMU::StartError::RetryPending, status.lastStartError);

    TEST_STUBS::ARDUINO::millisValue = status.nextRetryMs;
    probe.startResult = true;
    manager.tick();
    status = manager.getStatus();

    TEST_ASSERT_EQUAL_UINT32(2, probe.startCalls);
    TEST_ASSERT_EQUAL_UINT32(status.desiredMask, status.runningMask);
    TEST_ASSERT_TRUE(status.initialized);
    TEST_ASSERT_EQUAL(IMU::StartError::None, status.lastStartError);
}

void test_clearing_after_failed_start_resets_retry_window() {
    BackendProbe probe;
    probe.startResult = false;
    auto manager = makeManager(probe);

    manager.setConsumerActive(IMU::Consumer::Alarm, true);
    TEST_ASSERT_EQUAL_UINT32(1, probe.startCalls);
    TEST_ASSERT_GREATER_THAN_UINT32(0, manager.getStatus().nextRetryMs);

    manager.clearConsumers();
    IMU::ManagerStatus status = manager.getStatus();
    TEST_ASSERT_EQUAL_UINT32(0, status.desiredMask);
    TEST_ASSERT_EQUAL_UINT32(0, status.runningMask);
    TEST_ASSERT_EQUAL_UINT32(0, status.nextRetryMs);
    TEST_ASSERT_EQUAL(IMU::StartError::None, status.lastStartError);

    probe.startResult = true;
    TEST_STUBS::ARDUINO::millisValue = 1;
    manager.setConsumerActive(IMU::Consumer::Alarm, true);
    status = manager.getStatus();

    TEST_ASSERT_EQUAL_UINT32(2, probe.startCalls);
    TEST_ASSERT_EQUAL_UINT32(bit(IMU::Consumer::Alarm), status.runningMask);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_manager_starts_once_for_multiple_consumers_and_stops_when_clear);
    RUN_TEST(test_failed_start_keeps_desired_mask_and_retries_after_delay);
    RUN_TEST(test_clearing_after_failed_start_resets_retry_window);
    return UNITY_END();
}
