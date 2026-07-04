#include "NotificationSettingsJson.h"
#include "../../config/App.h"
#include "../../notifications/settings/NotificationConfigStore.h"

namespace {

template <size_t N>
void copyIfPresent(JsonObject& obj, const char* key, char (&target)[N]) {
    if (const char* value = obj[key] | static_cast<const char*>(nullptr)) {
        strlcpy(target, value, sizeof(target));
    }
}

}  // namespace

namespace CONFIG {
namespace JSON {

void loadNotification(JsonObject& obj) {
    ::NOTIFICATIONS::CONFIG_STORE::update([&](RTC::NotificationData& notification) {
        const bool hasTelegramEnabled = obj[Keys::kTelegramEnabled].is<bool>();
        const bool hasWebhookEnabled = obj[Keys::kWebhookEnabled].is<bool>();

        if (hasTelegramEnabled) {
            notification.telegramEnabled = obj[Keys::kTelegramEnabled].as<bool>();
        }
        if (hasWebhookEnabled) {
            notification.webhookEnabled = obj[Keys::kWebhookEnabled].as<bool>();
        }

        if (!hasTelegramEnabled && !hasWebhookEnabled && obj[Keys::kMode].is<const char*>()) {
            String modeStr = obj[Keys::kMode].as<String>();
            bool telegramEnabled = false;
            bool webhookEnabled = false;

            if (modeStr == "telegram") {
                telegramEnabled = true;
            } else if (modeStr == "webhook") {
                webhookEnabled = true;
            } else if (modeStr == "both") {
                telegramEnabled = true;
                webhookEnabled = true;
            }

            notification.telegramEnabled = telegramEnabled;
            notification.webhookEnabled = webhookEnabled;
        }

        copyIfPresent(obj, Keys::kBotToken, notification.botToken);
        copyIfPresent(obj, Keys::kChatId, notification.chatId);

        if (obj[Keys::kCommandsEnabled].is<bool>()) {
            notification.commandsEnabled = obj[Keys::kCommandsEnabled].as<bool>();
        }

        copyIfPresent(obj, Keys::kWebhookUrl, notification.webhookUrl);

        if (obj[Keys::kPushoverEnabled].is<bool>()) {
            notification.pushoverEnabled = obj[Keys::kPushoverEnabled].as<bool>();
        }
        copyIfPresent(obj, Keys::kPushoverUser, notification.pushoverUserKey);
        copyIfPresent(obj, Keys::kPushoverToken, notification.pushoverApiToken);
    });
}

void saveNotification(JsonObject& obj) {
    RTC::NotificationData n = ::NOTIFICATIONS::CONFIG_STORE::copy();
    
    obj[Keys::kTelegramEnabled] = n.telegramEnabled;
    obj[Keys::kWebhookEnabled] = n.webhookEnabled;
    
    // Use String() to force copy into JsonDocument (avoid zero-copy pointer to stack)
    obj[Keys::kBotToken].set(String(n.botToken));
    obj[Keys::kChatId].set(String(n.chatId));
    obj[Keys::kCommandsEnabled] = n.commandsEnabled;
    obj[Keys::kWebhookUrl].set(String(n.webhookUrl));

    obj[Keys::kPushoverEnabled] = n.pushoverEnabled;
    obj[Keys::kPushoverUser].set(String(n.pushoverUserKey));
    obj[Keys::kPushoverToken].set(String(n.pushoverApiToken));
}

} // namespace JSON
} // namespace CONFIG
