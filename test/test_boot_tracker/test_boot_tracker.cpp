#include <unity.h>

#include <cstdarg>
#include <cstdint>

#define private public
#include "../../src/system/boot/BootTracker.cpp"
#undef private

namespace TEST_STUBS::HEAP {
inline uint32_t freeHeap = 0;
}

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
uint32_t HeapMonitor::getFreeHeap() const {
    return TEST_STUBS::HEAP::freeHeap;
}
}  // namespace SYSTEM

namespace {

void resetBootTrackerState() {
    TEST_STUBS::ESP::reset();
    TEST_STUBS::ARDUINO::millisValue = 0;
    TEST_STUBS::HEAP::freeHeap = 0;

    SYSTEM::rtcBootState = {};
    SYSTEM::BootTracker::_state = {};
    SYSTEM::BootTracker::_lastSessionState = {};
    SYSTEM::BootTracker::_initialized = false;
    SYSTEM::BootTracker::_lastBootUnexpected = false;
}

SYSTEM::BootState makeValidRtcState() {
    SYSTEM::BootState state{};
    state.magic = SYSTEM::BootTracker::kMagic;
    state.bootCount = 7;
    state.lastShutdownMs = 55;
    state.lastUptimeMs = 66;
    state.lastShutdownReason = static_cast<uint8_t>(SYSTEM::ShutdownReason::CLEAN_SLEEP);
    state.lastResetReason = static_cast<uint8_t>(ESP_RST_DEEPSLEEP);
    state.unexpectedRestarts = 2;
    state.freeHeapAtShutdown = 777;
    return state;
}

}  // namespace

void setUp(void) {
    resetBootTrackerState();
}

void tearDown(void) {}

void test_begin_starts_fresh_when_rtc_magic_is_invalid() {
    TEST_STUBS::ESP::resetReason = ESP_RST_POWERON;

    SYSTEM::BootTracker::begin();

    TEST_ASSERT_EQUAL_HEX32(SYSTEM::BootTracker::kMagic, SYSTEM::rtcBootState.magic);
    TEST_ASSERT_EQUAL_UINT32(1, SYSTEM::BootTracker::getBootCount());
    TEST_ASSERT_EQUAL_UINT16(0, SYSTEM::BootTracker::getUnexpectedRestarts());
    TEST_ASSERT_FALSE(SYSTEM::BootTracker::wasLastBootUnexpected());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ESP_RST_POWERON), SYSTEM::rtcBootState.lastResetReason);
    TEST_ASSERT_EQUAL_UINT8(0, SYSTEM::rtcBootState.lastShutdownReason);
}

void test_begin_is_idempotent_after_first_initialization() {
    TEST_STUBS::ESP::resetReason = ESP_RST_POWERON;

    SYSTEM::BootTracker::begin();
    const uint32_t bootCountAfterFirstBegin = SYSTEM::BootTracker::getBootCount();
    const uint16_t unexpectedAfterFirstBegin = SYSTEM::BootTracker::getUnexpectedRestarts();
    const uint8_t resetReasonAfterFirstBegin = SYSTEM::rtcBootState.lastResetReason;

    TEST_STUBS::ESP::resetReason = ESP_RST_TASK_WDT;
    SYSTEM::BootTracker::begin();

    TEST_ASSERT_EQUAL_UINT32(bootCountAfterFirstBegin, SYSTEM::BootTracker::getBootCount());
    TEST_ASSERT_EQUAL_UINT16(unexpectedAfterFirstBegin, SYSTEM::BootTracker::getUnexpectedRestarts());
    TEST_ASSERT_EQUAL_UINT8(resetReasonAfterFirstBegin, SYSTEM::rtcBootState.lastResetReason);
}

void test_begin_counts_unexpected_restart_without_shutdown_marker() {
    SYSTEM::rtcBootState = makeValidRtcState();
    SYSTEM::rtcBootState.lastShutdownReason = 0;
    TEST_STUBS::ESP::resetReason = ESP_RST_TASK_WDT;

    SYSTEM::BootTracker::begin();

    TEST_ASSERT_EQUAL_UINT32(8, SYSTEM::BootTracker::getBootCount());
    TEST_ASSERT_EQUAL_UINT16(3, SYSTEM::BootTracker::getUnexpectedRestarts());
    TEST_ASSERT_TRUE(SYSTEM::BootTracker::wasLastBootUnexpected());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ESP_RST_TASK_WDT), SYSTEM::rtcBootState.lastResetReason);
    TEST_ASSERT_EQUAL_UINT8(0, SYSTEM::rtcBootState.lastShutdownReason);
}

