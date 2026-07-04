/**
 * @file test_telegram_json_parsers.cpp
 * @brief Unit tests for TelegramJsonParsers
 */

#include <unity.h>
#include <cstring>
#include "../stubs/NetworkClient.h"

// Include implementation
#include "../../src/system/utils/json/JsonStreamReader.cpp"
#include "../../src/notifications/telegram/polling/TelegramUpdatesLite.h"
#include "../../src/config/App.h" // The config header in this project seems to be App.h or Systems.h, let's use the define if it exists, or just hardcode it for the test
#include "../../src/notifications/telegram/polling/TelegramJsonParsers.cpp"

using namespace TELEGRAM;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// parseChatObject Tests
// ============================================================================

void test_parse_chat_object_positive_id() {
    NetworkClient client;
    client.setMockData(R"({"id":12345,"type":"private"})");
    
    Utils::JsonStreamReader reader(client);
    UpdateLite update = {};
    
    bool result = TelegramJsonParsers::parseChatObject(reader, update);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("12345", update.chatId);
}

void test_parse_chat_object_negative_id() {
    NetworkClient client;
    client.setMockData(R"({"id":-1001234567890,"type":"supergroup"})");
    
    Utils::JsonStreamReader reader(client);
    UpdateLite update = {};
    
    bool result = TelegramJsonParsers::parseChatObject(reader, update);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("-1001234567890", update.chatId);
}

void test_parse_chat_object_unknown_fields() {
    NetworkClient client;
    client.setMockData(R"({"id":999,"title":"Test","username":"test_chat"})");
    
    Utils::JsonStreamReader reader(client);
    UpdateLite update = {};
    
    bool result = TelegramJsonParsers::parseChatObject(reader, update);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("999", update.chatId);
}

void test_parse_chat_object_invalid_not_object() {
    NetworkClient client;
    client.setMockData(R"([1,2,3])");
    
    Utils::JsonStreamReader reader(client);
    UpdateLite update = {};
    
    bool result = TelegramJsonParsers::parseChatObject(reader, update);
    
    TEST_ASSERT_FALSE(result);
}

// ============================================================================
// parseFromObject Tests
// ============================================================================

void test_parse_from_object_with_username() {
    NetworkClient client;
    client.setMockData(R"({"id":123,"username":"john_doe","first_name":"John"})");
    
    Utils::JsonStreamReader reader(client);
    UpdateLite update = {};
    
    bool result = TelegramJsonParsers::parseFromObject(reader, update);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("john_doe", update.fromUsername);
}

void test_parse_from_object_no_username() {
    NetworkClient client;
    client.setMockData(R"({"id":123,"first_name":"John"})");
    
    Utils::JsonStreamReader reader(client);
    UpdateLite update = {};
    
    bool result = TelegramJsonParsers::parseFromObject(reader, update);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("", update.fromUsername);
}

void test_parse_from_object_empty() {
    NetworkClient client;
    client.setMockData(R"({})");
    
    Utils::JsonStreamReader reader(client);
    UpdateLite update = {};
    
    bool result = TelegramJsonParsers::parseFromObject(reader, update);
    
    TEST_ASSERT_TRUE(result);
}

// ============================================================================
// parseMessageObject Tests
// ============================================================================

void test_parse_message_object_complete() {
    NetworkClient client;
    client.setMockData(R"({
        "message_id":100,
        "from":{"id":123,"username":"alice"},
        "chat":{"id":456,"type":"private"},
        "date":1704672000,
        "text":"Hello world"
    })");
    
    Utils::JsonStreamReader reader(client);
    UpdateLite update = {};
    
    bool result = TelegramJsonParsers::parseMessageObject(reader, update);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(update.hasMessage);
    TEST_ASSERT_EQUAL_STRING("Hello world", update.text);
    TEST_ASSERT_EQUAL_STRING("456", update.chatId);
    TEST_ASSERT_EQUAL_STRING("alice", update.fromUsername);
    TEST_ASSERT_EQUAL_INT64(1704672000, update.date);
}

