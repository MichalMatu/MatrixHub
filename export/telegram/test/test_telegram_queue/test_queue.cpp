/**
 * @file test_queue.cpp
 * @brief Unit tests for MessageQueue (LRU Queue Overflow)
 */

#ifdef NATIVE_BUILD
#include <unity.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>

// Native stubs are provided globally in test/stubs

// Implementation
#include "../../src/notifications/telegram/queue/MessageQueue.cpp"

// Mocks for Logging
namespace LOG {
    Settings Logging::_settings;
    void Logging::log(esp_log_level_t level, const char *tag, const char *fmt, ...) {}
    void Logging::logSection(const char* title) {}
    void Logging::logStackHwm(const char* name, uint32_t period) {}
}

using namespace TELEGRAM;

namespace {
MessageQueue* gQueue = nullptr;
}

void setUp(void) {
    gQueue = new MessageQueue();
    TEST_ASSERT_NOT_NULL(gQueue);
    TEST_ASSERT_TRUE(gQueue->isReady());
    gQueue->clear();
}

void tearDown(void) {
    delete gQueue;
    gQueue = nullptr;
}

void test_queue_enqueue_dequeue() {
    TEST_ASSERT_TRUE(gQueue->isEmpty());
    TEST_ASSERT_EQUAL(0, gQueue->count());

    bool res = gQueue->enqueue("123", "Hello");
    TEST_ASSERT_TRUE(res);
    TEST_ASSERT_FALSE(gQueue->isEmpty());
    TEST_ASSERT_EQUAL(1, gQueue->count());

    OutboundMessage msg;
    bool deqRes = gQueue->dequeue(msg);
    TEST_ASSERT_TRUE(deqRes);
    TEST_ASSERT_EQUAL_STRING("123", msg.chatId);
    TEST_ASSERT_EQUAL_STRING("Hello", msg.text);

    TEST_ASSERT_TRUE(gQueue->isEmpty());
}

void test_queue_overflow_lru() {
    // Fill the queue to capacity
    for (size_t i = 0; i < kQueueCapacity; i++) {
        char text[32];
        snprintf(text, sizeof(text), "Message %zu", i);
        bool res = gQueue->enqueue("Chat", text);
        TEST_ASSERT_TRUE(res);
    }

    TEST_ASSERT_EQUAL(kQueueCapacity, gQueue->count());

    // Enqueue one more; this should force the oldest ("Message 0") out
    bool resOverflow = gQueue->enqueue("Chat", "Overflow Message");
    TEST_ASSERT_TRUE(resOverflow); // Queue accepts it by dropping oldest
    TEST_ASSERT_EQUAL(kQueueCapacity, gQueue->count());

    // Now dequeue the first message. It should be "Message 1", not "Message 0"
    OutboundMessage msg;
    bool deqRes = gQueue->dequeue(msg);
    TEST_ASSERT_TRUE(deqRes);

    TEST_ASSERT_EQUAL_STRING("Message 1", msg.text);

    // Drain the rest to verify order and the final overflow message
    for (size_t i = 2; i < kQueueCapacity; i++) {
        char expected[32];
        snprintf(expected, sizeof(expected), "Message %zu", i);
        
        gQueue->dequeue(msg);
        TEST_ASSERT_EQUAL_STRING(expected, msg.text);
    }

    // The very last message should be the overflow one
    gQueue->dequeue(msg);
    TEST_ASSERT_EQUAL_STRING("Overflow Message", msg.text);

    // Queue should now be empty
    TEST_ASSERT_TRUE(gQueue->isEmpty());
}

void test_queue_clear() {
    gQueue->enqueue("1", "A");
    gQueue->enqueue("2", "B");
    TEST_ASSERT_EQUAL(2, gQueue->count());

    gQueue->clear();
    TEST_ASSERT_EQUAL(0, gQueue->count());
    TEST_ASSERT_TRUE(gQueue->isEmpty());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_queue_enqueue_dequeue);
    RUN_TEST(test_queue_overflow_lru);
    RUN_TEST(test_queue_clear);
    return UNITY_END();
}

#endif // NATIVE_BUILD
