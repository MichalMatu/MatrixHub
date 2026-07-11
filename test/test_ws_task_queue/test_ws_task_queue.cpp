#include <unity.h>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>

#include "../../src/api/common/websocket/WsPayloadPool.cpp"
#include "../../src/api/common/websocket/WsTaskQueue.cpp"

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
using API::WEBSOCKET::WsTaskQueue;

namespace {
int g_processedCount = 0;
size_t g_lastProcessedLen = 0;
uint8_t g_lastProcessedBytes[API::WEBSOCKET::INLINE_PAYLOAD_CAPACITY] = {};

void resetStubs() {
    TEST_STUBS::FREERTOS::resetQueueStub();
    TEST_STUBS::FREERTOS::resetTaskCreateStub();
    TEST_STUBS::FREERTOS::resetSemaphoreStubs();
    g_processedCount = 0;
    g_lastProcessedLen = 0;
    memset(g_lastProcessedBytes, 0, sizeof(g_lastProcessedBytes));
}
} // namespace

void setUp(void) {
    resetStubs();
}
void tearDown(void) {}

void test_enable_uses_configured_ws_priority_and_core() {
    WsPayloadPool pool("test");
    WsTaskQueue queue("test", [](WsMessage& msg) {
        g_processedCount++;
        g_lastProcessedLen = msg.len;
    }, &pool);

    queue.enable(4, 4096);

    TEST_ASSERT_TRUE(queue.isEnabled());
    TEST_ASSERT_EQUAL_STRING("ws_tx", TEST_STUBS::FREERTOS::lastTaskName);
    TEST_ASSERT_EQUAL(CONFIG::TASKS::PRIO_WS_BROADCAST, TEST_STUBS::FREERTOS::lastTaskPriority);
    TEST_ASSERT_EQUAL(CONFIG::TASKS::CORE_WS_BROADCAST, TEST_STUBS::FREERTOS::lastTaskCore);

    TEST_STUBS::FREERTOS::taskState = eSuspended;
    TEST_ASSERT_TRUE(queue.disable());
}

void test_enqueue_queue_full_releases_pool_slot() {
    WsPayloadPool pool("test");
    TEST_ASSERT_TRUE(pool.init(1, 16));

    WsTaskQueue queue("test", [](WsMessage&) {}, &pool);
    queue.enable(1, 4096);

    uint8_t* slot = nullptr;
    int16_t slotIndex = -1;
    TEST_ASSERT_TRUE(pool.acquireSlot(8, &slot, &slotIndex));

    WsMessage msg = {slot, 8, HTTPD_WS_TYPE_BINARY, false, slotIndex, {0}, 0};
    TEST_STUBS::FREERTOS::failNextQueueSend = true;
    TEST_ASSERT_FALSE(queue.enqueue(msg));

    uint8_t* reacquired = nullptr;
    int16_t reacquiredIndex = -1;
    TEST_ASSERT_TRUE(pool.acquireSlot(8, &reacquired, &reacquiredIndex));
    TEST_ASSERT_EQUAL(slotIndex, reacquiredIndex);

    TEST_STUBS::FREERTOS::taskState = eSuspended;
    TEST_ASSERT_TRUE(queue.disable());
}

void test_enqueue_uses_contention_tolerant_lifecycle_lock_wait() {
    WsPayloadPool pool("test");
    TEST_ASSERT_TRUE(pool.init(1, 16));

    WsTaskQueue queue("test", [](WsMessage&) {}, &pool);
    queue.enable(2, 4096);

    uint8_t* slot = nullptr;
    int16_t slotIndex = -1;
    TEST_ASSERT_TRUE(pool.acquireSlot(8, &slot, &slotIndex));

    TEST_STUBS::FREERTOS::resetSemaphoreTakeStats();
    WsMessage msg = {slot, 8, HTTPD_WS_TYPE_BINARY, false, slotIndex, {0}, 0};
    TEST_ASSERT_TRUE(queue.enqueue(msg));
    TEST_ASSERT_EQUAL(pdMS_TO_TICKS(250), TEST_STUBS::FREERTOS::lastSemaphoreTakeTimeout);
    TEST_ASSERT_EQUAL_UINT32(1, TEST_STUBS::FREERTOS::semaphoreTakeCount);

    TEST_STUBS::FREERTOS::taskState = eSuspended;
    TEST_ASSERT_TRUE(queue.disable());
}

void test_enqueue_uses_brief_queue_send_wait() {
    WsPayloadPool pool("test");
    TEST_ASSERT_TRUE(pool.init(1, 16));

    WsTaskQueue queue("test", [](WsMessage&) {}, &pool);
    queue.enable(2, 4096);

    uint8_t* slot = nullptr;
    int16_t slotIndex = -1;
    TEST_ASSERT_TRUE(pool.acquireSlot(8, &slot, &slotIndex));

    WsMessage msg = {slot, 8, HTTPD_WS_TYPE_BINARY, false, slotIndex, {0}, 0};
    TEST_ASSERT_TRUE(queue.enqueue(msg));
    TEST_ASSERT_EQUAL(pdMS_TO_TICKS(10), TEST_STUBS::FREERTOS::lastQueueSendTimeout);

    TEST_STUBS::FREERTOS::taskState = eSuspended;
    TEST_ASSERT_TRUE(queue.disable());
}

