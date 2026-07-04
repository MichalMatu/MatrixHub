#ifdef NATIVE_BUILD
#include <unity.h>

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>
#include <WiFi.h>

#include "../../src/system/power/activity/ActivityMonitor.cpp"
#include "../../src/system/power/activity/ActivityLogger.cpp"

namespace {

struct CapturedLog {
    esp_log_level_t level;
    std::string tag;
    std::string message;
};

std::vector<CapturedLog> s_logs;

}  // namespace

namespace LOG {
Settings Logging::_settings{ESP_LOG_INFO};

void Logging::setLevel(esp_log_level_t level) {
    _settings.level = level;
}

void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    if (level > _settings.level) {
        return;
    }

    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    s_logs.push_back({
        level,
        tag ? tag : "",
        buffer,
    });
}

void Logging::logSection(const char* title) { (void)title; }

void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {
    (void)taskName;
    (void)stackSize;
}

const char* Logging::levelToString(esp_log_level_t level) {
    (void)level;
    return "info";
}

esp_log_level_t Logging::stringToLevel(std::string_view name, esp_log_level_t fallback) {
    (void)name;
    return fallback;
}
}  // namespace LOG

WiFiClass WiFi;

namespace {

bool s_sleepRequested = false;
const char* s_sleepReason = nullptr;

uint32_t s_inactivityTimeoutMs = 300000UL;
uint32_t s_gracePeriodMs = 0;
uint32_t s_timerWakeAwakeMs = 60000UL;
uint32_t s_buttonWakeAwakeMs = 120000UL;
bool s_sleepEnabled = true;

int stubApStations() {
    return 1;
}

void resetLogs(esp_log_level_t level = ESP_LOG_INFO) {
    s_logs.clear();
    LOG::Logging::setLevel(level);
}

}  // namespace

#include "../../src/system/power/PowerActivityTracker.cpp"

namespace POWER {

bool ActivityPersistenceManager::tryRestore(uint32_t& lastActivityMs, uint32_t& bootMs) {
    (void)lastActivityMs;
    (void)bootMs;
    return false;
}

void ActivityPersistenceManager::save(uint32_t lastActivityMs, uint32_t bootMs) {
    (void)lastActivityMs;
    (void)bootMs;
}

bool PowerSleepController::isSleepRequested() {
    return s_sleepRequested;
}

void PowerSleepController::requestSleep(const char* reason, uint32_t delayMs) {
    (void)delayMs;
    s_sleepRequested = true;
    s_sleepReason = reason;
}

uint32_t PowerSettings::getInactivityTimeout() const {
    return s_inactivityTimeoutMs;
}

uint32_t PowerSettings::getGracePeriod() const {
    return s_gracePeriodMs;
}

bool PowerSettings::getSleepEnabled() const {
    return s_sleepEnabled;
}

uint32_t PowerSettings::getAwakeWindowForWakeReason(WakeReason reason) const {
    switch (reason) {
        case WakeReason::Timer:
            return s_timerWakeAwakeMs;
        case WakeReason::Button:
        case WakeReason::Touch:
            return s_buttonWakeAwakeMs;
        default:
            return 0;
    }
}

}  // namespace POWER

void setUp(void) {
    TEST_STUBS::ARDUINO::millisValue = 0;
    resetLogs();
    s_sleepRequested = false;
    s_sleepReason = nullptr;
    s_inactivityTimeoutMs = 300000UL;
    s_gracePeriodMs = 0;
    s_timerWakeAwakeMs = 60000UL;
    s_buttonWakeAwakeMs = 120000UL;
    s_sleepEnabled = true;
    POWER::ActivityLogger::begin();
    POWER::ActivityMonitor::begin(0, 0);
}

void tearDown(void) {}

void test_websocket_activity_is_hidden_from_info_logs() {
    POWER::ActivityLogger::logActivity("ws/system", 1000);
    POWER::ActivityLogger::logActivity("ws/usbterminal", 2000);
    POWER::ActivityLogger::logActivity("button", 3000);

    TEST_ASSERT_EQUAL_UINT32(1, s_logs.size());
    TEST_ASSERT_EQUAL(ESP_LOG_INFO, s_logs[0].level);
    TEST_ASSERT_EQUAL_STRING("[Power] Activity: button", s_logs[0].message.c_str());
}

