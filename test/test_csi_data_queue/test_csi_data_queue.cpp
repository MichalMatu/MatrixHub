#include <unity.h>
#include <string_view>

#include "../../src/wifisensing/csi/data/CsiDataQueue.cpp"

namespace LOG {
Settings Logging::_settings;
void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    (void)level;
    (void)tag;
    (void)fmt;
}
void Logging::logSection(const char* title) { (void)title; }
void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {
    (void)taskName;
    (void)stackSize;
}
void Logging::logStackHwm(const char* taskName, uint32_t stackSize, uint32_t minFreeBytes) {
    (void)taskName;
    (void)stackSize;
    (void)minFreeBytes;
}
const char* Logging::levelToString(esp_log_level_t level) {
    (void)level;
    return "info";
}
esp_log_level_t Logging::stringToLevel(std::string_view name, esp_log_level_t fallback) {
    (void)name;
    return fallback;
}
} // namespace LOG

using WIFISENSING::CSI::CsiDataQueue;
using WIFISENSING::CSI::CsiPacket;

void setUp(void) {
    TEST_STUBS::FREERTOS::resetQueueStub();
}

void tearDown(void) {}

void test_queue_reports_capacity_depth_and_total_drops() {
    CsiDataQueue queue(2);
    TEST_ASSERT_TRUE(queue.begin());
    TEST_ASSERT_EQUAL_UINT32(2, queue.getCapacity());
    TEST_ASSERT_EQUAL_UINT32(0, queue.getDepth());

    CsiPacket packet{};
    packet.len = 4;
    packet.buf[0] = 42;

    TEST_ASSERT_TRUE(queue.pushFromIsr(packet));
    TEST_ASSERT_TRUE(queue.pushFromIsr(packet));
    TEST_ASSERT_EQUAL_UINT32(2, queue.getDepth());

    TEST_ASSERT_FALSE(queue.pushFromIsr(packet));
    TEST_ASSERT_EQUAL_UINT32(1, queue.getDroppedPacketsTotal());
    TEST_ASSERT_EQUAL_UINT32(1, queue.takeDroppedPackets());
    TEST_ASSERT_EQUAL_UINT32(0, queue.takeDroppedPackets());

    CsiPacket popped{};
    TEST_ASSERT_TRUE(queue.pop(popped, 0));
    TEST_ASSERT_EQUAL_UINT32(1, queue.getDepth());
    TEST_ASSERT_EQUAL_UINT8(42, popped.buf[0]);
}

void test_begin_failure_releases_partial_allocations() {
    TEST_STUBS::FREERTOS::failCreateStaticQueue = true;
    CsiDataQueue queue(2);

    TEST_ASSERT_FALSE(queue.begin());
    TEST_ASSERT_EQUAL_UINT32(0, queue.getDepth());
    TEST_ASSERT_EQUAL_UINT32(0, queue.getDroppedPacketsTotal());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_queue_reports_capacity_depth_and_total_drops);
    RUN_TEST(test_begin_failure_releases_partial_allocations);
    return UNITY_END();
}
