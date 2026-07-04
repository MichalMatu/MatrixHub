#include <unity.h>

#include "../../src/system/restart/RuntimeRestart.cpp"

namespace TEST_RUNTIME_RESTART {
inline SYSTEM::ShutdownReason lastReason = SYSTEM::ShutdownReason::UNKNOWN;
inline uint32_t recordCalls = 0;
}

namespace SYSTEM {
void BootTracker::recordShutdown(ShutdownReason reason) {
    TEST_RUNTIME_RESTART::lastReason = reason;
    TEST_RUNTIME_RESTART::recordCalls++;
}
}  // namespace SYSTEM

namespace {

void resetRestartStubs() {
    TEST_STUBS::ESP::reset();
    TEST_STUBS::FREERTOS::resetTaskCreateStub();
    TEST_RUNTIME_RESTART::lastReason = SYSTEM::ShutdownReason::UNKNOWN;
    TEST_RUNTIME_RESTART::recordCalls = 0;
}

}  // namespace

void setUp(void) {
    resetRestartStubs();
}

void tearDown(void) {}

void test_emergency_restart_records_reason_and_restarts_immediately_without_delay() {
    SYSTEM::RESTART::emergencyRestart(
        SYSTEM::ShutdownReason::RESTART_COMMAND,
        "MainLoop",
        "manual restart",
        0);

    TEST_ASSERT_EQUAL_UINT32(1, TEST_RUNTIME_RESTART::recordCalls);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SYSTEM::ShutdownReason::RESTART_COMMAND),
                          static_cast<int>(TEST_RUNTIME_RESTART::lastReason));
    TEST_ASSERT_EQUAL_UINT32(1, TEST_STUBS::ESP::restartCalls);
    TEST_ASSERT_EQUAL_UINT32(0, TEST_STUBS::FREERTOS::tickCount);
}

void test_emergency_restart_waits_before_restart_when_delay_is_requested() {
    SYSTEM::RESTART::emergencyRestart(
        SYSTEM::ShutdownReason::WATCHDOG_RESTART,
        nullptr,
        nullptr,
        25);

    TEST_ASSERT_EQUAL_UINT32(1, TEST_RUNTIME_RESTART::recordCalls);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SYSTEM::ShutdownReason::WATCHDOG_RESTART),
                          static_cast<int>(TEST_RUNTIME_RESTART::lastReason));
    TEST_ASSERT_EQUAL_UINT32(1, TEST_STUBS::ESP::restartCalls);
    TEST_ASSERT_EQUAL_UINT32(25, TEST_STUBS::FREERTOS::tickCount);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_emergency_restart_records_reason_and_restarts_immediately_without_delay);
    RUN_TEST(test_emergency_restart_waits_before_restart_when_delay_is_requested);
    return UNITY_END();
}
