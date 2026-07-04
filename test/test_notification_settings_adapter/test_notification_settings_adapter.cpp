#define NOMINMAX

#include <unity.h>
#include <ArduinoJson.h>

#include <string>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "../../src/notifications/settings/NotificationSettingsAdapter.cpp"

void setUp(void) {}
void tearDown(void) {}

void test_read_exposes_notification_fields_and_configured_flag() {
    RTC::NotificationData settings{};
    settings.telegramEnabled = true;
    strlcpy(settings.botToken, "bot-token", sizeof(settings.botToken));
    strlcpy(settings.chatId, "123456", sizeof(settings.chatId));
    settings.commandsEnabled = true;
    settings.webhookEnabled = true;
    strlcpy(settings.webhookUrl, "https://example.test/hook", sizeof(settings.webhookUrl));
    settings.pushoverEnabled = true;
    strlcpy(settings.pushoverUserKey, "pushover-user", sizeof(settings.pushoverUserKey));
    strlcpy(settings.pushoverApiToken, "pushover-token", sizeof(settings.pushoverApiToken));

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    NotificationSettingsAdapter::read(settings, root);

    TEST_ASSERT_TRUE(root[CONFIG::Keys::kTelegramEnabled].as<bool>());
    TEST_ASSERT_TRUE(root[CONFIG::Keys::kWebhookEnabled].as<bool>());
    TEST_ASSERT_TRUE(root[CONFIG::Keys::kCommandsEnabled].as<bool>());
    TEST_ASSERT_TRUE(root[CONFIG::Keys::kPushoverEnabled].as<bool>());
    TEST_ASSERT_EQUAL_STRING("bot-token", root[CONFIG::Keys::kBotToken].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("123456", root[CONFIG::Keys::kChatId].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("https://example.test/hook", root[CONFIG::Keys::kWebhookUrl].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("pushover-user", root[CONFIG::Keys::kPushoverUser].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("pushover-token", root[CONFIG::Keys::kPushoverToken].as<const char*>());
    TEST_ASSERT_TRUE(root["is_configured"].as<bool>());
}

void test_update_bot_token_clears_chat_id_and_reports_change() {
    RTC::NotificationData settings{};
    strlcpy(settings.botToken, "old-token", sizeof(settings.botToken));
    strlcpy(settings.chatId, "keep-me", sizeof(settings.chatId));

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root[CONFIG::Keys::kBotToken] = "new-token";

    const StateUpdateResult result = NotificationSettingsAdapter::update(root, settings, "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::CHANGED, result);
    TEST_ASSERT_EQUAL_STRING("new-token", settings.botToken);
    TEST_ASSERT_EQUAL('\0', settings.chatId[0]);
}

void test_update_ignores_chat_id_from_frontend() {
    RTC::NotificationData settings{};
    strlcpy(settings.chatId, "auto-discovered", sizeof(settings.chatId));

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root[CONFIG::Keys::kChatId] = "frontend-override";

    const StateUpdateResult result = NotificationSettingsAdapter::update(root, settings, "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::UNCHANGED, result);
    TEST_ASSERT_EQUAL_STRING("auto-discovered", settings.chatId);
}

void test_update_truncated_webhook_url_same_value_is_unchanged() {
    RTC::NotificationData settings{};
    const std::string overlong(sizeof(settings.webhookUrl) + 32, 'a');
    strlcpy(settings.webhookUrl, overlong.c_str(), sizeof(settings.webhookUrl));

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root[CONFIG::Keys::kWebhookUrl] = overlong.c_str();

    const StateUpdateResult result = NotificationSettingsAdapter::update(root, settings, "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::UNCHANGED, result);
    TEST_ASSERT_EQUAL_UINT(sizeof(settings.webhookUrl) - 1, strlen(settings.webhookUrl));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_read_exposes_notification_fields_and_configured_flag);
    RUN_TEST(test_update_bot_token_clears_chat_id_and_reports_change);
    RUN_TEST(test_update_ignores_chat_id_from_frontend);
    RUN_TEST(test_update_truncated_webhook_url_same_value_is_unchanged);
    return UNITY_END();
}