void test_parse_message_object_text_only() {
    NetworkClient client;
    client.setMockData(R"({"text":"Just text"})");
    
    Utils::JsonStreamReader reader(client);
    UpdateLite update = {};
    
    bool result = TelegramJsonParsers::parseMessageObject(reader, update);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(update.hasMessage);
    TEST_ASSERT_EQUAL_STRING("Just text", update.text);
}

void test_parse_message_object_no_text() {
    NetworkClient client;
    client.setMockData(R"({"photo":[{"file_id":"abc123"}]})");
    
    Utils::JsonStreamReader reader(client);
    UpdateLite update = {};
    
    bool result = TelegramJsonParsers::parseMessageObject(reader, update);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(update.hasMessage);
    TEST_ASSERT_EQUAL_STRING("", update.text);
}

void test_parse_message_object_nested_objects() {
    NetworkClient client;
    client.setMockData(R"({
        "chat":{"id":-100,"type":"supergroup"},
        "from":{"id":1,"username":"bob","first_name":"Bob"},
        "text":"/start"
    })");
    
    Utils::JsonStreamReader reader(client);
    UpdateLite update = {};
    
    bool result = TelegramJsonParsers::parseMessageObject(reader, update);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("-100", update.chatId);
    TEST_ASSERT_EQUAL_STRING("bob", update.fromUsername);
    TEST_ASSERT_EQUAL_STRING("/start", update.text);
}

// ============================================================================
// parseUpdateObject Tests
// ============================================================================

void test_parse_update_object_with_message() {
    NetworkClient client;
    client.setMockData(R"({
        "update_id":987654321,
        "message":{
            "message_id":1,
            "from":{"id":10,"username":"user1"},
            "chat":{"id":20},
            "date":1704700000,
            "text":"Test message"
        }
    })");
    
    Utils::JsonStreamReader reader(client);
    UpdateLite update = {};
    
    bool result = TelegramJsonParsers::parseUpdateObject(reader, update);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_INT64(987654321, update.updateId);
    TEST_ASSERT_TRUE(update.hasMessage);
    TEST_ASSERT_EQUAL_STRING("Test message", update.text);
    TEST_ASSERT_EQUAL_STRING("20", update.chatId);
    TEST_ASSERT_EQUAL_STRING("user1", update.fromUsername);
}

void test_parse_update_object_edited_message() {
    NetworkClient client;
    client.setMockData(R"({
        "update_id":111,
        "edited_message":{
            "text":"Edited text",
            "chat":{"id":30}
        }
    })");
    
    Utils::JsonStreamReader reader(client);
    UpdateLite update = {};
    
    bool result = TelegramJsonParsers::parseUpdateObject(reader, update);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_INT64(111, update.updateId);
    TEST_ASSERT_TRUE(update.hasMessage);
    TEST_ASSERT_EQUAL_STRING("Edited text", update.text);
}

void test_parse_update_object_channel_post() {
    NetworkClient client;
    client.setMockData(R"({
        "update_id":222,
        "channel_post":{
            "text":"Channel message",
            "chat":{"id":-1001234}
        }
    })");
    
    Utils::JsonStreamReader reader(client);
    UpdateLite update = {};
    
    bool result = TelegramJsonParsers::parseUpdateObject(reader, update);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_INT64(222, update.updateId);
    TEST_ASSERT_TRUE(update.hasMessage);
    TEST_ASSERT_EQUAL_STRING("Channel message", update.text);
}

void test_parse_update_object_no_message() {
    NetworkClient client;
    client.setMockData(R"({
        "update_id":333,
        "callback_query":{"id":"abc"}
    })");
    
    Utils::JsonStreamReader reader(client);
    UpdateLite update = {};
    
    bool result = TelegramJsonParsers::parseUpdateObject(reader, update);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_INT64(333, update.updateId);
    TEST_ASSERT_FALSE(update.hasMessage);
}