void test_begin_does_not_count_restart_when_shutdown_marker_exists() {
    SYSTEM::rtcBootState = makeValidRtcState();
    TEST_STUBS::ESP::resetReason = ESP_RST_SW;

    SYSTEM::BootTracker::begin();

    TEST_ASSERT_EQUAL_UINT32(8, SYSTEM::BootTracker::getBootCount());
    TEST_ASSERT_EQUAL_UINT16(2, SYSTEM::BootTracker::getUnexpectedRestarts());
    TEST_ASSERT_FALSE(SYSTEM::BootTracker::wasLastBootUnexpected());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ESP_RST_SW), SYSTEM::rtcBootState.lastResetReason);
}

void test_begin_treats_restart_command_marker_as_controlled_software_restart() {
    SYSTEM::rtcBootState = makeValidRtcState();
    SYSTEM::rtcBootState.lastShutdownReason =
        static_cast<uint8_t>(SYSTEM::ShutdownReason::RESTART_COMMAND);
    TEST_STUBS::ESP::resetReason = ESP_RST_SW;

    SYSTEM::BootTracker::begin();

    TEST_ASSERT_EQUAL_UINT32(8, SYSTEM::BootTracker::getBootCount());
    TEST_ASSERT_EQUAL_UINT16(2, SYSTEM::BootTracker::getUnexpectedRestarts());
    TEST_ASSERT_FALSE(SYSTEM::BootTracker::wasLastBootUnexpected());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SYSTEM::ShutdownReason::RESTART_COMMAND),
                            SYSTEM::BootTracker::getLastShutdownReason());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ESP_RST_SW),
                            SYSTEM::rtcBootState.lastResetReason);
}

void test_begin_caps_unexpected_restart_counter_at_uint16_max() {
    SYSTEM::rtcBootState = makeValidRtcState();
    SYSTEM::rtcBootState.lastShutdownReason = 0;
    SYSTEM::rtcBootState.unexpectedRestarts = UINT16_MAX;
    TEST_STUBS::ESP::resetReason = ESP_RST_WDT;

    SYSTEM::BootTracker::begin();

    TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, SYSTEM::BootTracker::getUnexpectedRestarts());
    TEST_ASSERT_TRUE(SYSTEM::BootTracker::wasLastBootUnexpected());
}

void test_record_shutdown_persists_reason_uptime_and_heap() {
    TEST_STUBS::ARDUINO::millisValue = 4321;
    TEST_STUBS::HEAP::freeHeap = 8765;

    SYSTEM::BootTracker::recordShutdown(SYSTEM::ShutdownReason::FACTORY_RESET);

    TEST_ASSERT_EQUAL_UINT32(1, SYSTEM::BootTracker::getBootCount());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SYSTEM::ShutdownReason::FACTORY_RESET),
                            SYSTEM::rtcBootState.lastShutdownReason);
    TEST_ASSERT_EQUAL_UINT32(4321, SYSTEM::rtcBootState.lastShutdownMs);
    TEST_ASSERT_EQUAL_UINT32(4321, SYSTEM::rtcBootState.lastUptimeMs);
    TEST_ASSERT_EQUAL_UINT32(8765, SYSTEM::rtcBootState.freeHeapAtShutdown);
}

void test_public_diagnostics_getters_expose_retained_shutdown_state() {
    SYSTEM::rtcBootState = makeValidRtcState();
    TEST_STUBS::ESP::resetReason = ESP_RST_DEEPSLEEP;

    SYSTEM::BootTracker::begin();

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SYSTEM::ShutdownReason::CLEAN_SLEEP),
                            SYSTEM::BootTracker::getLastShutdownReason());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ESP_RST_DEEPSLEEP),
                            SYSTEM::BootTracker::getLastResetReason());
    TEST_ASSERT_EQUAL_UINT32(777, SYSTEM::BootTracker::getFreeHeapAtShutdown());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_begin_starts_fresh_when_rtc_magic_is_invalid);
    RUN_TEST(test_begin_is_idempotent_after_first_initialization);
    RUN_TEST(test_begin_counts_unexpected_restart_without_shutdown_marker);
    RUN_TEST(test_begin_does_not_count_restart_when_shutdown_marker_exists);
    RUN_TEST(test_begin_treats_restart_command_marker_as_controlled_software_restart);
    RUN_TEST(test_begin_caps_unexpected_restart_counter_at_uint16_max);
    RUN_TEST(test_record_shutdown_persists_reason_uptime_and_heap);
    RUN_TEST(test_public_diagnostics_getters_expose_retained_shutdown_state);
    return UNITY_END();
}
