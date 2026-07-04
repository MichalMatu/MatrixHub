#ifdef NATIVE_BUILD
#include <unity.h>
#include <Arduino.h>

static uint32_t _mockMillis = 1000;
#define millis() _mockMillis

namespace SleepService {
    void executeSleepCallbacks();
}

// Include the source directly so it sees all dummy classes below if needed, OR we include it AFTER providing mocks.
// Real headers will be resolved. We provide linker implementations for missing pieces.
#include "../../src/system/power/PowerSleepController.cpp"

void SleepService::executeSleepCallbacks() {}

extern "C" {
    void esp_sleep_pd_config(int domain, int option) {}
    void esp_deep_sleep_start() {}
}

namespace LOG {
    Settings Logging::_settings;
    void Logging::log(esp_log_level_t level, const char *tag, const char *fmt, ...) {}
    void Logging::logSection(const char* title) {}
    void Logging::logStackHwm(const char* name, uint32_t period) {}
}

namespace POWER {
    // Provide dummy WakeController for the setup pointer
    bool PowerWakeController::configureWakeSources(const WakeSourceConfig& config) {
        (void)config;
        return true;
    }
}

namespace RTC {
    void prepareForSleep() {}
}

// Test spies
static bool _test_pre_sleep_hook_called = false;
static bool _test_sleep_callback_called = false;
static const char* _test_sleep_reason = nullptr;

void test_sleep_callback(const char* reason) {
    _test_sleep_callback_called = true;
    _test_sleep_reason = reason;
}

void test_pre_sleep_hook() {
    _test_pre_sleep_hook_called = true;
}

static POWER::PowerSleepController sleepController;
static POWER::PowerWakeController dummyWake;

void setUp(void) {
    _mockMillis = 1000;
    _test_pre_sleep_hook_called = false;
    _test_sleep_callback_called = false;
    _test_sleep_reason = nullptr;
    
    sleepController.begin(&dummyWake);
    sleepController.resetTestHooks();
    sleepController.setSleepCallback(test_sleep_callback);
}

void tearDown(void) {}

void test_immediate_sleep_request() {
    sleepController.requestSleep("immediate_test", 0);
    
    TEST_ASSERT_TRUE(sleepController.isSleepRequested());
    TEST_ASSERT_EQUAL_STRING("immediate_test", sleepController.getSleepReason());
    
    // With async-only model, sleep is deferred to loopTick (min 50ms delay)
    TEST_ASSERT_FALSE(_test_sleep_callback_called);
    
    // Advance time past the minimum delay and tick
    _mockMillis += 60;
    sleepController.loopTick();
    TEST_ASSERT_TRUE(_test_sleep_callback_called);
    TEST_ASSERT_EQUAL_STRING("immediate_test", _test_sleep_reason);
}

void test_delayed_sleep_request() {
    sleepController.requestSleep("delayed_test", 5000); // 5 sec delay
    
    TEST_ASSERT_TRUE(sleepController.isSleepRequested());
    TEST_ASSERT_EQUAL_STRING("delayed_test", sleepController.getSleepReason());
    TEST_ASSERT_FALSE(_test_sleep_callback_called);
    
    // Check initial ETA
    TEST_ASSERT_EQUAL(5000, sleepController.getSleepEtaMs());
    
    // Advance time by 2 seconds
    _mockMillis += 2000;
    TEST_ASSERT_EQUAL(3000, sleepController.getSleepEtaMs());
    sleepController.loopTick(); // Shouldn't sleep yet
    TEST_ASSERT_FALSE(_test_sleep_callback_called);
    
    // Advance time past the delay
    _mockMillis += 3500;
    // ETA should hit 0
    TEST_ASSERT_EQUAL(0, sleepController.getSleepEtaMs());
    
    // Call loopTick to trigger the actual sleep
    sleepController.loopTick();
    TEST_ASSERT_TRUE(_test_sleep_callback_called);
    TEST_ASSERT_EQUAL_STRING("delayed_test", _test_sleep_reason);
}

void test_delayed_sleep_with_rollover() {
    // 10 seconds before millis() rolls over at 49 days (0xFFFFFFFF = 4294967295)
    _mockMillis = 0xFFFFFFFF - 10000;
    
    // Request a sleep in 20 seconds (crosses the rollover line)
    sleepController.requestSleep("rollover_test", 20000); 
    
    TEST_ASSERT_TRUE(sleepController.isSleepRequested());
    TEST_ASSERT_EQUAL(20000, sleepController.getSleepEtaMs());
    
    // Advance time 5 seconds (still before rollover)
    _mockMillis += 5000; 
    sleepController.loopTick();
    TEST_ASSERT_FALSE(_test_sleep_callback_called);
    TEST_ASSERT_EQUAL(15000, sleepController.getSleepEtaMs());
    
    // Advance time 10 seconds (this CROSSES the rollover line)
    // 0xFFFFFFFF - 5000 + 10000 -> rollover to 4999
    _mockMillis += 10000; 
    sleepController.loopTick();
    TEST_ASSERT_FALSE(_test_sleep_callback_called);
    
    // Using subtraction-based math (current - start > delay), the ETA should still be correct
    TEST_ASSERT_EQUAL(5000, sleepController.getSleepEtaMs());
    
    // Advance past the target
    _mockMillis += 6000;
    sleepController.loopTick();
    TEST_ASSERT_TRUE(_test_sleep_callback_called);
    TEST_ASSERT_EQUAL_STRING("rollover_test", _test_sleep_reason);
}

void test_pre_sleep_hook_execution() {
    // If we DO NOT set the test callback, the controller will execute its full enterDeepSleep logic
    sleepController.resetTestHooks();
    sleepController.setPreSleepHook(test_pre_sleep_hook);
    
    sleepController.requestSleep("hook_test", 0);
    TEST_ASSERT_FALSE(_test_pre_sleep_hook_called);
    
    // Advance past minimum delay and trigger via loopTick
    _mockMillis += 60;
    sleepController.loopTick();
    TEST_ASSERT_TRUE(_test_pre_sleep_hook_called);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_immediate_sleep_request);
    RUN_TEST(test_delayed_sleep_request);
    RUN_TEST(test_delayed_sleep_with_rollover);
    RUN_TEST(test_pre_sleep_hook_execution);
    return UNITY_END();
}
#endif // NATIVE_BUILD
