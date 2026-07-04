/**
 * @file test_telegram_reboot_command.cpp
 * @brief Unit tests for Telegram /reboot command scheduling
 */

#include <unity.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define NATIVE_BUILD 1

#include "../../src/notifications/telegram/commands/TelegramCommandTypes.h"

#include "../../src/notifications/telegram/commands/TelegramReplyBuilder.cpp"
#include "../../src/notifications/telegram/commands/RebootCommand.cpp"

namespace SYSTEM {
void ShutdownSequence::execute(ServiceRegistry&) {}
void ShutdownSequence::execute(ServiceRegistry&, ShutdownReason explicitReason) {
    (void)explicitReason;
}
}  // namespace SYSTEM

using namespace TELEGRAM::Commands;

void setUp(void) {
    TEST_STUBS::FREERTOS::resetTaskCreateStub();
    TELEGRAM::Commands::s_expectedRebootPin = 0;
    srand(1);
}

void tearDown(void) {}

static ParsedMessage createMsg(const char* text) {
    ParsedMessage msg = {};
    strncpy(msg.text, text, CMD_TEXT_MAX - 1);
    msg.text[CMD_TEXT_MAX - 1] = '\0';
    msg.isCommand = true;
    return msg;
}

static CommandContext createCtx(ParsedMessage& msg) {
    CommandContext ctx = {};
    ctx.msg = &msg;
    ctx.response[0] = '\0';
    ctx.responseLen = 0;
    ctx.shouldReply = false;
    return ctx;
}

static unsigned long extractPin(const CommandContext& ctx) {
    const char* marker = strstr(ctx.response, "/reboot ");
    TEST_ASSERT_NOT_NULL(marker);
    marker += strlen("/reboot ");
    return strtoul(marker, nullptr, 10);
}

void test_reboot_without_pin_returns_confirmation_code() {
    ParsedMessage msg = createMsg("/reboot");
    msg.commandArgs = std::string_view();
    CommandContext ctx = createCtx(msg);

    TEST_ASSERT_TRUE(handleReboot(ctx));
    TEST_ASSERT_TRUE(ctx.shouldReply);
    TEST_ASSERT_NOT_NULL(strstr(ctx.response, "Confirm with: /reboot "));
    TEST_ASSERT_NULL(TEST_STUBS::FREERTOS::lastTaskFunction);
}

void test_reboot_with_valid_pin_schedules_reboot_task() {
    ParsedMessage initialMsg = createMsg("/reboot");
    initialMsg.commandArgs = std::string_view();
    CommandContext initialCtx = createCtx(initialMsg);
    TEST_ASSERT_TRUE(handleReboot(initialCtx));
    const unsigned long pin = extractPin(initialCtx);

    char command[32];
    snprintf(command, sizeof(command), "%lu", pin);
    ParsedMessage confirmMsg = createMsg("/reboot 1234");
    confirmMsg.commandArgs = std::string_view(command);
    CommandContext confirmCtx = createCtx(confirmMsg);
    confirmCtx.registry = reinterpret_cast<ServiceRegistry*>(0x1234);

    TEST_STUBS::FREERTOS::taskCreateResult = pdPASS;
    TEST_STUBS::FREERTOS::createdTaskHandle = reinterpret_cast<TaskHandle_t>(0x1234);

    TEST_ASSERT_TRUE(handleReboot(confirmCtx));
    TEST_ASSERT_TRUE(confirmCtx.shouldReply);
    TEST_ASSERT_NOT_NULL(strstr(confirmCtx.response, "Device will reboot in a moment"));
    TEST_ASSERT_EQUAL_STRING("reboot_task", TEST_STUBS::FREERTOS::lastTaskName);
    TEST_ASSERT_NOT_NULL(TEST_STUBS::FREERTOS::lastTaskFunction);
}

void test_reboot_with_valid_pin_and_missing_registry_returns_error() {
    ParsedMessage initialMsg = createMsg("/reboot");
    initialMsg.commandArgs = std::string_view();
    CommandContext initialCtx = createCtx(initialMsg);
    TEST_ASSERT_TRUE(handleReboot(initialCtx));
    const unsigned long pin = extractPin(initialCtx);

    char command[32];
    snprintf(command, sizeof(command), "%lu", pin);
    ParsedMessage confirmMsg = createMsg("/reboot 1234");
    confirmMsg.commandArgs = std::string_view(command);
    CommandContext confirmCtx = createCtx(confirmMsg);

    TEST_ASSERT_TRUE(handleReboot(confirmCtx));
    TEST_ASSERT_TRUE(confirmCtx.shouldReply);
    TEST_ASSERT_NOT_NULL(strstr(confirmCtx.response, "service registry not available"));
    TEST_ASSERT_NULL(TEST_STUBS::FREERTOS::lastTaskFunction);
}

void test_reboot_with_valid_pin_reports_task_creation_failure() {
    ParsedMessage initialMsg = createMsg("/reboot");
    initialMsg.commandArgs = std::string_view();
    CommandContext initialCtx = createCtx(initialMsg);
    TEST_ASSERT_TRUE(handleReboot(initialCtx));
    const unsigned long pin = extractPin(initialCtx);

    char command[32];
    snprintf(command, sizeof(command), "%lu", pin);
    ParsedMessage confirmMsg = createMsg("/reboot 1234");
    confirmMsg.commandArgs = std::string_view(command);
    CommandContext confirmCtx = createCtx(confirmMsg);
    confirmCtx.registry = reinterpret_cast<ServiceRegistry*>(0x1234);

    TEST_STUBS::FREERTOS::taskCreateResult = pdFAIL;

    TEST_ASSERT_TRUE(handleReboot(confirmCtx));
    TEST_ASSERT_TRUE(confirmCtx.shouldReply);
    TEST_ASSERT_NOT_NULL(strstr(confirmCtx.response, "Failed to schedule reboot"));
    TEST_ASSERT_NULL(strstr(confirmCtx.response, "Device will reboot in a moment"));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_reboot_without_pin_returns_confirmation_code);
    RUN_TEST(test_reboot_with_valid_pin_schedules_reboot_task);
    RUN_TEST(test_reboot_with_valid_pin_and_missing_registry_returns_error);
    RUN_TEST(test_reboot_with_valid_pin_reports_task_creation_failure);
    return UNITY_END();
}
