/**
 * @file test_telegram_help_command.cpp
 * @brief Unit tests for registry-driven Telegram /help output
 */

#include <unity.h>
#include <cstdio>
#include <cstring>

#define NATIVE_BUILD 1

#include "../../src/notifications/telegram/commands/TelegramCommandTypes.h"

namespace TELEGRAM::Commands {
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
#include "../../src/notifications/telegram/commands/HelpCommand.cpp"

using namespace TELEGRAM::Commands;

void setUp(void) {}
void tearDown(void) {}

static CommandContext createCtx() {
    CommandContext ctx = {};
    ctx.response[0] = '\0';
    ctx.responseLen = 0;
    ctx.shouldReply = false;
    return ctx;
}

void test_help_lists_every_registry_command_description() {
    CommandContext ctx = createCtx();

    TEST_ASSERT_TRUE(handleHelp(ctx));
    TEST_ASSERT_TRUE(ctx.shouldReply);
    TEST_ASSERT_EQUAL(strlen(ctx.response), ctx.responseLen);
    TEST_ASSERT_NULL(strstr(ctx.response, "[truncated]"));

    for (size_t i = 0; i < kCommandCount; i++) {
        char expected[160];
        snprintf(expected, sizeof(expected), "/%s - %s", kCommands[i].name, kCommands[i].description);
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(ctx.response, expected), expected);
    }
}

void test_help_keeps_exec_subcommands_documented() {
    CommandContext ctx = createCtx();

    TEST_ASSERT_TRUE(handleHelp(ctx));
    TEST_ASSERT_NOT_NULL(strstr(ctx.response, "/exec cancel - Send Ctrl+C to host"));
    TEST_ASSERT_NOT_NULL(strstr(ctx.response, "/exec status - Peek at command capture buffer"));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_help_lists_every_registry_command_description);
    RUN_TEST(test_help_keeps_exec_subcommands_documented);
    return UNITY_END();
}