void test_request_stop_is_nonblocking_and_rejects_new_messages() {
    WsPayloadPool pool("test");
    TEST_ASSERT_TRUE(pool.init(1, 16));

    WsTaskQueue queue("test", [](WsMessage&) {}, &pool);
    queue.enable(2, 4096);

    TEST_STUBS::FREERTOS::resetSemaphoreTakeStats();
    queue.requestStop();

    TEST_ASSERT_FALSE(queue.isEnabled());
    TEST_ASSERT_EQUAL_UINT32(0, TEST_STUBS::FREERTOS::semaphoreTakeCount);

    uint8_t* slot = nullptr;
    int16_t slotIndex = -1;
    TEST_ASSERT_TRUE(pool.acquireSlot(8, &slot, &slotIndex));
    WsMessage msg = {slot, 8, HTTPD_WS_TYPE_BINARY, false, slotIndex, {0}, 0};
    TEST_ASSERT_FALSE(queue.enqueue(msg));

    uint8_t* reacquired = nullptr;
    int16_t reacquiredIndex = -1;
    TEST_ASSERT_TRUE(pool.acquireSlot(8, &reacquired, &reacquiredIndex));
    TEST_ASSERT_EQUAL(slotIndex, reacquiredIndex);

    TEST_STUBS::FREERTOS::taskState = eSuspended;
    TEST_ASSERT_TRUE(queue.disable());
}

void test_request_stop_waits_for_an_enqueue_that_already_crossed_the_fence() {
    WsPayloadPool pool("test");
    TEST_ASSERT_TRUE(pool.init(1, 16));

    WsTaskQueue queue("test", [](WsMessage& msg) {
        g_processedCount++;
        g_lastProcessedLen = msg.len;
    }, &pool);
    queue.enable(2, 4096);

    uint8_t* slot = nullptr;
    int16_t slotIndex = -1;
    TEST_ASSERT_TRUE(pool.acquireSlot(8, &slot, &slotIndex));
    WsMessage msg = {slot, 8, HTTPD_WS_TYPE_BINARY, false, slotIndex, {0}, 0};

    std::mutex gateMutex;
    std::condition_variable gateCv;
    bool sendEntered = false;
    bool releaseSend = false;
    TEST_STUBS::FREERTOS::beforeQueueSendHook = [&]() {
        std::unique_lock<std::mutex> lock(gateMutex);
        sendEntered = true;
        gateCv.notify_all();
        gateCv.wait(lock, [&]() { return releaseSend; });
    };

    bool enqueueResult = false;
    std::thread enqueueThread([&]() {
        enqueueResult = queue.enqueue(msg);
    });
    {
        std::unique_lock<std::mutex> lock(gateMutex);
        gateCv.wait(lock, [&]() { return sendEntered; });
    }

    queue.requestStop();
    std::thread workerThread([&queue]() {
        TEST_STUBS::FREERTOS::lastTaskFunction(&queue);
    });

    {
        std::lock_guard<std::mutex> lock(gateMutex);
        releaseSend = true;
    }
    gateCv.notify_all();
    enqueueThread.join();
    TEST_STUBS::FREERTOS::beforeQueueSendHook = nullptr;
    workerThread.join();

    TEST_ASSERT_TRUE(enqueueResult);
    TEST_ASSERT_EQUAL(1, g_processedCount);
    TEST_ASSERT_EQUAL(8, g_lastProcessedLen);
    TEST_ASSERT_TRUE(queue.disable());

    uint8_t* reacquired = nullptr;
    int16_t reacquiredIndex = -1;
    TEST_ASSERT_TRUE(pool.acquireSlot(8, &reacquired, &reacquiredIndex));
    TEST_ASSERT_EQUAL(slotIndex, reacquiredIndex);
}

void test_disable_cannot_reap_queue_while_full_producer_lease_is_active() {
    WsPayloadPool pool("test");
    TEST_ASSERT_TRUE(pool.init(1, 16));

    WsTaskQueue queue("test", [](WsMessage&) {}, &pool);
    queue.enable(2, 4096);

    auto producerLease = queue.acquireProducerLease();
    TEST_ASSERT_TRUE(static_cast<bool>(producerLease));

    // The lease models a producer paused after pool acquisition but before its
    // writer/enqueue step. Teardown must remain retryable and keep the queue
    // alive for that producer.
    TEST_ASSERT_FALSE(queue.disable());
    TEST_ASSERT_TRUE(queue.isEnabled());
    TEST_ASSERT_NOT_NULL(TEST_STUBS::FREERTOS::lastCreatedQueue);

    producerLease = {};
    TEST_STUBS::FREERTOS::taskState = eSuspended;
    TEST_ASSERT_TRUE(queue.disable());
    TEST_ASSERT_NULL(TEST_STUBS::FREERTOS::lastCreatedQueue);
}

