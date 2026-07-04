#define NOMINMAX

#include <unity.h>
#include <ArduinoJson.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "../../src/config/json/NotificationSettingsJson.cpp"

namespace NOTIFICATIONS {
namespace CONFIG_STORE {

static RTC::NotificationData mockData;

RTC::NotificationData copy() {
    return mockData;
}

void withConfig(const std::function<void(const RTC::NotificationData&)>& reader) {
    reader(mockData);
}

bool update(const std::function<void(RTC::NotificationData&)>& updater) {
    updater(mockData);
    return true;
}

}  // namespace CONFIG_STORE
}  // namespace NOTIFICATIONS

void setUp(void) {
    auto& data = NOTIFICATIONS::CONFIG_STORE::mockData;
    data = RTC::NotificationData{};
    data.telegramEnabled = true;
    data.webhookEnabled = true;
    data.commandsEnabled = true;
    data.pushoverEnabled = true;
    strlcpy(data.botToken, "factory-token", sizeof(data.botToken));
    strlcpy(data.chatId, "chat-123", sizeof(data.chatId));
    strlcpy(data.webhookUrl, "https://old.example/hook", sizeof(data.webhookUrl));
    strlcpy(data.pushoverUserKey, "old-user", sizeof(data.pushoverUserKey));
    strlcpy(data.pushoverApiToken, "old-token", sizeof(data.pushoverApiToken));
}

void tearDown(void) {}

void test_load_notification_preserves_missing_fields() {
    JsonDocument doc;
    doc[CONFIG::Keys::kWebhookUrl] = "https://new.example/hook";

    JsonObject obj = doc.as<JsonObject>();
    CONFIG::JSON::loadNotification(obj);

    const auto& data = NOTIFICATIONS::CONFIG_STORE::mockData;
    TEST_ASSERT_TRUE(data.telegramEnabled);
    TEST_ASSERT_TRUE(data.webhookEnabled);
    TEST_ASSERT_TRUE(data.commandsEnabled);
    TEST_ASSERT_TRUE(data.pushoverEnabled);
    TEST_ASSERT_EQUAL_STRING("factory-token", data.botToken);
    TEST_ASSERT_EQUAL_STRING("chat-123", data.chatId);
    TEST_ASSERT_EQUAL_STRING("https://new.example/hook", data.webhookUrl);
    TEST_ASSERT_EQUAL_STRING("old-user", data.pushoverUserKey);
    TEST_ASSERT_EQUAL_STRING("old-token", data.pushoverApiToken);
}

void test_load_notification_legacy_mode_replaces_both_channel_flags() {
    JsonDocument doc;
    doc[CONFIG::Keys::kMode] = "telegram";

    JsonObject obj = doc.as<JsonObject>();
    CONFIG::JSON::loadNotification(obj);

    const auto& data = NOTIFICATIONS::CONFIG_STORE::mockData;
    TEST_ASSERT_TRUE(data.telegramEnabled);
    TEST_ASSERT_FALSE(data.webhookEnabled);
}

void test_load_notification_applies_explicit_empty_strings() {
    JsonDocument doc;
    doc[CONFIG::Keys::kBotToken] = "";
    doc[CONFIG::Keys::kChatId] = "";
    doc[CONFIG::Keys::kPushoverUser] = "";

    JsonObject obj = doc.as<JsonObject>();
    CONFIG::JSON::loadNotification(obj);

    const auto& data = NOTIFICATIONS::CONFIG_STORE::mockData;
    TEST_ASSERT_EQUAL_STRING("", data.botToken);
    TEST_ASSERT_EQUAL_STRING("", data.chatId);
    TEST_ASSERT_EQUAL_STRING("", data.pushoverUserKey);
    TEST_ASSERT_EQUAL_STRING("old-token", data.pushoverApiToken);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_load_notification_preserves_missing_fields);
    RUN_TEST(test_load_notification_legacy_mode_replaces_both_channel_flags);
    RUN_TEST(test_load_notification_applies_explicit_empty_strings);
    return UNITY_END();
}
