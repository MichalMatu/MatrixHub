#ifdef UNIT_TEST

#include <unity.h>

#define private public
#include "../../src/system/matrix/MatrixTask.h"
#undef private

#include "../../src/system/logging/Logging.h"
#include "../../src/system/rtc/RtcConfig.h"
#include "../../src/system/watchdog/TaskWatchdog.h"
#include "../../src/ble/BleService.h"
#include "../../src/sensors/runtime/SensorState.h"
#include "../../src/sensors/imu/ImuManager.h"
#include "../../src/sensors/imu/ImuService.h"
#include "../../src/wifisensing/WifiSensingService.h"
#include "../../src/wifisensing/csi/core/CsiService.h"
#include "../../test/stubs/esp_heap_caps.h"

namespace RTC {

ConfigStore mockStore{};

const ConfigStore& getConfig() {
    return mockStore;
}

ConfigStore& getMutableConfig() {
    return mockStore;
}

void withConfig(const std::function<void(const ConfigStore&)>& reader) {
    reader(mockStore);
}

}  // namespace RTC

namespace LOG {

Settings Logging::_settings{ESP_LOG_INFO};

void Logging::begin(const Settings& settings) { _settings = settings; }
void Logging::setLevel(esp_log_level_t level) { _settings.level = level; }
Settings Logging::settings() { return _settings; }
bool Logging::isEnabled(esp_log_level_t level) {
    return level <= _settings.level;
}
void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    (void)level;
    (void)tag;
    (void)fmt;
}
void Logging::clearBuffer() {}
const char* Logging::levelToString(esp_log_level_t level) {
    (void)level;
    return "info";
}
esp_log_level_t Logging::stringToLevel(std::string_view name, esp_log_level_t fallback) {
    (void)name;
    return fallback;
}
void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {
    (void)taskName;
    (void)stackSize;
}
void Logging::logSection(const char* title) {
    (void)title;
}
void Logging::suppressNoisyModules() {}

}  // namespace LOG

namespace SYSTEM {

void TaskWatchdog::begin(uint32_t timeoutSec, bool panicOnTimeout) {
    (void)panicOnTimeout;
    _timeoutSec = timeoutSec;
    _initialized = true;
}

bool TaskWatchdog::registerCurrentTask() { return true; }
bool TaskWatchdog::registerTask(TaskHandle_t taskHandle) {
    (void)taskHandle;
    return true;
}
bool TaskWatchdog::unregisterCurrentTask() { return true; }
bool TaskWatchdog::unregisterTask(TaskHandle_t taskHandle) {
    (void)taskHandle;
    return true;
}
bool TaskWatchdog::reset() { return true; }

}  // namespace SYSTEM

namespace {

bool g_hasAccel = false;
float g_ax = 0.0f;
float g_ay = 0.0f;
float g_az = 0.0f;
bool g_hasSample = false;
IMU::ImuSample g_sample{};
uint32_t g_setConsumerCalls = 0;
bool g_lastConsumerActive = false;
IMU::Consumer g_lastConsumer = IMU::Consumer::AutoRotate;
SensorSnapshot g_sensorSnapshot{};
WIFISENSING::RssiStats g_rssiStats{};
WIFISENSING::CSI::CsiVisualizationSnapshot g_csiVisualization{};
uint32_t g_csiConsumerCalls = 0;
bool g_lastCsiConsumerActive = false;
WIFISENSING::CSI::CsiConsumer g_lastCsiConsumer = WIFISENSING::CSI::CsiConsumer::Frontend;
char g_lastBleRequestedMac[18] = {};
uint32_t g_bleSelectedLookupCalls = 0;
uint32_t g_bleSlotLookupCalls = 0;

}  // namespace

bool ImuService::getCachedAccel(float& x, float& y, float& z) const {
    x = g_ax;
    y = g_ay;
    z = g_az;
    return g_hasAccel;
}

bool ImuService::getCachedSample(float& ax, float& ay, float& az, float& gx, float& gy, float& gz) const {
    ax = g_sample.ax;
    ay = g_sample.ay;
    az = g_sample.az;
    gx = g_sample.gx;
    gy = g_sample.gy;
    gz = g_sample.gz;
    return g_hasSample;
}

bool ImuService::getCachedSample(IMU::ImuSample& sample) const {
    sample = g_sample;
    return g_hasSample;
}

namespace IMU {

ImuManager::ImuManager(ImuService* imuService)
    : _imuService(imuService) {}

ImuManager::~ImuManager() = default;

void ImuManager::setConsumerActive(Consumer consumer, bool active) {
    g_lastConsumer = consumer;
    g_lastConsumerActive = active;
    g_setConsumerCalls++;
}

}  // namespace IMU

