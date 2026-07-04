/**
 * @file NotificationSettingsAdapter.cpp
 * @brief JSON adapter implementation
 */

#include "NotificationSettingsAdapter.h"

#include "../../config/json/ConfigKeys.h"

namespace {

constexpr const char* kIsConfigured = "is_configured";

}

void NotificationSettingsAdapter::read(RTC::NotificationData& settings, JsonObject& root) {
    root[CONFIG::Keys::kTelegramEnabled] = settings.telegramEnabled;
    root[CONFIG::Keys::kWebhookEnabled] = settings.webhookEnabled;

    root[CONFIG::Keys::kBotToken] = settings.botToken;
    root[CONFIG::Keys::kChatId] = settings.chatId;
    root[CONFIG::Keys::kCommandsEnabled] = settings.commandsEnabled;

    root[CONFIG::Keys::kWebhookUrl] = settings.webhookUrl;

    root[CONFIG::Keys::kPushoverEnabled] = settings.pushoverEnabled;
    root[CONFIG::Keys::kPushoverUser] = settings.pushoverUserKey;
    root[CONFIG::Keys::kPushoverToken] = settings.pushoverApiToken;

    // Read-only info
    root[kIsConfigured] = settings.isConfigured();
}

StateUpdateResult NotificationSettingsAdapter::update(JsonObject& root, RTC::NotificationData& settings, std::string_view originId) {
    (void)originId;
    bool changed = false;

    changed |= updateBoolField(root, CONFIG::Keys::kTelegramEnabled, settings.telegramEnabled);
    changed |= updateBoolField(root, CONFIG::Keys::kWebhookEnabled, settings.webhookEnabled);

    // Telegram params
    if (updateStringField(root, CONFIG::Keys::kBotToken, settings.botToken, sizeof(settings.botToken))) {
        // Clear chatId when token changes - force re-discovery
        settings.chatId[0] = '\0';
        changed = true;
    }

    // chat_id is read-only (auto-discovered), ignore any value from frontend

    changed |= updateBoolField(root, CONFIG::Keys::kCommandsEnabled, settings.commandsEnabled);

    // Webhook params
    changed |= updateStringField(root, CONFIG::Keys::kWebhookUrl, settings.webhookUrl, sizeof(settings.webhookUrl));

    // Pushover params
    changed |= updateBoolField(root, CONFIG::Keys::kPushoverEnabled, settings.pushoverEnabled);
    changed |= updateStringField(root, CONFIG::Keys::kPushoverUser, settings.pushoverUserKey, sizeof(settings.pushoverUserKey));
    changed |= updateStringField(root, CONFIG::Keys::kPushoverToken, settings.pushoverApiToken, sizeof(settings.pushoverApiToken));

    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
}

bool NotificationSettingsAdapter::updateBoolField(JsonObject& root, const char* key, bool& dest) {
    JsonVariant value = root[key];
    if (value.isNull()) {
        return false;
    }

    const bool next = value.as<bool>();
    if (dest == next) {
        return false;
    }

    dest = next;
    return true;
}

bool NotificationSettingsAdapter::updateStringField(JsonObject& root, const char* key, char* dest, size_t maxLen) {
    JsonVariant value = root[key];
    if (!value.is<const char*>()) {
        return false;
    }

    return updateString(dest, value.as<const char*>(), maxLen);
}

bool NotificationSettingsAdapter::updateString(char* dest, const char* src, size_t maxLen) {
    if (!dest || !src || maxLen == 0) {
        return false;
    }

    const size_t srcLen = strnlen(src, maxLen);
    if (srcLen < maxLen) {
        if (strncmp(dest, src, maxLen) == 0) {
            return false;
        }
    } else if (strnlen(dest, maxLen) == maxLen - 1 && strncmp(dest, src, maxLen - 1) == 0) {
        return false;
    }

    strlcpy(dest, src, maxLen);
    return true;
}
