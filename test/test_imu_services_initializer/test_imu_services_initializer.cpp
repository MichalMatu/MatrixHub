#include <unity.h>

#include <cstdarg>
#include <memory>

#include "../../src/system/init/services/ImuServicesInitializer.cpp"

namespace {

bool gRetainedTrigger = false;
uint32_t gRetainedQueryCount = 0;
ALARMS::AlarmSource gRetainedQuerySource = ALARMS::AlarmSource::CO2;
bool gRuntimeBeginRetained = false;
uint32_t gRuntimeBeginCount = 0;
FS* gRuntimeFs = nullptr;
ImuService* gRuntimeImuService = nullptr;
IMU::ImuManager* gRuntimeManager = nullptr;
ALARMS::AlarmService* gRuntimeAlarmService = nullptr;
RTC::ConfigStore gRtcConfig{};

}  // namespace

namespace LOG {

void Logging::log(esp_log_level_t, const char*, const char*, ...) {}

}  // namespace LOG

namespace RTC {

SemaphoreHandle_t getLock() {
    static SemaphoreHandle_t lock = xSemaphoreCreateMutex();
    return lock;
}

const ConfigStore& getConfig() {
    return gRtcConfig;
}

ConfigStore& getMutableConfig() {
    return gRtcConfig;
}

void markValid() {}

}  // namespace RTC

namespace ALARMS {

bool AlarmService::isSourceTriggered(AlarmSource source) {
    ++gRetainedQueryCount;
    gRetainedQuerySource = source;
    return gRetainedTrigger;
}

}  // namespace ALARMS

namespace IMU {

ImuManager::ImuManager(ImuService* imuService)
    : _imuService(imuService) {}

ImuManager::~ImuManager() = default;

ImuRuntimeService::ImuRuntimeService(FS* fs,
                                     ImuService* imuService,
                                     ImuManager* imuManager,
                                     ALARMS::AlarmService* alarmService)
    : RtcStatefulService(&RTC::ConfigStore::imu),
      _fs(fs),
      _imuService(imuService),
      _imuManager(imuManager),
      _alarmService(alarmService) {
    gRuntimeFs = fs;
    gRuntimeImuService = imuService;
    gRuntimeManager = imuManager;
    gRuntimeAlarmService = alarmService;
}

void ImuRuntimeService::begin(bool retainedAlarmTriggered) {
    ++gRuntimeBeginCount;
    gRuntimeBeginRetained = retainedAlarmTriggered;
}

}  // namespace IMU

void setUp() {
    gRetainedTrigger = false;
    gRetainedQueryCount = 0;
    gRetainedQuerySource = ALARMS::AlarmSource::CO2;
    gRuntimeBeginRetained = false;
    gRuntimeBeginCount = 0;
    gRuntimeFs = nullptr;
    gRuntimeImuService = nullptr;
    gRuntimeManager = nullptr;
    gRuntimeAlarmService = nullptr;
    gRtcConfig = RTC::ConfigStore{};
}

void tearDown() {}

void test_initializer_restores_retained_imu_alarm_before_runtime_begin() {
    FS fs;
    std::unique_ptr<ImuService> imuService;
    std::unique_ptr<IMU::ImuManager> imuManager;
    std::unique_ptr<IMU::ImuRuntimeService> runtime;
    auto* alarmService = reinterpret_cast<ALARMS::AlarmService*>(0x1234);
    gRetainedTrigger = true;

    ImuServicesInitializer::initialize(
        {imuService, imuManager, runtime, alarmService, &fs});

    TEST_ASSERT_EQUAL_UINT32(1, gRetainedQueryCount);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ALARMS::AlarmSource::ImuTamper),
        static_cast<uint8_t>(gRetainedQuerySource));
    TEST_ASSERT_EQUAL_UINT32(1, gRuntimeBeginCount);
    TEST_ASSERT_TRUE(gRuntimeBeginRetained);
    TEST_ASSERT_EQUAL_PTR(&fs, gRuntimeFs);
    TEST_ASSERT_EQUAL_PTR(imuService.get(), gRuntimeImuService);
    TEST_ASSERT_EQUAL_PTR(imuManager.get(), gRuntimeManager);
    TEST_ASSERT_EQUAL_PTR(alarmService, gRuntimeAlarmService);
}

void test_initializer_begins_clear_when_no_retained_alarm_exists() {
    FS fs;
    std::unique_ptr<ImuService> imuService;
    std::unique_ptr<IMU::ImuManager> imuManager;
    std::unique_ptr<IMU::ImuRuntimeService> runtime;
    auto* alarmService = reinterpret_cast<ALARMS::AlarmService*>(0x5678);

    ImuServicesInitializer::initialize(
        {imuService, imuManager, runtime, alarmService, &fs});

    TEST_ASSERT_EQUAL_UINT32(1, gRuntimeBeginCount);
    TEST_ASSERT_FALSE(gRuntimeBeginRetained);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_initializer_restores_retained_imu_alarm_before_runtime_begin);
    RUN_TEST(test_initializer_begins_clear_when_no_retained_alarm_exists);
    return UNITY_END();
}