void test_websocket_activity_is_rate_limited_even_at_debug() {
    resetLogs(ESP_LOG_DEBUG);
    POWER::ActivityLogger::begin();

    POWER::ActivityLogger::logActivity("ws/system", 1000);
    POWER::ActivityLogger::logActivity("ws/usbterminal", 2000);
    POWER::ActivityLogger::logActivity("ws/system", 61000);

    TEST_ASSERT_EQUAL_UINT32(2, s_logs.size());
    TEST_ASSERT_EQUAL(ESP_LOG_DEBUG, s_logs[0].level);
    TEST_ASSERT_EQUAL_STRING("[Power] Activity: ws/system", s_logs[0].message.c_str());
    TEST_ASSERT_EQUAL_STRING("[Power] Activity: ws/system", s_logs[1].message.c_str());
}

void test_websocket_activity_refreshes_timer_without_info_spam() {
    POWER::PowerActivityTracker tracker;

    TEST_STUBS::ARDUINO::millisValue = 5000;
    tracker.notifyActivity("ws/system");

    TEST_ASSERT_EQUAL_UINT32(5000, POWER::ActivityMonitor::getLastActivityMs());
    TEST_ASSERT_EQUAL_UINT32(0, s_logs.size());
}

void test_ap_station_presence_refreshes_activity_without_spam() {
    POWER::PowerActivityTracker tracker;
    POWER::PowerSleepController sleepController;
    POWER::PowerSettings settings;

    tracker.setApStationsProvider(stubApStations);

    TEST_STUBS::ARDUINO::millisValue = 1000;
    tracker.loopTick(sleepController, settings);
    TEST_ASSERT_EQUAL_UINT32(1000, POWER::ActivityMonitor::getLastActivityMs());
    TEST_ASSERT_EQUAL_UINT32(1, s_logs.size());
    TEST_ASSERT_EQUAL_STRING("[Power] AP client detected (1). Resetting inactivity timer.", s_logs[0].message.c_str());

    TEST_STUBS::ARDUINO::millisValue = 2000;
    tracker.loopTick(sleepController, settings);
    TEST_ASSERT_EQUAL_UINT32(2000, POWER::ActivityMonitor::getLastActivityMs());
    TEST_ASSERT_EQUAL_UINT32(1, s_logs.size());
}

void test_timer_wake_window_requests_sleep_when_no_activity() {
    POWER::PowerActivityTracker tracker;
    POWER::PowerSleepController sleepController;
    POWER::PowerSettings settings;

    TEST_STUBS::ARDUINO::millisValue = 0;
    tracker.begin(POWER::WakeReason::Timer);

    TEST_STUBS::ARDUINO::millisValue = 59000;
    tracker.loopTick(sleepController, settings);
    TEST_ASSERT_FALSE(s_sleepRequested);

    TEST_STUBS::ARDUINO::millisValue = 60000;
    tracker.loopTick(sleepController, settings);
    TEST_ASSERT_TRUE(s_sleepRequested);
    TEST_ASSERT_EQUAL_STRING("timer-wake-window", s_sleepReason);
}

void test_timer_wake_window_is_cancelled_by_client_activity() {
    POWER::PowerActivityTracker tracker;
    POWER::PowerSleepController sleepController;
    POWER::PowerSettings settings;

    TEST_STUBS::ARDUINO::millisValue = 0;
    tracker.begin(POWER::WakeReason::Timer);

    TEST_STUBS::ARDUINO::millisValue = 10000;
    tracker.notifyActivity("ws/system");

    TEST_STUBS::ARDUINO::millisValue = 60000;
    tracker.loopTick(sleepController, settings);
    TEST_ASSERT_FALSE(s_sleepRequested);
}

void test_button_wake_window_uses_admin_timeout() {
    POWER::PowerActivityTracker tracker;
    POWER::PowerSleepController sleepController;
    POWER::PowerSettings settings;

    TEST_STUBS::ARDUINO::millisValue = 0;
    tracker.begin(POWER::WakeReason::Button);

    TEST_STUBS::ARDUINO::millisValue = 119000;
    tracker.loopTick(sleepController, settings);
    TEST_ASSERT_FALSE(s_sleepRequested);

    TEST_STUBS::ARDUINO::millisValue = 120000;
    tracker.loopTick(sleepController, settings);
    TEST_ASSERT_TRUE(s_sleepRequested);
    TEST_ASSERT_EQUAL_STRING("button-wake-window", s_sleepReason);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_websocket_activity_is_hidden_from_info_logs);
    RUN_TEST(test_websocket_activity_is_rate_limited_even_at_debug);
    RUN_TEST(test_websocket_activity_refreshes_timer_without_info_spam);
    RUN_TEST(test_ap_station_presence_refreshes_activity_without_spam);
    RUN_TEST(test_timer_wake_window_requests_sleep_when_no_activity);
    RUN_TEST(test_timer_wake_window_is_cancelled_by_client_activity);
    RUN_TEST(test_button_wake_window_uses_admin_timeout);
    return UNITY_END();
}
#endif  // NATIVE_BUILD
