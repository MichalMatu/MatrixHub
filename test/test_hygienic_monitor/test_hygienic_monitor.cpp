#ifdef NATIVE_BUILD
#include <unity.h>
#include <Arduino.h>

static uint32_t _mockFreeHeap = 200000;
static uint32_t _mockLargestBlock = 100000;
static uint32_t _mockMillis = 1000;

#ifdef __cplusplus
extern "C" {
#endif
    size_t heap_caps_get_free_size(uint32_t caps) { return _mockFreeHeap; }
    size_t heap_caps_get_largest_free_block(uint32_t caps) { return _mockLargestBlock; }
    size_t heap_caps_get_total_size(uint32_t caps) { return 300000; }
    size_t heap_caps_get_minimum_free_size(uint32_t caps) { return _mockFreeHeap; }
    size_t esp_psram_get_size() { return 0; }
    uint32_t esp_get_minimum_free_heap_size() { return _mockFreeHeap; }
    uint32_t esp_get_free_heap_size() { return _mockFreeHeap; }
#ifdef __cplusplus
}
#endif

// Define millis natively
#define millis() _mockMillis

namespace POWER { class PowerManager; }
class ServiceRegistry {
public:
    POWER::PowerManager* getPowerManager();
};

class Application {
public:
    static Application& instance() { static Application app; return app; }
    ServiceRegistry* getServiceRegistry();
};
#include "../../src/system/health/heap/HeapMonitor.cpp"

using namespace SYSTEM;

// --- LINKER MOCKS FOR DEPENDENCIES ---
namespace LOG {
    Settings Logging::_settings;
    void Logging::log(esp_log_level_t level, const char *tag, const char *fmt, ...) {}
    void Logging::logSection(const char* title) {}
    void Logging::logStackHwm(const char* name, uint32_t period) {}
}

namespace RTC {
    RtcRuntimeStats runtimeStats;
    void prepareForSleep() {}
    void getBootReasonName(esp_reset_reason_t r, char* out, size_t len) {}
    void getSleepWakeupReasonName(esp_sleep_wakeup_cause_t c, char* out, size_t len) {}
    bool markMaintenanceSleepPending(uint32_t nowMs, float thermalShutdownTemp) {
        runtimeStats.hygieneSleepActive = true;
        runtimeStats.hygieneSleepCount++;
        runtimeStats.lastHygieneSleepMs = nowMs;
        runtimeStats.lastThermalShutdownTemp = thermalShutdownTemp;
        return true;
    }
}

namespace PERSIST {
    void incrementHygieneSleep() {}
    void saveSystemState() {}
}

// Linker mocks for PowerManager and BootTracker
static bool _test_sleep_requested = false;
static uint32_t _test_wake_int = 0;

void SYSTEM::BootTracker::recordShutdown(ShutdownReason reason) {}

namespace POWER {
    void PowerManager::requestSleep(const char* reason, uint32_t delay) { _test_sleep_requested = true; }
    void PowerManager::setWakeInterval(uint32_t interval) { _test_wake_int = interval; }
    bool PowerManager::isSleepRequested() { return _test_sleep_requested; }
}

// Application / ServiceRegistry mocks
static POWER::PowerManager dummyPower;
ServiceRegistry* Application::getServiceRegistry() {
    static ServiceRegistry registry;
    return &registry;
}
POWER::PowerManager* ServiceRegistry::getPowerManager() {
    return &dummyPower;
}

// Minimal BleLifecycleManager mock to satisfy BleService
namespace BLE {
    class BleLifecycleManager {
    public:
        void update() {}
        bool isShuttingDown() { return false; }
    };
}
// ------------------------------------

