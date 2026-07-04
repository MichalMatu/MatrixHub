/**
 * @file test_validator.cpp
 * @brief Unit tests for Webhook send request validation
 */

#ifdef NATIVE_BUILD
#include <unity.h>

#include "../../src/config/System.h"
#include "../../src/notifications/webhook/WebhookSendValidator.h"

using NOTIFICATIONS::WEBHOOK::WebhookSendValidationInput;
using NOTIFICATIONS::WEBHOOK::WebhookSendValidator;

void setUp(void) {}
void tearDown(void) {}

void test_missing_settings_uses_custom_error() {
    WebhookSendValidationInput input;
    input.settingsAvailable = false;
    input.missingSettingsError = "not_initialized";

    const auto result = WebhookSendValidator::validate(input);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL_STRING("not_initialized", result.error);
    TEST_ASSERT_NULL(result.url);
}

void test_disabled_mode_is_rejected() {
    WebhookSendValidationInput input;
    input.settingsAvailable = true;
    input.enabled = false;
    input.configured = true;
    input.url = "https://example.test/hook";
    input.payloadLen = 16;

    const auto result = WebhookSendValidator::validate(input);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL_STRING("mode_not_webhook", result.error);
}

void test_missing_configuration_is_rejected() {
    WebhookSendValidationInput input;
    input.settingsAvailable = true;
    input.enabled = true;
    input.configured = false;
    input.url = "https://example.test/hook";
    input.payloadLen = 16;

    const auto result = WebhookSendValidator::validate(input);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL_STRING("not_configured", result.error);
}

void test_invalid_length_is_rejected() {
    WebhookSendValidationInput input;
    input.settingsAvailable = true;
    input.enabled = true;
    input.configured = true;
    input.url = "https://example.test/hook";
    input.payloadLen = CONFIG::NOTIFICATIONS::WEBHOOK::MAX_PAYLOAD;

    const auto result = WebhookSendValidator::validate(input);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL_STRING("invalid_length", result.error);
}

void test_valid_request_returns_url() {
    const char* expectedUrl = "https://example.test/hook";

    WebhookSendValidationInput input;
    input.settingsAvailable = true;
    input.enabled = true;
    input.configured = true;
    input.url = expectedUrl;
    input.payloadLen = 32;

    const auto result = WebhookSendValidator::validate(input);

    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_NULL(result.error);
    TEST_ASSERT_EQUAL_PTR(expectedUrl, result.url);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_missing_settings_uses_custom_error);
    RUN_TEST(test_disabled_mode_is_rejected);
    RUN_TEST(test_missing_configuration_is_rejected);
    RUN_TEST(test_invalid_length_is_rejected);
    RUN_TEST(test_valid_request_returns_url);
    return UNITY_END();
}
#endif