void test_parse_update_object_resets_state() {
    NetworkClient client;
    client.setMockData(R"({"update_id":444})");
    
    Utils::JsonStreamReader reader(client);
    UpdateLite update = {};
    
    // Pre-fill with data
    update.updateId = 999;
    strcpy(update.text, "Old text");
    strcpy(update.chatId, "999");
    update.hasMessage = true;
    
    bool result = TelegramJsonParsers::parseUpdateObject(reader, update);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_INT64(444, update.updateId);
    TEST_ASSERT_EQUAL_STRING("", update.text);
    TEST_ASSERT_EQUAL_STRING("", update.chatId);
    TEST_ASSERT_FALSE(update.hasMessage);
}

void test_parse_update_object_deeply_nested_json_memory_safety() {
    // A massive nested JSON that could potentially overflow a naive parser stack
    NetworkClient client;
    std::string payload = "{\"update_id\":555, \"message\": {\"text\": \"Safe\", \"nested\":";
    for(int i=0; i<100; i++) payload += "{\"a\":";
    payload += "1";
    for(int i=0; i<100; i++) payload += "}";
    payload += "}}";
    
    client.setMockData(payload.c_str());
    
    Utils::JsonStreamReader reader(client);
    UpdateLite update = {};
    
    // As long as it doesn't crash from stack overflow, it passes this stress test
    bool result = TelegramJsonParsers::parseUpdateObject(reader, update);
    
    // Depending on the stream reader's exact limits, it may fail or succeed parsing due to depth.
    // The main assertion here is that it RETURNS rather than crashing the RTOS thread.
}

void test_parse_message_long_string_truncation() {
    NetworkClient client;
    // Create a string that exceeds TELEGRAM::Commands::CMD_TEXT_MAX.
    std::string longText = "{\"message_id\":1,\"text\":\"";
    for(int i=0; i < TELEGRAM::Commands::CMD_TEXT_MAX + 100; i++) longText += "X";
    longText += "\"}";
    
    client.setMockData(longText.c_str());
    
    Utils::JsonStreamReader reader(client);
    TELEGRAM::UpdateLite update = {};
    
    bool result = TelegramJsonParsers::parseMessageObject(reader, update);
    
    TEST_ASSERT_TRUE(result);
    // Buffer should be perfectly truncated and null terminated without stomping on memory
    TEST_ASSERT_EQUAL_INT(TELEGRAM::Commands::CMD_TEXT_MAX - 1, strlen(update.text));
    TEST_ASSERT_EQUAL('X', update.text[TELEGRAM::Commands::CMD_TEXT_MAX - 2]);
    TEST_ASSERT_EQUAL('\0', update.text[TELEGRAM::Commands::CMD_TEXT_MAX - 1]);
}

// ============================================================================
// Unity Setup
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // parseChatObject
    RUN_TEST(test_parse_chat_object_positive_id);
    RUN_TEST(test_parse_chat_object_negative_id);
    RUN_TEST(test_parse_chat_object_unknown_fields);
    RUN_TEST(test_parse_chat_object_invalid_not_object);
    
    // parseFromObject
    RUN_TEST(test_parse_from_object_with_username);
    RUN_TEST(test_parse_from_object_no_username);
    RUN_TEST(test_parse_from_object_empty);
    
    // parseMessageObject
    RUN_TEST(test_parse_message_object_complete);
    RUN_TEST(test_parse_message_object_text_only);
    RUN_TEST(test_parse_message_object_no_text);
    RUN_TEST(test_parse_message_object_nested_objects);
    
    // parseUpdateObject
    RUN_TEST(test_parse_update_object_with_message);
    RUN_TEST(test_parse_update_object_edited_message);
    RUN_TEST(test_parse_update_object_channel_post);
    RUN_TEST(test_parse_update_object_no_message);
    RUN_TEST(test_parse_update_object_resets_state);
    RUN_TEST(test_parse_update_object_deeply_nested_json_memory_safety);
    RUN_TEST(test_parse_message_long_string_truncation);
    
    return UNITY_END();
}