namespace SENSORS {

SensorSnapshot SensorState::getLastGoodSnapshot() {
    return g_sensorSnapshot;
}

SensorSnapshot SensorState::getSnapshot() {
    return g_sensorSnapshot;
}

}  // namespace SENSORS

namespace BLE {

bool BleService::getCachedDeviceData(
    const char* mac,
    float& outTemp,
    float& outHumid,
    uint8_t& outBatt,
    int8_t& outRssi,
    uint32_t& outLastSeen) const {
    g_bleSelectedLookupCalls++;
    std::strncpy(g_lastBleRequestedMac, mac ? mac : "", sizeof(g_lastBleRequestedMac) - 1);
    g_lastBleRequestedMac[sizeof(g_lastBleRequestedMac) - 1] = '\0';
    outTemp = 21.5f;
    outHumid = 44.0f;
    outBatt = 90;
    outRssi = -55;
    outLastSeen = TEST_STUBS::ARDUINO::millisValue;
    return true;
}

bool BleService::getCachedDeviceDataAt(
    size_t slotIndex,
    const char*& outMac,
    float& outTemp,
    float& outHumid,
    uint8_t& outBatt,
    int8_t& outRssi,
    uint32_t& outLastSeen) const {
    (void)slotIndex;
    g_bleSlotLookupCalls++;
    static const char* kMac = "11:22:33:44:55:66";
    outMac = kMac;
    outTemp = 19.0f;
    outHumid = 50.0f;
    outBatt = 80;
    outRssi = -70;
    outLastSeen = TEST_STUBS::ARDUINO::millisValue;
    return true;
}

}  // namespace BLE

namespace WIFISENSING {

RssiStats WifiSensingService::getStats() const {
    return g_rssiStats;
}

uint16_t WifiSensingService::getSamples(RssiSample* outBuffer, uint16_t maxCount) const {
    if (!outBuffer || maxCount == 0) {
        return 0;
    }
    const uint16_t count = maxCount > 4 ? 4 : maxCount;
    for (uint16_t i = 0; i < count; ++i) {
        outBuffer[i].rssi = static_cast<int8_t>(-70 + i * 5);
        outBuffer[i].timestampMs = TEST_STUBS::ARDUINO::millisValue;
    }
    return count;
}

namespace CSI {

bool CsiService::setConsumerActive(CsiConsumer consumer, bool active) {
    g_lastCsiConsumer = consumer;
    g_lastCsiConsumerActive = active;
    g_csiConsumerCalls++;
    return active;
}

CsiVisualizationSnapshot CsiService::getVisualizationSnapshot() const {
    return g_csiVisualization;
}

}  // namespace CSI
}  // namespace WIFISENSING

#include "../../src/system/matrix/MatrixTask.cpp"

namespace MATRIX {

void MatrixMenuService::update() {}

}  // namespace MATRIX

namespace MATRIX_MANAGER {

void MatrixManagerService::update() {}

}  // namespace MATRIX_MANAGER

namespace {

void resetState() {
    memset(&RTC::mockStore, 0, sizeof(RTC::mockStore));
    TEST_STUBS::ARDUINO::millisValue = 0;
    g_hasAccel = false;
    g_ax = 0.0f;
    g_ay = 0.0f;
    g_az = 0.0f;
    g_hasSample = false;
    g_sample = IMU::ImuSample{};
    g_setConsumerCalls = 0;
    g_lastConsumerActive = false;
    g_lastConsumer = IMU::Consumer::AutoRotate;
    g_sensorSnapshot = SensorSnapshot{};
    g_rssiStats = WIFISENSING::RssiStats{};
    g_csiVisualization = WIFISENSING::CSI::CsiVisualizationSnapshot{};
    g_csiConsumerCalls = 0;
    g_lastCsiConsumerActive = false;
    g_lastCsiConsumer = WIFISENSING::CSI::CsiConsumer::Frontend;
    std::memset(g_lastBleRequestedMac, 0, sizeof(g_lastBleRequestedMac));
    g_bleSelectedLookupCalls = 0;
    g_bleSlotLookupCalls = 0;
    MATRIX::MatrixTask::resetAutoRotationState();
}

}  // namespace

void setUp() {
    resetState();
}

void tearDown() {}

