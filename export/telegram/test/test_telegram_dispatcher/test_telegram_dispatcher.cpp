/**
 * @file test_telegram_dispatcher.cpp
 * @brief Unit tests for Telegram command dispatcher responses
 */

#include <unity.h>
#include <cstdlib>
#include <cstring>

#define NATIVE_BUILD 1

#include "../../src/notifications/telegram/commands/TelegramCommandTypes.h"
#include "../../src/system/logging/Logging.h"

namespace LOG {
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
}  // namespace LOG

namespace TELEGRAM::Commands {
bool handleHelp(CommandContext&) { return true; }
bool handleStatus(CommandContext&) { return true; }
bool handleSensors(CommandContext&) { return true; }
bool handleAlarms(CommandContext&) { return true; }
bool handleShelly(CommandContext&) { return true; }
bool handleTriggered(CommandContext&) { return true; }
bool handleBle(CommandContext&) { return true; }
bool handleScripts(CommandContext&) { return true; }
bool handleRun(CommandContext&) { return true; }
bool handleMacroStop(CommandContext&) { return true; }
bool handleIp(CommandContext&) { return true; }
bool handleMatrix(CommandContext&) { return true; }
bool handleReboot(CommandContext&) { return true; }
bool handleExec(CommandContext&) { return true; }
bool handleUsers(CommandContext&) { return true; }
bool handleHealth(CommandContext&) { return true; }
}  // namespace TELEGRAM::Commands

#include "../../src/notifications/telegram/commands/TelegramReplyBuilder.cpp"
#include "../../src/notifications/telegram/commands/TelegramCommandDispatcher.cpp"

using namespace TELEGRAM::Commands;

void setUp(void) {
    srand(1);
}

void tearDown(void) {}

static ParsedMessage createUnknownCommand() {
    ParsedMessage msg = {};
    strncpy(msg.text, "/abracadabra", CMD_TEXT_MAX - 1);
    msg.text[CMD_TEXT_MAX - 1] = '\0';
    msg.isCommand = true;
    msg.commandName = std::string_view("abracadabra");
    msg.commandArgs = std::string_view();
    return msg;
}

static ParsedMessage createPlainTextMessage() {
    ParsedMessage msg = {};
    strncpy(msg.text, "hello there", CMD_TEXT_MAX - 1);
    msg.text[CMD_TEXT_MAX - 1] = '\0';
    msg.isCommand = false;
    msg.commandName = std::string_view();
    msg.commandArgs = std::string_view();
    return msg;
}

static CommandContext createCtx() {
    CommandContext ctx = {};
    ctx.response[0] = '\0';
    ctx.responseLen = 0;
    ctx.shouldReply = false;
    return ctx;
}

void test_unknown_command_returns_joke_and_help_only() {
    ParsedMessage msg = createUnknownCommand();
    CommandContext ctx = createCtx();

    TEST_ASSERT_TRUE(TelegramCommandDispatcher::dispatch(msg, ctx));
    TEST_ASSERT_TRUE(ctx.shouldReply);
    TEST_ASSERT_EQUAL(strlen(ctx.response), ctx.responseLen);
    TEST_ASSERT_NULL(strstr(ctx.response, "Unknown command"));
    TEST_ASSERT_NULL(strstr(ctx.response, "Use /help"));

    const char* suffix = "\n\n/help\n";
    const size_t suffixLen = strlen(suffix);
    TEST_ASSERT_TRUE(suffixLen > 0);
    TEST_ASSERT_TRUE(ctx.responseLen >= suffixLen);
    TEST_ASSERT_EQUAL_STRING(suffix, ctx.response + ctx.responseLen - suffixLen);
}

void test_plain_text_message_returns_joke_and_help_only() {
    ParsedMessage msg = createPlainTextMessage();
    CommandContext ctx = createCtx();

    TEST_ASSERT_TRUE(TelegramCommandDispatcher::dispatch(msg, ctx));
    TEST_ASSERT_TRUE(ctx.shouldReply);
    TEST_ASSERT_EQUAL(strlen(ctx.response), ctx.responseLen);
    TEST_ASSERT_NULL(strstr(ctx.response, "Unknown command"));

    const char* suffix = "\n\n/help\n";
    const size_t suffixLen = strlen(suffix);
    TEST_ASSERT_TRUE(ctx.responseLen >= suffixLen);
    TEST_ASSERT_EQUAL_STRING(suffix, ctx.response + ctx.responseLen - suffixLen);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_unknown_command_returns_joke_and_help_only);
    RUN_TEST(test_plain_text_message_returns_joke_and_help_only);
    return UNITY_END();
}
