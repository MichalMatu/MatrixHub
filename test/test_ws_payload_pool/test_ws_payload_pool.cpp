#include <unity.h>

#include "../../src/api/common/websocket/WsPayloadPool.cpp"

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
const char* Logging::levelToString(esp_log_level_t level) {
    (void)level;
    return "info";
}
esp_log_level_t Logging::stringToLevel(std::string_view name, esp_log_level_t fallback) {
    (void)name;
    return fallback;
}
} // namespace LOG

namespace SYSTEM::HEALTH {
void HttpServerHealthTracker::recordWsQueueDrop(size_t payloadLen) {
    (void)payloadLen;
}
} // namespace SYSTEM::HEALTH

using API::WEBSOCKET::WsMessage;
using API::WEBSOCKET::WsPayloadPool;

void setUp(void) {}
void tearDown(void) {}

void test_pool_acquire_release_and_reuse_slot() {
    WsPayloadPool pool("test");
    TEST_ASSERT_TRUE(pool.init(2, 32));

    uint8_t* slotA = nullptr;
    uint8_t* slotB = nullptr;
    uint8_t* slotC = nullptr;
    int16_t idxA = -1;
    int16_t idxB = -1;
    int16_t idxC = -1;

    TEST_ASSERT_TRUE(pool.acquireSlot(16, &slotA, &idxA));
    TEST_ASSERT_TRUE(pool.acquireSlot(16, &slotB, &idxB));
    TEST_ASSERT_NOT_EQUAL(idxA, idxB);
    TEST_ASSERT_FALSE(pool.acquireSlot(16, &slotC, &idxC));

    pool.releaseSlot(idxA);
    TEST_ASSERT_TRUE(pool.acquireSlot(16, &slotC, &idxC));
    TEST_ASSERT_EQUAL(idxA, idxC);
}

void test_pool_rejects_oversized_payload() {
    WsPayloadPool pool("test");
    TEST_ASSERT_TRUE(pool.init(1, 8));

    uint8_t* slot = nullptr;
    int16_t idx = -1;
    TEST_ASSERT_FALSE(pool.acquireSlot(9, &slot, &idx));
}

void test_pool_acquire_uses_brief_lock_wait() {
    WsPayloadPool pool("test");
    TEST_ASSERT_TRUE(pool.init(1, 16));

    uint8_t* slot = nullptr;
    int16_t idx = -1;
    TEST_STUBS::FREERTOS::resetSemaphoreTakeStats();
    TEST_ASSERT_TRUE(pool.acquireSlot(8, &slot, &idx));
    TEST_ASSERT_EQUAL(pdMS_TO_TICKS(10), TEST_STUBS::FREERTOS::lastSemaphoreTakeTimeout);
    TEST_ASSERT_EQUAL_UINT32(1, TEST_STUBS::FREERTOS::semaphoreTakeCount);
}

void test_pool_exhaustion_retries_briefly_before_drop() {
    WsPayloadPool pool("test");
    TEST_ASSERT_TRUE(pool.init(1, 16));

    uint8_t* slot = nullptr;
    int16_t idx = -1;
    TEST_ASSERT_TRUE(pool.acquireSlot(8, &slot, &idx));

    uint8_t* secondSlot = nullptr;
    int16_t secondIdx = -1;
    TEST_STUBS::FREERTOS::tickCount = 0;
    TEST_ASSERT_FALSE(pool.acquireSlot(8, &secondSlot, &secondIdx));
    TEST_ASSERT_EQUAL(pdMS_TO_TICKS(5), TEST_STUBS::FREERTOS::tickCount);
}

void test_release_message_resources_returns_slot_to_pool() {
    WsPayloadPool pool("test");
    TEST_ASSERT_TRUE(pool.init(1, 16));

    uint8_t* slot = nullptr;
    int16_t idx = -1;
    TEST_ASSERT_TRUE(pool.acquireSlot(8, &slot, &idx));

    WsMessage msg = {slot, 8, HTTPD_WS_TYPE_BINARY, false, idx, {0}, 0};
    pool.releaseMessageResources(msg);

    uint8_t* reacquired = nullptr;
    int16_t reacquiredIdx = -1;
    TEST_ASSERT_TRUE(pool.acquireSlot(8, &reacquired, &reacquiredIdx));
    TEST_ASSERT_EQUAL(idx, reacquiredIdx);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_pool_acquire_release_and_reuse_slot);
    RUN_TEST(test_pool_rejects_oversized_payload);
    RUN_TEST(test_pool_acquire_uses_brief_lock_wait);
    RUN_TEST(test_pool_exhaustion_retries_briefly_before_drop);
    RUN_TEST(test_release_message_resources_returns_slot_to_pool);
    return UNITY_END();
}