void test_auto_rotate_reapplies_rotation_after_toggle() {
    MatrixService matrix;
    ImuService imu;
    IMU::ImuManager manager(&imu);

    RTC::mockStore.matrix.autoRotate = true;
    RTC::mockStore.matrix.rotation = 0;
    g_hasAccel = true;
    g_ax = 1.0f;
    g_ay = 0.0f;
    g_az = 0.0f;

    TEST_STUBS::ARDUINO::millisValue = 100;
    MATRIX::MatrixTask::evaluateAutoRotation(&imu, &manager, &matrix);

    TEST_ASSERT_EQUAL_UINT32(1, matrix.setRotationCalls);
    TEST_ASSERT_EQUAL_UINT8(1, matrix.lastRotation);
    TEST_ASSERT_EQUAL_UINT32(1, g_setConsumerCalls);
    TEST_ASSERT_EQUAL(IMU::Consumer::AutoRotate, g_lastConsumer);
    TEST_ASSERT_TRUE(g_lastConsumerActive);

    RTC::mockStore.matrix.autoRotate = false;
    TEST_STUBS::ARDUINO::millisValue = 200;
    MATRIX::MatrixTask::evaluateAutoRotation(&imu, &manager, &matrix);

    TEST_ASSERT_EQUAL_UINT32(2, g_setConsumerCalls);
    TEST_ASSERT_FALSE(g_lastConsumerActive);

    RTC::mockStore.matrix.autoRotate = true;
    TEST_STUBS::ARDUINO::millisValue = 210;
    MATRIX::MatrixTask::evaluateAutoRotation(&imu, &manager, &matrix);

    // Regression guard: toggling auto-rotate back on must clear the cached
    // last orientation so the current pose is applied again immediately.
    TEST_ASSERT_EQUAL_UINT32(2, matrix.setRotationCalls);
    TEST_ASSERT_EQUAL_UINT8(1, matrix.lastRotation);
    TEST_ASSERT_EQUAL_UINT32(3, g_setConsumerCalls);
    TEST_ASSERT_TRUE(g_lastConsumerActive);
}

void test_effect_input_activates_imu_consumer_for_native_3d_provider() {
    MatrixService matrix;
    ImuService imu;
    IMU::ImuManager manager(&imu);

    RTC::mockStore.matrix.effectEnabled = true;
    RTC::mockStore.matrix.effectEngine = 1;
    RTC::mockStore.matrix.effectReactivityProvider = 1;
    g_hasSample = true;
    g_sample.ax = 0.25f;
    g_sample.ay = -0.50f;
    g_sample.az = 0.90f;
    g_sample.gx = 45.0f;
    g_sample.gy = -20.0f;
    g_sample.gz = 10.0f;
    g_sample.timestampMs = 700;
    g_sample.valid = true;
    TEST_STUBS::ARDUINO::millisValue = 750;

    MATRIX::MatrixTask::evaluateEffectInput(&imu, &manager, &matrix);

    TEST_ASSERT_EQUAL_UINT32(1, g_setConsumerCalls);
    TEST_ASSERT_EQUAL(IMU::Consumer::MatrixEffects, g_lastConsumer);
    TEST_ASSERT_TRUE(g_lastConsumerActive);
    TEST_ASSERT_EQUAL_UINT32(1, matrix.setEffectInputCalls);
    TEST_ASSERT_TRUE(matrix.lastEffectInput.imuValid);
    TEST_ASSERT_EQUAL_FLOAT(0.25f, matrix.lastEffectInput.ax);
    TEST_ASSERT_EQUAL_FLOAT(-0.50f, matrix.lastEffectInput.ay);
    TEST_ASSERT_EQUAL_FLOAT(0.90f, matrix.lastEffectInput.az);
    TEST_ASSERT_EQUAL_FLOAT(45.0f, matrix.lastEffectInput.gx);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, matrix.lastEffectInput.motionEnergy);
    TEST_ASSERT_EQUAL_UINT32(750, matrix.lastEffectInput.timestampMs);
}

void test_effect_input_disables_imu_consumer_when_provider_no_longer_needs_it() {
    MatrixService matrix;
    ImuService imu;
    IMU::ImuManager manager(&imu);

    RTC::mockStore.matrix.effectEnabled = true;
    RTC::mockStore.matrix.effectEngine = 1;
    RTC::mockStore.matrix.effectReactivityProvider = 1;
    g_hasSample = true;
    g_sample.ax = 0.0f;
    g_sample.ay = 0.0f;
    g_sample.az = 1.0f;

    MATRIX::MatrixTask::evaluateEffectInput(&imu, &manager, &matrix);
    RTC::mockStore.matrix.effectEnabled = false;
    MATRIX::MatrixTask::evaluateEffectInput(&imu, &manager, &matrix);

    TEST_ASSERT_EQUAL_UINT32(2, g_setConsumerCalls);
    TEST_ASSERT_EQUAL(IMU::Consumer::MatrixEffects, g_lastConsumer);
    TEST_ASSERT_FALSE(g_lastConsumerActive);
    TEST_ASSERT_EQUAL_UINT32(2, matrix.setEffectInputCalls);
    TEST_ASSERT_FALSE(matrix.lastEffectInput.imuValid);
}

