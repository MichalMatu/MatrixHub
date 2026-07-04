/**
 * @file test_telegram_parser.cpp
 * @brief Unit tests for TelegramCommandParser pure logic
 */

#include <unity.h>
#include <cstring>
#include "../../src/notifications/telegram/commands/TelegramCommandParser.h"
#include "../../src/notifications/telegram/commands/TelegramCommandParser.cpp" // Include impl for native linking

using namespace TELEGRAM::Commands;

// ============================================================================
// Helpers
// ============================================================================

ParsedMessage createMsg(const char* text) {
    ParsedMessage msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    return msg;
}

// ============================================================================
// Tests
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

void test_basic_command() {
    ParsedMessage msg = createMsg("/status");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName == "status");
    TEST_ASSERT_TRUE(msg.commandArgs.empty());
}

void test_command_with_args() {
    ParsedMessage msg = createMsg("/set threshold 25");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName == "set");
    TEST_ASSERT_FALSE(msg.commandArgs.empty());
    TEST_ASSERT_TRUE(msg.commandArgs == "threshold 25");
}

void test_case_insensitive() {
    ParsedMessage msg = createMsg("/StaTus");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName == "StaTus");
}

void test_bot_name_handling() {
    ParsedMessage msg = createMsg("/status@my_bot_name arg");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName == "status");
    TEST_ASSERT_FALSE(msg.commandArgs.empty());
    TEST_ASSERT_TRUE(msg.commandArgs == "arg");
}

void test_bot_name_no_args() {
    ParsedMessage msg = createMsg("/status@my_bot_name");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName == "status");
    TEST_ASSERT_TRUE(msg.commandArgs.empty());
}

void test_not_a_command() {
    ParsedMessage msg = createMsg("Just a message");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_FALSE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName.empty());
}

void test_empty_command() {
    ParsedMessage msg = createMsg("/");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_FALSE(msg.isCommand);
}

void test_command_too_long() {
    // Construct a command longer than CMD_NAME_MAX (32)
    char buffer[100] = "/";
    for(int i=0; i<50; i++) strcat(buffer, "a");
    
    ParsedMessage msg = createMsg(buffer);
    TelegramCommandParser::parseCommand(msg);
    
    // Parser should accept long commands; no truncation here
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_EQUAL(50, msg.commandName.size());
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_basic_command);
    RUN_TEST(test_command_with_args);
    RUN_TEST(test_case_insensitive);
    RUN_TEST(test_bot_name_handling);
    RUN_TEST(test_bot_name_no_args);
    RUN_TEST(test_not_a_command);
    RUN_TEST(test_empty_command);
    RUN_TEST(test_command_too_long);
    return UNITY_END();
}
