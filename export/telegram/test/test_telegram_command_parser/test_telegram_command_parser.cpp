/**
 * @file test_telegram_command_parser.cpp
 * @brief Unit tests for TelegramCommandParser
 * 
 * Tests cover:
 * - Basic command parsing (/status, /help)
 * - Command with arguments (/set limit 25)
 * - Case insensitivity (/STATUS -> status)
 * - Bot mention handling (/status@mybot)
 * - Edge cases (empty, just "/", special characters)
 */

#include <unity.h>
#include <cstring>

#define NATIVE_BUILD 1

// Include types header (no dependencies)
#include "../../src/notifications/telegram/commands/TelegramCommandTypes.h"
// Include implementation directly for native build
#include "../../src/notifications/telegram/commands/TelegramCommandParser.cpp"

using namespace TELEGRAM::Commands;

void setUp(void) {}
void tearDown(void) {}

// Helper to create a message with text
static ParsedMessage createMsg(const char* text) {
    ParsedMessage msg = {};
    strncpy(msg.text, text, CMD_TEXT_MAX - 1);
    return msg;
}

// ============================================================================
// Test: Non-command messages
// ============================================================================

void test_not_a_command_plain_text() {
    ParsedMessage msg = createMsg("hello world");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_FALSE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName.empty());
    TEST_ASSERT_TRUE(msg.commandArgs.empty());
}

void test_not_a_command_empty() {
    ParsedMessage msg = createMsg("");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_FALSE(msg.isCommand);
}

void test_not_a_command_just_slash() {
    ParsedMessage msg = createMsg("/");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_FALSE(msg.isCommand);
}

void test_not_a_command_slash_space() {
    ParsedMessage msg = createMsg("/ something");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_FALSE(msg.isCommand);
}

// ============================================================================
// Test: Basic command parsing
// ============================================================================

void test_simple_command_status() {
    ParsedMessage msg = createMsg("/status");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName == "status");
    TEST_ASSERT_TRUE(msg.commandArgs.empty());
}

void test_simple_command_help() {
    ParsedMessage msg = createMsg("/help");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName == "help");
}

void test_command_single_char() {
    ParsedMessage msg = createMsg("/x");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName == "x");
}

// ============================================================================
// Test: Command with arguments
// ============================================================================

void test_command_with_single_arg() {
    ParsedMessage msg = createMsg("/set 25");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName == "set");
    TEST_ASSERT_FALSE(msg.commandArgs.empty());
    TEST_ASSERT_TRUE(msg.commandArgs == "25");
}

void test_command_with_multiple_args() {
    ParsedMessage msg = createMsg("/alarm temp above 30");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName == "alarm");
    TEST_ASSERT_FALSE(msg.commandArgs.empty());
    TEST_ASSERT_TRUE(msg.commandArgs == "temp above 30");
}

void test_command_extra_spaces_before_args() {
    ParsedMessage msg = createMsg("/set    value");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName == "set");
    TEST_ASSERT_TRUE(msg.commandArgs == "value");
}

void test_command_trailing_space_no_args() {
    ParsedMessage msg = createMsg("/status ");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName == "status");
    // Trailing space with nothing after = no args
    TEST_ASSERT_TRUE(msg.commandArgs.empty());
}

// ============================================================================
// Test: Case insensitivity
// ============================================================================

void test_command_uppercase() {
    ParsedMessage msg = createMsg("/STATUS");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName == "STATUS");
}

void test_command_mixed_case() {
    ParsedMessage msg = createMsg("/GetStatus");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName == "GetStatus");
}

// ============================================================================
// Test: Bot mention handling (@botname)
// ============================================================================

void test_command_with_bot_mention() {
    ParsedMessage msg = createMsg("/status@mybot");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName == "status");
    TEST_ASSERT_TRUE(msg.commandArgs.empty());
}

void test_command_with_bot_mention_and_args() {
    ParsedMessage msg = createMsg("/set@matrixhub_bot 25");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName == "set");
    TEST_ASSERT_FALSE(msg.commandArgs.empty());
    TEST_ASSERT_TRUE(msg.commandArgs == "25");
}

void test_command_with_bot_mention_extra_spaces() {
    ParsedMessage msg = createMsg("/help@bot   arg1 arg2");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName == "help");
    TEST_ASSERT_TRUE(msg.commandArgs == "arg1 arg2");
}

// ============================================================================
// Test: Edge cases and limits
// ============================================================================

void test_command_name_at_max_length() {
    // CMD_NAME_MAX is 32, so 31 chars + null
    char longCmd[CMD_TEXT_MAX];
    strcpy(longCmd, "/");
    for (int i = 0; i < 31; i++) {
        longCmd[i + 1] = 'a' + (i % 26);
    }
    longCmd[32] = '\0';
    
    ParsedMessage msg = createMsg(longCmd);
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    // Should be truncated to 31 chars (CMD_NAME_MAX - 1)
    TEST_ASSERT_EQUAL(31, msg.commandName.size());
}

void test_command_with_numbers() {
    ParsedMessage msg = createMsg("/alarm123");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName == "alarm123");
}

void test_command_with_underscore() {
    ParsedMessage msg = createMsg("/get_status");
    TelegramCommandParser::parseCommand(msg);
    
    TEST_ASSERT_TRUE(msg.isCommand);
    TEST_ASSERT_TRUE(msg.commandName == "get_status");
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    UNITY_BEGIN();
    
    // Non-command tests
    RUN_TEST(test_not_a_command_plain_text);
    RUN_TEST(test_not_a_command_empty);
    RUN_TEST(test_not_a_command_just_slash);
    RUN_TEST(test_not_a_command_slash_space);
    
    // Basic commands
    RUN_TEST(test_simple_command_status);
    RUN_TEST(test_simple_command_help);
    RUN_TEST(test_command_single_char);
    
    // Commands with arguments
    RUN_TEST(test_command_with_single_arg);
    RUN_TEST(test_command_with_multiple_args);
    RUN_TEST(test_command_extra_spaces_before_args);
    RUN_TEST(test_command_trailing_space_no_args);
    
    // Case insensitivity
    RUN_TEST(test_command_uppercase);
    RUN_TEST(test_command_mixed_case);
    
    // Bot mention
    RUN_TEST(test_command_with_bot_mention);
    RUN_TEST(test_command_with_bot_mention_and_args);
    RUN_TEST(test_command_with_bot_mention_extra_spaces);
    
    // Edge cases
    RUN_TEST(test_command_name_at_max_length);
    RUN_TEST(test_command_with_numbers);
    RUN_TEST(test_command_with_underscore);
    
    return UNITY_END();
}
