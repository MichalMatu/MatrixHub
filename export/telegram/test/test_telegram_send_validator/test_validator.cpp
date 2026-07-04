/**
 * @file test_validator.cpp
 * @brief Unit tests for Telegram send request validation
 */

#ifdef NATIVE_BUILD
#include <unity.h>

#include "../../src/config/App.h"
#include "../../src/notifications/telegram/TelegramSendValidator.h"

using NOTIFICATIONS::TELEGRAM::TelegramSendValidationInput;
using NOTIFICATIONS::TELEGRAM::TelegramSendValidator;

void setUp(void) {}
void tearDown(void) {}

void test_missing_settings_uses_custom_error() {
    TelegramSendValidationInput input;
    input.settingsAvailable = false;
    input.missingSettingsError = "not_initialized";

    const auto result = TelegramSendValidator::validate(input);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL_STRING("not_initialized", result.error);
    TEST_ASSERT_NULL(result.chatId);
}

void test_disabled_mode_is_rejected() {
    TelegramSendValidationInput input;
    input.settingsAvailable = true;
    input.enabled = false;
    input.configured = true;
    input.chatId = "123";
    input.textLen = 4;

    const auto result = TelegramSendValidator::validate(input);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL_STRING("mode_not_telegram", result.error);
}

void test_missing_configuration_is_rejected() {
    TelegramSendValidationInput input;
    input.settingsAvailable = true;
    input.enabled = true;
    input.configured = false;
    input.chatId = "123";
    input.textLen = 4;

    const auto result = TelegramSendValidator::validate(input);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL_STRING("not_configured", result.error);
}

void test_empty_text_is_rejected() {
    TelegramSendValidationInput input;
    input.settingsAvailable = true;
    input.enabled = true;
    input.configured = true;
    input.chatId = "123";
    input.textLen = 0;

    const auto result = TelegramSendValidator::validate(input);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL_STRING("empty_text", result.error);
}

void test_too_long_text_is_rejected() {
    TelegramSendValidationInput input;
    input.settingsAvailable = true;
    input.enabled = true;
    input.configured = true;
    input.chatId = "123";
    input.textLen = APP::NOTIFICATIONS::TELEGRAM_MAX_TEXT_LEN + 1;

    const auto result = TelegramSendValidator::validate(input);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL_STRING("text_too_long", result.error);
}

void test_valid_request_returns_chat_id() {
    const char* expectedChatId = "123456";
    TelegramSendValidationInput input;
    input.settingsAvailable = true;
    input.enabled = true;
    input.configured = true;
    input.chatId = expectedChatId;
    input.textLen = 12;

    const auto result = TelegramSendValidator::validate(input);

    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_NULL(result.error);
    TEST_ASSERT_EQUAL_PTR(expectedChatId, result.chatId);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_missing_settings_uses_custom_error);
    RUN_TEST(test_disabled_mode_is_rejected);
    RUN_TEST(test_missing_configuration_is_rejected);
    RUN_TEST(test_empty_text_is_rejected);
    RUN_TEST(test_too_long_text_is_rejected);
    RUN_TEST(test_valid_request_returns_chat_id);
    return UNITY_END();
}
#endif