void test_data_visualization_scd4x_sets_sensor_input() {
    MatrixService matrix;

    RTC::mockStore.matrix.backgroundMode =
        static_cast<uint8_t>(MATRIX::MatrixBackgroundMode::DataVisualization);
    RTC::mockStore.matrix.dataVisualizationEnabled = true;
    RTC::mockStore.matrix.dataVisualizationSource =
        static_cast<uint8_t>(MATRIX::MatrixDataSource::Scd4x);
    RTC::mockStore.matrix.dataVisualizationMetric =
        static_cast<uint8_t>(MATRIX::MatrixDataMetric::Co2);
    RTC::mockStore.matrix.dataVisualizationMin = 400.0f;
    RTC::mockStore.matrix.dataVisualizationMax = 2000.0f;
    g_sensorSnapshot.co2 = 812;
    g_sensorSnapshot.temp = 22.0f;
    g_sensorSnapshot.humid = 45.0f;
    g_sensorSnapshot.timestamp_ms = 900;
    TEST_STUBS::ARDUINO::millisValue = 1000;

    MATRIX::MatrixTask::evaluateDataVisualizationInput(nullptr, nullptr, nullptr, &matrix);

    TEST_ASSERT_EQUAL_UINT32(1, matrix.setDataVisualizationInputCalls);
    TEST_ASSERT_TRUE(matrix.lastDataVisualizationInput.valid);
    TEST_ASSERT_FALSE(matrix.lastDataVisualizationInput.stale);
    TEST_ASSERT_EQUAL_FLOAT(812.0f, matrix.lastDataVisualizationInput.value);
    TEST_ASSERT_EQUAL_UINT32(1000, matrix.lastDataVisualizationInput.timestampMs);
}

void test_data_visualization_scd4x_keeps_stale_retained_value() {
    MatrixService matrix;

    RTC::mockStore.matrix.backgroundMode =
        static_cast<uint8_t>(MATRIX::MatrixBackgroundMode::DataVisualization);
    RTC::mockStore.matrix.dataVisualizationEnabled = true;
    RTC::mockStore.matrix.dataVisualizationSource =
        static_cast<uint8_t>(MATRIX::MatrixDataSource::Scd4x);
    RTC::mockStore.matrix.dataVisualizationMetric =
        static_cast<uint8_t>(MATRIX::MatrixDataMetric::Humidity);
    RTC::mockStore.matrix.dataVisualizationMin = 20.0f;
    RTC::mockStore.matrix.dataVisualizationMax = 80.0f;
    g_sensorSnapshot.co2 = 700;
    g_sensorSnapshot.temp = 22.0f;
    g_sensorSnapshot.humid = 54.0f;
    g_sensorSnapshot.timestamp_ms = 1000;
    TEST_STUBS::ARDUINO::millisValue = 1000 + SENSOR::SNAPSHOT_TIMEOUT_MS + 1;

    MATRIX::MatrixTask::evaluateDataVisualizationInput(nullptr, nullptr, nullptr, &matrix);

    TEST_ASSERT_EQUAL_UINT32(1, matrix.setDataVisualizationInputCalls);
    TEST_ASSERT_TRUE(matrix.lastDataVisualizationInput.valid);
    TEST_ASSERT_TRUE(matrix.lastDataVisualizationInput.stale);
    TEST_ASSERT_EQUAL_FLOAT(54.0f, matrix.lastDataVisualizationInput.value);
}

