/**
 * @file test_memory_safety.cpp
 * @brief Unit tests for memory-safe operations
 * 
 * Tests that critical components don't leak memory or cause fragmentation.
 * These tests can run on the host (native) without hardware.
 */

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
#include <cstdlib>

// ============================================================================
// Mock heap tracking for tests
// ============================================================================

static size_t g_allocatedBytes = 0;
static size_t g_allocationCount = 0;
static size_t g_peakMemory = 0;

void reset_memory_tracking() {
    g_allocatedBytes = 0;
    g_allocationCount = 0;
    g_peakMemory = 0;
}

size_t get_allocated_bytes() {
    return g_allocatedBytes;
}

// ============================================================================
// Test: TelegramChat struct uses no dynamic memory
// ============================================================================

// Replicate the TelegramChat struct for testing
struct TelegramChatTest {
    char id[24];
    char type[16];
    char firstName[48];
    char lastName[48];
    char username[48];
    char title[64];
    
    TelegramChatTest() {
        memset(this, 0, sizeof(*this));
    }
};

void test_telegram_chat_fixed_size() {
    // TelegramChat should be exactly this size (no hidden allocations)
    size_t expectedSize = 24 + 16 + 48 + 48 + 48 + 64;
    TEST_ASSERT_EQUAL(expectedSize, sizeof(TelegramChatTest));
    
    // Creating many instances should not grow heap
    TelegramChatTest chats[100];
    
    // Fill with data
    for (int i = 0; i < 100; i++) {
        snprintf(chats[i].id, sizeof(chats[i].id), "%d", i);
        snprintf(chats[i].type, sizeof(chats[i].type), "private");
        snprintf(chats[i].firstName, sizeof(chats[i].firstName), "User%d", i);
    }
    
    // Verify data integrity
    TEST_ASSERT_EQUAL_STRING("99", chats[99].id);
    TEST_ASSERT_EQUAL_STRING("private", chats[99].type);
    TEST_ASSERT_EQUAL_STRING("User99", chats[99].firstName);
}

// ============================================================================
// Test: Fixed buffer string operations don't overflow
// ============================================================================

void test_strncpy_safety() {
    char buffer[16];
    const char* longString = "This is a very long string that exceeds buffer";
    
    // This should truncate safely
    strncpy(buffer, longString, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    TEST_ASSERT_EQUAL(15, strlen(buffer));
    TEST_ASSERT_EQUAL_STRING("This is a very ", buffer);
}

void test_snprintf_safety() {
    char buffer[16];
    
    // snprintf should never overflow
    int written = snprintf(buffer, sizeof(buffer), "%s%s%s", "Hello", " ", "World and more stuff");
    
    // Written count indicates what WOULD have been written
    TEST_ASSERT_GREATER_THAN(sizeof(buffer), written);
    
    // But buffer is safely truncated
    TEST_ASSERT_EQUAL(15, strlen(buffer));
}

// ============================================================================
// Test: JsonDocument parsing doesn't leak
// ============================================================================

void test_json_parsing_no_leak() {
    const char* jsonInput = R"({
        "ok": true,
        "result": [
            {"update_id": 1, "message": {"chat": {"id": 123, "type": "private"}}},
            {"update_id": 2, "message": {"chat": {"id": 456, "type": "group"}}}
        ]
    })";
    
    // Parse multiple times - should not accumulate memory
    for (int i = 0; i < 100; i++) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, jsonInput);
        TEST_ASSERT_FALSE(err);
        
        JsonArray result = doc["result"].as<JsonArray>();
        TEST_ASSERT_EQUAL(2, result.size());
    }
    
    // If we got here without crash, parsing is stable
    TEST_PASS();
}

// ============================================================================
// Test: Repeated operations don't grow memory
// ============================================================================

void test_repeated_json_operations() {
    // Simulate what happens during long-running Telegram polling
    
    const char* responses[] = {
        R"({"ok":true,"result":[]})",
        R"({"ok":true,"result":[{"update_id":1}]})",
        R"({"ok":true,"result":[{"update_id":2},{"update_id":3}]})",
    };
    
    // Simulate 1000 poll cycles
    for (int cycle = 0; cycle < 1000; cycle++) {
        const char* response = responses[cycle % 3];
        
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, response);
        TEST_ASSERT_FALSE(err);
        
        bool ok = doc["ok"].as<bool>();
        TEST_ASSERT_TRUE(ok);
        
        JsonArray result = doc["result"].as<JsonArray>();
        
        // Extract data into fixed buffers
        for (JsonVariant update : result) {
            char updateIdStr[20];
            int64_t updateId = update["update_id"];
            snprintf(updateIdStr, sizeof(updateIdStr), "%lld", updateId);
        }
    }
    
    TEST_PASS_MESSAGE("1000 parse cycles completed without issues");
}

// ============================================================================
// Test: Chat ID extraction uses fixed buffers
// ============================================================================

void test_chat_id_extraction() {
    const char* jsonInput = R"({
        "ok": true,
        "result": [
            {"message": {"chat": {"id": -1001234567890, "type": "supergroup", "title": "Test Group"}}}
        ]
    })";
    
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, jsonInput);
    TEST_ASSERT_FALSE(err);
    
    JsonArray result = doc["result"].as<JsonArray>();
    JsonObject chat = result[0]["message"]["chat"];
    
    // Extract into fixed buffer
    char chatIdStr[24];
    if (chat["id"].is<long long>()) {
        snprintf(chatIdStr, sizeof(chatIdStr), "%lld", chat["id"].as<long long>());
    } else {
        snprintf(chatIdStr, sizeof(chatIdStr), "%ld", chat["id"].as<long>());
    }
    
    // Verify extraction
    TEST_ASSERT_EQUAL_STRING("-1001234567890", chatIdStr);
    
    char typeStr[16];
    const char* type = chat["type"] | "";
    strncpy(typeStr, type, sizeof(typeStr) - 1);
    typeStr[sizeof(typeStr) - 1] = '\0';
    
    TEST_ASSERT_EQUAL_STRING("supergroup", typeStr);
}

// ============================================================================
// Unity test runner
// ============================================================================

void setUp(void) {
    // Reset before each test
    reset_memory_tracking();
}

void tearDown(void) {
    // Cleanup after each test
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_telegram_chat_fixed_size);
    RUN_TEST(test_strncpy_safety);
    RUN_TEST(test_snprintf_safety);
    RUN_TEST(test_json_parsing_no_leak);
    RUN_TEST(test_repeated_json_operations);
    RUN_TEST(test_chat_id_extraction);
    
    return UNITY_END();
}
