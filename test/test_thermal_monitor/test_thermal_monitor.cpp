#ifdef NATIVE_BUILD
#include <unity.h>
#include <Arduino.h>

// ESP32 System Macros
#define ESP_PWR_LVL_P9 7
typedef int esp_power_level_t;

// ESP WiFi Ps Type
typedef enum {
    WIFI_PS_NONE,
    WIFI_PS_MIN_MODEM,
    WIFI_PS_MAX_MODEM
} wifi_ps_type_t;

#define MALLOC_CAP_INTERNAL 0
#define MALLOC_CAP_8BIT 0
#define xTaskCreateStaticPinnedToCore(...) (void*)1
#define xTaskGetCurrentTaskHandle() (void*)1

// Fakes
static float _mockTemp = 40.0f;
static uint32_t _mockFreq = 240;
static wifi_ps_type_t _mockPsType = WIFI_PS_NONE;

#ifdef __cplusplus
extern "C" {
#endif
    typedef int esp_err_t;
    float temperatureRead() { return _mockTemp; }
    void setCpuFrequencyMhz(uint32_t freq) { _mockFreq = freq; }
    uint32_t getCpuFrequencyMhz() { return _mockFreq; }
    esp_err_t esp_wifi_set_ps(wifi_ps_type_t type) { _mockPsType = type; return 0; }
#ifdef __cplusplus
}
#endif
#include "../../src/config/Hardware.h"
#include "../../src/config/System.h"
#include "../../src/system/rtc/RtcConfig.h"
#include "../../src/system/rtc/types/RtcRuntimeTypes.h"
#include "../../src/system/thermal/ThermalMonitor.h"
#include "../../src/system/power/PowerManager.h"

namespace RTC {
    RtcRuntimeStats runtimeStats;
    bool markMaintenanceSleepPending(uint32_t nowMs, float thermalShutdownTemp) {
        runtimeStats.hygieneSleepActive = true;
        runtimeStats.hygieneSleepCount++;
        runtimeStats.lastHygieneSleepMs = nowMs;
        runtimeStats.lastThermalShutdownTemp = thermalShutdownTemp;
        return true;
    }
}

namespace LOG {
    Settings Logging::_settings;
    void Logging::log(esp_log_level_t level, const char *tag, const char *fmt, ...) {}
    void Logging::logSection(const char* title) {}
    void Logging::logStackHwm(const char *taskName, uint32_t periodMs) {}
}

// Linker mocks
#include <WiFi.h>
WiFiClass WiFi;
static bool _test_sleep_requested = false;
namespace POWER {
    void PowerManager::requestSleep(const char *reason, uint32_t delayMs) { _test_sleep_requested = true; }
    void PowerManager::setWakeInterval(uint32_t intervalMs) {}
    bool PowerManager::isSleepRequested() { return _test_sleep_requested; }
}

#include "../../src/system/thermal/ThermalMonitor.cpp"

using namespace SYSTEM;

// Extractor helper
class ThermalMonitorTestable : public ThermalMonitor {
public:
    void testEvaluate(float temp) { evaluateAndApply(temp); }
    void resetState() { _state = ThermalState::NORMAL; _currentFreq = THERMAL::FREQ_NORMAL; }
};

static ThermalMonitorTestable* monitor;
static MatrixService mockMatrix;
static POWER::PowerManager mockPower;

void setUp(void) {
    monitor = static_cast<ThermalMonitorTestable*>(&ThermalMonitor::instance());
    monitor->setMatrixService(&mockMatrix);
    monitor->setPowerManager(&mockPower);
    monitor->setBleService(nullptr); // Excluded natively
    
    _mockTemp = 40.0f;
    _mockFreq = 240;
    mockMatrix.lastLimit = 255;
    _test_sleep_requested = false;
    RTC::runtimeStats.hygieneSleepActive = false;
    monitor->resetState();
}

void tearDown(void) {}

void test_normal_state_threshold() {
    TEST_ASSERT_EQUAL(ThermalState::NORMAL, monitor->getState());
    monitor->testEvaluate(45.0f);
    TEST_ASSERT_EQUAL(ThermalState::NORMAL, monitor->getState());
    TEST_ASSERT_EQUAL(240, _mockFreq);
    TEST_ASSERT_EQUAL(255, mockMatrix.lastLimit);
}

void test_soft_throttle() {
    monitor->testEvaluate(THERMAL::TEMP_SOFT_THROTTLE + 1.0f);
    TEST_ASSERT_EQUAL(ThermalState::SOFT_THROTTLE, monitor->getState());
    TEST_ASSERT_EQUAL(THERMAL::FREQ_SOFT, _mockFreq);
    TEST_ASSERT_EQUAL(16, mockMatrix.lastLimit);
}

void test_hard_throttle() {
    monitor->testEvaluate(THERMAL::TEMP_HARD_THROTTLE + 1.0f);
    TEST_ASSERT_EQUAL(ThermalState::HARD_THROTTLE, monitor->getState());
    TEST_ASSERT_EQUAL(THERMAL::FREQ_HARD, _mockFreq);
    TEST_ASSERT_EQUAL(2, mockMatrix.lastLimit);
    TEST_ASSERT_EQUAL(WIFI_PS_MAX_MODEM, _mockPsType);
}

void test_critical_shutdown() {
    monitor->testEvaluate(THERMAL::TEMP_CRITICAL + 1.0f);
    TEST_ASSERT_EQUAL(ThermalState::CRITICAL, monitor->getState());
    TEST_ASSERT_EQUAL(0, mockMatrix.lastLimit);
    TEST_ASSERT_TRUE(mockPower.isSleepRequested());
    TEST_ASSERT_TRUE(RTC::runtimeStats.hygieneSleepActive);
}

void test_hysteresis_recovery() {
    monitor->testEvaluate(THERMAL::TEMP_SOFT_THROTTLE + 1.0f);
    TEST_ASSERT_EQUAL(ThermalState::SOFT_THROTTLE, monitor->getState());
    
    // Cool down exactly to boundary (should NOT recover due to hysteresis)
    monitor->testEvaluate(THERMAL::TEMP_SOFT_THROTTLE - 0.1f);
    TEST_ASSERT_EQUAL(ThermalState::SOFT_THROTTLE, monitor->getState());
    
    // Cool down PAST hysteresis
    monitor->testEvaluate(THERMAL::TEMP_SOFT_THROTTLE - THERMAL::HYSTERESIS - 1.0f);
    TEST_ASSERT_EQUAL(ThermalState::NORMAL, monitor->getState());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_normal_state_threshold);
    RUN_TEST(test_soft_throttle);
    RUN_TEST(test_hard_throttle);
    RUN_TEST(test_critical_shutdown);
    RUN_TEST(test_hysteresis_recovery);
    return UNITY_END();
}
#endif // NATIVE_BUILD