void test_data_visualization_ble_uses_selected_device_as_single_source() {
    MatrixService matrix;
    auto* ble = reinterpret_cast<BLE::BleService*>(0x1);

    RTC::mockStore.matrix.backgroundMode =
        static_cast<uint8_t>(MATRIX::MatrixBackgroundMode::DataVisualization);
    RTC::mockStore.matrix.dataVisualizationEnabled = true;
    RTC::mockStore.matrix.dataVisualizationSource =
        static_cast<uint8_t>(MATRIX::MatrixDataSource::BleThermometer);
    RTC::mockStore.matrix.dataVisualizationMetric =
        static_cast<uint8_t>(MATRIX::MatrixDataMetric::Humidity);
    std::strncpy(
        RTC::mockStore.matrix.dataVisualizationDeviceId,
        "AA:BB:CC:DD:EE:FF",
        sizeof(RTC::mockStore.matrix.dataVisualizationDeviceId) - 1);
    TEST_STUBS::ARDUINO::millisValue = 2000;

    MATRIX::MatrixTask::evaluateDataVisualizationInput(ble, nullptr, nullptr, &matrix);

    TEST_ASSERT_EQUAL_UINT32(1, g_bleSelectedLookupCalls);
    TEST_ASSERT_EQUAL_UINT32(0, g_bleSlotLookupCalls);
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:FF", g_lastBleRequestedMac);
    TEST_ASSERT_EQUAL_UINT32(1, matrix.setDataVisualizationInputCalls);
    TEST_ASSERT_TRUE(matrix.lastDataVisualizationInput.valid);
    TEST_ASSERT_FALSE(matrix.lastDataVisualizationInput.stale);
    TEST_ASSERT_EQUAL_FLOAT(44.0f, matrix.lastDataVisualizationInput.value);
    TEST_ASSERT_EQUAL_FLOAT(90.0f, matrix.lastDataVisualizationInput.secondary);
}

void test_data_visualization_csi_enables_consumer_and_forwards_bins() {
    MatrixService matrix;
    auto* csi = reinterpret_cast<WIFISENSING::CSI::CsiService*>(0x1);

    RTC::mockStore.matrix.backgroundMode =
        static_cast<uint8_t>(MATRIX::MatrixBackgroundMode::DataVisualization);
    RTC::mockStore.matrix.dataVisualizationEnabled = true;
    RTC::mockStore.matrix.dataVisualizationSource =
        static_cast<uint8_t>(MATRIX::MatrixDataSource::WifiCsi);
    RTC::mockStore.matrix.dataVisualizationMetric =
        static_cast<uint8_t>(MATRIX::MatrixDataMetric::CsiMotion);
    g_csiVisualization.valid = true;
    g_csiVisualization.stale = false;
    g_csiVisualization.timestampMs = 900;
    g_csiVisualization.width = 128;
    g_csiVisualization.value = 42.0f;
    g_csiVisualization.binCount = 64;
    for (uint8_t i = 0; i < 64; ++i) {
        g_csiVisualization.bins[i] = static_cast<uint8_t>(63u - i);
    }
    TEST_STUBS::ARDUINO::millisValue = 1000;

    MATRIX::MatrixTask::evaluateDataVisualizationInput(nullptr, nullptr, csi, &matrix);

    TEST_ASSERT_EQUAL_UINT32(1, g_csiConsumerCalls);
    TEST_ASSERT_EQUAL(WIFISENSING::CSI::CsiConsumer::MatrixVisualization, g_lastCsiConsumer);
    TEST_ASSERT_TRUE(g_lastCsiConsumerActive);
    TEST_ASSERT_EQUAL_UINT32(1, matrix.setDataVisualizationInputCalls);
    TEST_ASSERT_TRUE(matrix.lastDataVisualizationInput.valid);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, matrix.lastDataVisualizationInput.value);
    TEST_ASSERT_EQUAL_FLOAT(128.0f, matrix.lastDataVisualizationInput.secondary);
    TEST_ASSERT_EQUAL_UINT8(64, matrix.lastDataVisualizationInput.binCount);
    TEST_ASSERT_EQUAL_UINT8(63, matrix.lastDataVisualizationInput.bins[0]);
    TEST_ASSERT_EQUAL_UINT8(0, matrix.lastDataVisualizationInput.bins[63]);

    RTC::mockStore.matrix.backgroundMode =
        static_cast<uint8_t>(MATRIX::MatrixBackgroundMode::Effects);
    TEST_STUBS::ARDUINO::millisValue = 1300;
    MATRIX::MatrixTask::evaluateDataVisualizationInput(nullptr, nullptr, csi, &matrix);

    TEST_ASSERT_EQUAL_UINT32(2, g_csiConsumerCalls);
    TEST_ASSERT_FALSE(g_lastCsiConsumerActive);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_auto_rotate_reapplies_rotation_after_toggle);
    RUN_TEST(test_effect_input_activates_imu_consumer_for_native_3d_provider);
    RUN_TEST(test_effect_input_disables_imu_consumer_when_provider_no_longer_needs_it);
    RUN_TEST(test_data_visualization_scd4x_sets_sensor_input);
    RUN_TEST(test_data_visualization_scd4x_keeps_stale_retained_value);
    RUN_TEST(test_data_visualization_ble_uses_selected_device_as_single_source);
    RUN_TEST(test_data_visualization_csi_enables_consumer_and_forwards_bins);
    return UNITY_END();
}

#endif
