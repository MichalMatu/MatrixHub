#ifdef UNIT_TEST

#include <unity.h>

#include "../../src/notifications/telegram/TelegramNotifier.h"
#include "../../src/notifications/webhook/WebhookNotifier.h"
#include "../../src/notifications/pushover/PushoverNotifier.h"
#include "../../src/alarms/wiring/AlarmNotificationBridge.cpp"

namespace {

bool gTelegramConfigured = true;
bool gTelegramEnabled = true;
bool gTelegramQueued = true;

bool gWebhookConfigured = true;
bool gWebhookEnabled = true;
bool gWebhookQueued = true;

bool gPushoverConfigured = true;
bool gPushoverEnabled = true;
bool gPushoverQueued = true;
size_t gPushoverLastLen = 0;

}  // namespace

namespace NOTIFICATIONS {

TelegramNotifier::TelegramNotifier(
    NotificationSettingsService* settings,
    ::TELEGRAM::MessageQueue* queue)
    : _settings(settings), _queue(queue) {}

TelegramSendResult TelegramNotifier::sendMessage(const char* text, size_t textLen) {
    (void)text;
    (void)textLen;
    return {.queued = gTelegramQueued, .error = gTelegramQueued ? nullptr : "queue_full"};
}

bool TelegramNotifier::isConfigured() const {
    return gTelegramConfigured;
}

bool TelegramNotifier::isEnabled() const {
    return gTelegramEnabled;
}

WebhookNotifier::WebhookNotifier(NotificationSettingsService* settings, WebhookWorker* worker)
    : _settings(settings), _worker(worker) {}

WebhookSendResult WebhookNotifier::sendMessage(const char* payload, size_t len) {
    (void)payload;
    (void)len;
    return {.queued = gWebhookQueued, .error = gWebhookQueued ? nullptr : "queue_full"};
}

WebhookSendResult WebhookNotifier::sendTextMessage(const char* message, size_t len) {
    (void)message;
    (void)len;
    return {.queued = gWebhookQueued, .error = gWebhookQueued ? nullptr : "queue_full"};
}

bool WebhookNotifier::isConfigured() const {
    return gWebhookConfigured;
}

bool WebhookNotifier::isEnabled() const {
    return gWebhookEnabled;
}

PushoverNotifier::PushoverNotifier(NotificationSettingsService* settings, PushoverWorker* worker)
    : _settings(settings), _worker(worker) {}

PushoverSendResult PushoverNotifier::sendMessage(const char* message, size_t len) {
    (void)message;
    gPushoverLastLen = len;
    return {.queued = gPushoverQueued, .error = gPushoverQueued ? nullptr : "queue_full"};
}

bool PushoverNotifier::isConfigured() const {
    return gPushoverConfigured;
}

bool PushoverNotifier::isEnabled() const {
    return gPushoverEnabled;
}

}  // namespace NOTIFICATIONS

void setUp(void) {
    gTelegramConfigured = true;
    gTelegramEnabled = true;
    gTelegramQueued = true;
    gWebhookConfigured = true;
    gWebhookEnabled = true;
    gWebhookQueued = true;
    gPushoverConfigured = true;
    gPushoverEnabled = true;
    gPushoverQueued = true;
    gPushoverLastLen = 0;
}

void tearDown(void) {}

void test_bridge_uses_pushover_notifier_when_channel_ready() {
    NOTIFICATIONS::TelegramNotifier telegram(nullptr, nullptr);
    NOTIFICATIONS::WebhookNotifier webhook(nullptr, nullptr);
    NOTIFICATIONS::PushoverNotifier pushover(nullptr, nullptr);

    const auto backend = ALARMS::AlarmNotificationBridge::build(&telegram, &webhook, &pushover);

    TEST_ASSERT_TRUE(backend.pushoverSend("alarm", 5));
    TEST_ASSERT_EQUAL_UINT32(5, gPushoverLastLen);
}

void test_bridge_skips_pushover_when_channel_not_ready() {
    gPushoverConfigured = false;
    NOTIFICATIONS::TelegramNotifier telegram(nullptr, nullptr);
    NOTIFICATIONS::WebhookNotifier webhook(nullptr, nullptr);
    NOTIFICATIONS::PushoverNotifier pushover(nullptr, nullptr);

    const auto backend = ALARMS::AlarmNotificationBridge::build(&telegram, &webhook, &pushover);

    TEST_ASSERT_FALSE(backend.pushoverSend("alarm", 5));
    TEST_ASSERT_EQUAL_UINT32(0, gPushoverLastLen);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_bridge_uses_pushover_notifier_when_channel_ready);
    RUN_TEST(test_bridge_skips_pushover_when_channel_not_ready);
    return UNITY_END();
}

#endif  // UNIT_TEST