void test_disable_drains_pending_messages_and_releases_slot() {
    WsPayloadPool pool("test");
    TEST_ASSERT_TRUE(pool.init(1, 16));

    WsTaskQueue queue("test", [](WsMessage&) {}, &pool);
    queue.enable(2, 4096);

    uint8_t* slot = nullptr;
    int16_t slotIndex = -1;
    TEST_ASSERT_TRUE(pool.acquireSlot(8, &slot, &slotIndex));
    WsMessage msg = {slot, 8, HTTPD_WS_TYPE_BINARY, false, slotIndex, {0}, 0};
    TEST_ASSERT_TRUE(queue.enqueue(msg));

    std::thread workerThread([&queue]() {
        TEST_STUBS::FREERTOS::lastTaskFunction(&queue);
    });
    TEST_ASSERT_TRUE(queue.disable());
    workerThread.join();

    uint8_t* reacquired = nullptr;
    int16_t reacquiredIndex = -1;
    TEST_ASSERT_TRUE(pool.acquireSlot(8, &reacquired, &reacquiredIndex));
    TEST_ASSERT_EQUAL(slotIndex, reacquiredIndex);
    TEST_ASSERT_FALSE(queue.isEnabled());
}

void test_broadcast_task_processes_message_and_releases_resources() {
    WsPayloadPool pool("test");
    TEST_ASSERT_TRUE(pool.init(1, 16));

    WsTaskQueue queue("test", [](WsMessage& msg) {
        g_processedCount++;
        g_lastProcessedLen = msg.len;
    }, &pool);
    queue.enable(2, 4096);

    uint8_t* slot = nullptr;
    int16_t slotIndex = -1;
    TEST_ASSERT_TRUE(pool.acquireSlot(12, &slot, &slotIndex));
    memset(slot, 0xAB, 12);

    WsMessage dataMsg = {slot, 12, HTTPD_WS_TYPE_BINARY, false, slotIndex, {0}, 0};
    TEST_ASSERT_TRUE(queue.enqueue(dataMsg));

    TEST_ASSERT_NOT_NULL(TEST_STUBS::FREERTOS::lastTaskFunction);
    std::thread workerThread([&queue]() {
        TEST_STUBS::FREERTOS::lastTaskFunction(&queue);
    });
    TEST_ASSERT_TRUE(queue.disable());
    workerThread.join();

    TEST_ASSERT_EQUAL(1, g_processedCount);
    TEST_ASSERT_EQUAL(12, g_lastProcessedLen);

    uint8_t* reacquired = nullptr;
    int16_t reacquiredIndex = -1;
    TEST_ASSERT_TRUE(pool.acquireSlot(12, &reacquired, &reacquiredIndex));
    TEST_ASSERT_EQUAL(slotIndex, reacquiredIndex);
}

void test_broadcast_task_rebinds_inline_payload_after_queue_copy() {
    WsTaskQueue queue("test", [](WsMessage& msg) {
        g_processedCount++;
        g_lastProcessedLen = msg.len;
        TEST_ASSERT_TRUE(msg.isInline);
        TEST_ASSERT_EQUAL_PTR(msg.inlineData, msg.data);
        memcpy(g_lastProcessedBytes, msg.data, msg.len);
    }, nullptr);
    queue.enable(2, 4096);

    WsMessage msg = {};
    msg.len = 10;
    msg.type = HTTPD_WS_TYPE_BINARY;
    msg.payloadSlot = -1;
    msg.isInline = true;
    for (size_t i = 0; i < msg.len; i++) {
        msg.inlineData[i] = static_cast<uint8_t>(0xA0 + i);
    }
    TEST_ASSERT_TRUE(queue.enqueue(msg));

    TEST_ASSERT_NOT_NULL(TEST_STUBS::FREERTOS::lastTaskFunction);
    std::thread workerThread([&queue]() {
        TEST_STUBS::FREERTOS::lastTaskFunction(&queue);
    });
    TEST_ASSERT_TRUE(queue.disable());
    workerThread.join();

    TEST_ASSERT_EQUAL(1, g_processedCount);
    TEST_ASSERT_EQUAL(10, g_lastProcessedLen);
    for (size_t i = 0; i < g_lastProcessedLen; i++) {
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(0xA0 + i), g_lastProcessedBytes[i]);
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_enable_uses_configured_ws_priority_and_core);
    RUN_TEST(test_enqueue_queue_full_releases_pool_slot);
    RUN_TEST(test_enqueue_uses_contention_tolerant_lifecycle_lock_wait);
    RUN_TEST(test_enqueue_uses_brief_queue_send_wait);
    RUN_TEST(test_request_stop_is_nonblocking_and_rejects_new_messages);
    RUN_TEST(test_request_stop_waits_for_an_enqueue_that_already_crossed_the_fence);
    RUN_TEST(test_disable_cannot_reap_queue_while_full_producer_lease_is_active);
    RUN_TEST(test_disable_drains_pending_messages_and_releases_slot);
    RUN_TEST(test_broadcast_task_processes_message_and_releases_resources);
    RUN_TEST(test_broadcast_task_rebinds_inline_payload_after_queue_copy);
    return UNITY_END();
}
