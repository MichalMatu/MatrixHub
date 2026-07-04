/**
 * @file test_updates_parser.cpp
 * @brief Unit tests for TelegramUpdatesLiteParser (Memory & Zero-Copy)
 */

#ifdef NATIVE_BUILD
#include <unity.h>
#include <cstring>
#include <cstdlib>
#include <string>

// Stubs
#include "../stubs/NetworkClient.h"

// System and parser implementations
#include "../../src/system/utils/json/JsonStreamReader.cpp"
#include "../../src/notifications/telegram/polling/TelegramUpdatesLite.h"
#include "../../src/config/App.h"
#include "../../src/notifications/telegram/polling/TelegramJsonParsers.cpp"
#include "../../src/notifications/telegram/polling/TelegramUpdatesLiteParser.cpp"

using namespace TELEGRAM;

// Mocks for Logging if not globally provided in NATIVE_BUILD
namespace LOG {
    Settings Logging::_settings;
    void Logging::log(esp_log_level_t level, const char *tag, const char *fmt, ...) {}
    void Logging::logSection(const char* title) {}
    void Logging::logStackHwm(const char* name, uint32_t period) {}
}

namespace SYSTEM {
    namespace CONFIG {
        void load() {}
        bool save() { return true; }
        // ... if any other system configs are needed
    }
}

// Memory tracking wrapper
static size_t _allocatedMemory = 0;

void* operator new(size_t size) {
    _allocatedMemory += size;
    return malloc(size);
}

void operator delete(void* ptr) noexcept {
    // We don't track size on delete for this simple test, 
    // but in Unity we mostly care that _allocatedMemory shouldn't grow during parse
    free(ptr);
}

void setUp(void) {
    _allocatedMemory = 0;
}
void tearDown(void) {}

// Test callback
static bool updatesCallback(const UpdateLite& update, void* user) {
    int* count = static_cast<int*>(user);
    (*count)++;
    
    // Verify zero-copy contents
    TEST_ASSERT_EQUAL_INT64(12345, update.updateId);
    TEST_ASSERT_TRUE(update.hasMessage);
    TEST_ASSERT_EQUAL_STRING("Hello World", update.text);
    TEST_ASSERT_EQUAL_STRING("987654321", update.chatId);
    TEST_ASSERT_EQUAL_STRING("john_doe", update.fromUsername);
    return true; // continue parsing
}

void test_memory_and_zerocopy() {
    NetworkClient client;
    client.setMockData(R"({
        "ok": true,
        "result": [
            {
                "update_id": 12345,
                "message": {
                    "message_id": 1,
                    "from": {"id": 111, "username": "john_doe"},
                    "chat": {"id": 987654321, "type": "private"},
                    "date": 1700000000,
                    "text": "Hello World"
                }
            }
        ]
    })");

    int updatesCount = 0;
    bool telegramOk = false;

    // Record memory before the parsing loop
    size_t memBefore = _allocatedMemory;

    bool parseResult = TelegramUpdatesLiteParser::parse(
        client,
        updatesCallback,
        &updatesCount,
        telegramOk
    );

    size_t memAfter = _allocatedMemory;

    TEST_ASSERT_TRUE(parseResult);
    TEST_ASSERT_TRUE(telegramOk);
    TEST_ASSERT_EQUAL_INT(1, updatesCount);

    // Verify ZERO-COPY: allocations shouldn't happen inside parse loop
    // std::string uses some heap perhaps, but JsonStreamReader shouldn't allocate
    TEST_ASSERT_EQUAL(memBefore, memAfter);
}

void test_parse_large_buffer_no_leak() {
    NetworkClient client;
    
    // Make a large payload with multiple updates
    std::string payload = "{\"ok\":true,\"result\":[";
    for (int i=0; i<50; i++) {
        payload += "{\"update_id\":" + std::to_string(5000+i) + ",\"message\":{\"text\":\"Test\",\"chat\":{\"id\":123}}}";
        if (i < 49) payload += ",";
    }
    payload += "]}";
    
    client.setMockData(payload);

    int count = 0;
    bool telegramOk = false;
    
    auto cb = [](const UpdateLite& update, void* user) -> bool {
        int* c = static_cast<int*>(user);
        (*c)++;
        return true;
    };

    size_t memBefore = _allocatedMemory;
    bool parseResult = TelegramUpdatesLiteParser::parse(client, cb, &count, telegramOk);
    size_t memAfter = _allocatedMemory;

    TEST_ASSERT_TRUE(parseResult);
    TEST_ASSERT_TRUE(telegramOk);
    TEST_ASSERT_EQUAL_INT(50, count);
    
    // Memory should not grow!
    TEST_ASSERT_EQUAL(memBefore, memAfter);
}

void test_parse_negative_corrupted_json() {
    NetworkClient client;
    client.setMockData(R"({"ok": true, "result": [{"update_id": 123,)"); // Missing closing brackets and value

    int count = 0;
    bool telegramOk = false;
    
    auto cb = [](const UpdateLite& update, void* user) -> bool {
        return true;
    };

    bool parseResult = TelegramUpdatesLiteParser::parse(client, cb, &count, telegramOk);
    
    // Should fail elegantly without crashing or infinite loop
    TEST_ASSERT_FALSE(parseResult);
}

void test_parse_negative_wrong_result_type() {
    NetworkClient client;
    client.setMockData(R"({"ok": true, "result": {"not_an_array": true}})"); // Object instead of array

    int count = 0;
    bool telegramOk = false;
    
    auto cb = [](const UpdateLite& update, void* user) -> bool {
        return true;
    };

    bool parseResult = TelegramUpdatesLiteParser::parse(client, cb, &count, telegramOk);
    
    // Should fail gracefully because it expects '[' for result
    TEST_ASSERT_FALSE(parseResult);
}

void test_parse_negative_missing_ok() {
    NetworkClient client;
    client.setMockData(R"({"result": []})"); // Missing "ok" field

    int count = 0;
    bool telegramOk = false;
    
    auto cb = [](const UpdateLite& update, void* user) -> bool {
        return true;
    };

    bool parseResult = TelegramUpdatesLiteParser::parse(client, cb, &count, telegramOk);
    
    // Should fail because ok is required by the parser logic
    TEST_ASSERT_FALSE(parseResult);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_memory_and_zerocopy);
    RUN_TEST(test_parse_large_buffer_no_leak);
    RUN_TEST(test_parse_negative_corrupted_json);
    RUN_TEST(test_parse_negative_wrong_result_type);
    RUN_TEST(test_parse_negative_missing_ok);
    return UNITY_END();
}

#endif // NATIVE_BUILD