class HeapMonitorTestable : public HeapMonitor {
public:
    void resetState() {
        _highFragStartMs = 0;
        _initialized = false;
        RTC::runtimeStats = {};
        _test_sleep_requested = false;
        _test_wake_int = 0;
    }
    uint32_t getHighFragStartMs() const { return _highFragStartMs; }
    bool invokeCheckHygieneAt(uint32_t timeMs, uint32_t freeH, uint32_t largestB) {
        _mockMillis = timeMs;
        _mockFreeHeap = freeH;
        _mockLargestBlock = largestB;
        return checkHygieneConditions();
    }
};

static HeapMonitorTestable* monitor;

static void wireRuntimeDependencies() {
    monitor->setPowerManager(&dummyPower);
}

void setUp(void) {
    monitor = static_cast<HeapMonitorTestable*>(&HeapMonitor::instance());
    monitor->resetState();
    wireRuntimeDependencies();
    _mockMillis = 1000;
}

void tearDown(void) {}

void test_healthy_heap_no_trigger() {
    bool triggered = monitor->invokeCheckHygieneAt(1000, 200000, 150000); // 25% frag
    TEST_ASSERT_FALSE(triggered);
    TEST_ASSERT_EQUAL(0, monitor->getHighFragStartMs());
    TEST_ASSERT_FALSE(RTC::runtimeStats.hygieneSleepActive);
}

void test_critical_fragmentation() {
    // Dip below threshold (30719 bytes natively without PSRAM) past 10 minutes
    bool triggered2 = monitor->invokeCheckHygieneAt(603000, 100000, 30719);
    TEST_ASSERT_TRUE(triggered2);
    TEST_ASSERT_TRUE(RTC::runtimeStats.hygieneSleepActive);
    TEST_ASSERT_TRUE(_test_sleep_requested);
    TEST_ASSERT_EQUAL(100, _test_wake_int);
}

void test_sustained_fragmentation() {
    // T=605000 (~10m 5s): Initial high fragmentation (~76%)
    bool triggered1 = monitor->invokeCheckHygieneAt(605000, 150000, 35000);
    TEST_ASSERT_FALSE(triggered1);
    TEST_ASSERT_EQUAL(605000, monitor->getHighFragStartMs());
    
    // T=750000: Mid-way through (2.5 mins in)
    bool triggered2 = monitor->invokeCheckHygieneAt(750000, 150000, 35000);
    TEST_ASSERT_FALSE(triggered2);
    
    // T=906000: Past the 5-minute threshold
    bool triggered3 = monitor->invokeCheckHygieneAt(906000, 150000, 35000);
    TEST_ASSERT_TRUE(triggered3);
    TEST_ASSERT_TRUE(RTC::runtimeStats.hygieneSleepActive);
}

void test_hygiene_rate_limit() {
    // Highly fragmented, but only 5 minutes uptime (kMinHygieneIntervalMs is 10 min)
    bool triggered = monitor->invokeCheckHygieneAt(300000, 150000, 20000); 
    TEST_ASSERT_FALSE(triggered);
    
    // Past 10 minutes
    bool triggered2 = monitor->invokeCheckHygieneAt(601000, 150000, 20000); 
    TEST_ASSERT_TRUE(triggered2);
}

void test_fragmentation_timer_reset() {
    monitor->invokeCheckHygieneAt(605000, 150000, 35000); // 76% frag
    TEST_ASSERT_EQUAL(605000, monitor->getHighFragStartMs());
    
    // Recover (30% frag)
    monitor->invokeCheckHygieneAt(610000, 150000, 105000);
    TEST_ASSERT_EQUAL(0, monitor->getHighFragStartMs());
    
    // High frag again
    monitor->invokeCheckHygieneAt(615000, 150000, 35000);
    TEST_ASSERT_EQUAL(615000, monitor->getHighFragStartMs());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_healthy_heap_no_trigger);
    RUN_TEST(test_critical_fragmentation);
    RUN_TEST(test_sustained_fragmentation);
    RUN_TEST(test_hygiene_rate_limit);
    RUN_TEST(test_fragmentation_timer_reset);
    return UNITY_END();
}
#endif // NATIVE_BUILD
