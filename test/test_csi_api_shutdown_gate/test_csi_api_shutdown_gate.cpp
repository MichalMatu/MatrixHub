#include <unity.h>

#include <atomic>
#include <thread>

#include "../../src/api/wifisensing/CsiApiShutdownGate.h"

void setUp(void) {}
void tearDown(void) {}

void test_close_rejects_late_handler_while_preexisting_handler_drains() {
    API::CsiApiShutdownGate gate;
    std::atomic<bool> entered{false};
    std::atomic<bool> acquired{false};
    std::atomic<bool> releaseHandler{false};

    std::thread handler([&]() {
        API::CsiApiShutdownGate::Lease lease(gate);
        acquired.store(static_cast<bool>(lease), std::memory_order_release);
        entered.store(true, std::memory_order_release);
        while (!releaseHandler.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    });

    while (!entered.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    gate.close();
    API::CsiApiShutdownGate::Lease lateLease(gate);

    TEST_ASSERT_TRUE(acquired.load(std::memory_order_acquire));
    TEST_ASSERT_TRUE(gate.isClosed());
    TEST_ASSERT_FALSE(static_cast<bool>(lateLease));
    TEST_ASSERT_TRUE(gate.hasInFlight());

    releaseHandler.store(true, std::memory_order_release);
    handler.join();

    TEST_ASSERT_FALSE(gate.hasInFlight());
}

void test_close_without_in_flight_handler_is_immediately_drained() {
    API::CsiApiShutdownGate gate;

    gate.close();

    TEST_ASSERT_TRUE(gate.isClosed());
    TEST_ASSERT_FALSE(gate.hasInFlight());
    API::CsiApiShutdownGate::Lease lease(gate);
    TEST_ASSERT_FALSE(static_cast<bool>(lease));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_close_rejects_late_handler_while_preexisting_handler_drains);
    RUN_TEST(test_close_without_in_flight_handler_is_immediately_drained);
    return UNITY_END();
}
