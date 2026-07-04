/**
 * @file test_validator.cpp
 * @brief Unit tests for Pushover send request validation
 */

#ifdef NATIVE_BUILD
#include <unity.h>

#include "../../src/notifications/pushover/PushoverSendValidator.h"

using NOTIFICATIONS::PUSHOVER::PushoverSendValidationInput;
using NOTIFICATIONS::PUSHOVER::PushoverSendValidator;

void setUp(void) {}
void tearDown(void) {}

void test_missing_settings_uses_custom_error() {
    PushoverSendValidationInput input;
    input.settingsAvailable = false;
    input.missingSettingsError = "not_initialized";

    const auto result = PushoverSendValidator::validate(input);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL_STRING("not_initialized", result.error);
    TEST_ASSERT_NULL(result.userKey);
    TEST_ASSERT_NULL(result.apiToken);
}

void test_disabled_mode_uses_configured_error_code() {
    PushoverSendValidationInput input;
    input.settingsAvailable = true;
    input.enabled = false;
    input.configured = true;
    input.userKey = "user";
    input.apiToken = "token";

    const auto result = PushoverSendValidator::validate(input);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL_STRING("disabled", result.error);
}

void test_missing_configuration_is_rejected() {
    PushoverSendValidationInput input;
    input.settingsAvailable = true;
    input.enabled = true;
    input.configured = false;
    input.userKey = "user";
    input.apiToken = "token";

    const auto result = PushoverSendValidator::validate(input);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL_STRING("not_configured", result.error);
}

void test_valid_request_returns_credentials() {
    const char* expectedUser = "user";
    const char* expectedToken = "token";

    PushoverSendValidationInput input;
    input.settingsAvailable = true;
    input.enabled = true;
    input.configured = true;
    input.userKey = expectedUser;
    input.apiToken = expectedToken;

    const auto result = PushoverSendValidator::validate(input);

    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_NULL(result.error);
    TEST_ASSERT_EQUAL_PTR(expectedUser, result.userKey);
    TEST_ASSERT_EQUAL_PTR(expectedToken, result.apiToken);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_missing_settings_uses_custom_error);
    RUN_TEST(test_disabled_mode_uses_configured_error_code);
    RUN_TEST(test_missing_configuration_is_rejected);
    RUN_TEST(test_valid_request_returns_credentials);
    return UNITY_END();
}
#endif
