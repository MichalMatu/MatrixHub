/**
 * @file test_telegram_reply_builder.cpp
 * @brief Unit tests for TelegramReplyBuilder
 */

#include <unity.h>
#include <cstring>

#define NATIVE_BUILD 1

#include "../../src/notifications/telegram/commands/TelegramCommandTypes.h"
#include "../../src/notifications/telegram/commands/TelegramReplyBuilder.cpp"

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

void test_header_and_lines_render_consistently() {
    CommandContext ctx = createCtx();
    TelegramReplyBuilder reply(ctx);

    reply.header("H", "Title");
    reply.line("Body");
    reply.finalize();

    TEST_ASSERT_TRUE(ctx.shouldReply);
    TEST_ASSERT_EQUAL_STRING("H Title\n\nBody\n", ctx.response);
    TEST_ASSERT_EQUAL(strlen(ctx.response), ctx.responseLen);
}

void test_usage_formats_usage_and_hint() {
    CommandContext ctx = createCtx();
    TelegramReplyBuilder reply(ctx);

    reply.usage("/run <name>", "Use /scripts first");
    reply.finalize();

    TEST_ASSERT_EQUAL_STRING(
        "Usage: /run <name>\n"
        "Hint: Use /scripts first\n",
        ctx.response);
}

void test_key_value_line_is_single_line() {
    CommandContext ctx = createCtx();
    TelegramReplyBuilder reply(ctx);

    reply.kvf("I", "Value", "%d", 42);
    reply.finalize();

    TEST_ASSERT_EQUAL_STRING("I Value: 42\n", ctx.response);
}

void test_detail_line_is_indented() {
    CommandContext ctx = createCtx();
    TelegramReplyBuilder reply(ctx);

    reply.detailf("Child: %s", "ok");
    reply.finalize();

    TEST_ASSERT_EQUAL_STRING("  Child: ok\n", ctx.response);
}

void test_truncation_appends_suffix() {
    CommandContext ctx = createCtx();
    TelegramReplyBuilder reply(ctx);

    char large[1400];
    memset(large, 'A', sizeof(large) - 1);
    large[sizeof(large) - 1] = '\0';

    reply.linef("%s", large);
    reply.finalize();

    TEST_ASSERT_TRUE(reply.isTruncated());
    TEST_ASSERT_TRUE(ctx.shouldReply);
    TEST_ASSERT_NOT_NULL(strstr(ctx.response, "[truncated]"));
    TEST_ASSERT_EQUAL(strlen(ctx.response), ctx.responseLen);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_header_and_lines_render_consistently);
    RUN_TEST(test_usage_formats_usage_and_hint);
    RUN_TEST(test_key_value_line_is_single_line);
    RUN_TEST(test_detail_line_is_indented);
    RUN_TEST(test_truncation_appends_suffix);
    return UNITY_END();
}
