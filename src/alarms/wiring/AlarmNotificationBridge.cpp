#include "AlarmNotificationBridge.h"

#include "../../notifications/telegram/TelegramNotifier.h"
#include "../../notifications/pushover/PushoverNotifier.h"
#include "../../notifications/webhook/WebhookNotifier.h"

namespace ALARMS {

AlarmNotificationBackend AlarmNotificationBridge::build(
    NOTIFICATIONS::TelegramNotifier* telegram,
    NOTIFICATIONS::WebhookNotifier* webhook,
    NOTIFICATIONS::PushoverNotifier* pushover) {
    AlarmNotificationBackend backend;

    backend.telegramSend = [telegram](const char* message, size_t len) {
        if (!telegram || !telegram->isConfigured() || !telegram->isEnabled()) {
            return false;
        }

        auto result = telegram->sendMessage(message, len);
        return result.queued;
    };

    backend.webhookSend = [webhook](const char* message, size_t len) {
        if (!webhook || !webhook->isConfigured() || !webhook->isEnabled()) {
            return false;
        }

        auto result = webhook->sendTextMessage(message, len);
        return result.queued;
    };

    backend.pushoverSend = [pushover](const char* message, size_t len) {
        if (!pushover || !pushover->isConfigured() || !pushover->isEnabled()) {
            return false;
        }

        auto result = pushover->sendMessage(message, len);
        return result.queued;
    };

    return backend;
}

}  // namespace ALARMS
