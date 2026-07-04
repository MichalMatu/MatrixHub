#include "NotificationChannelStateView.h"

#include "NotificationSettingsService.h"

#include <cstring>

namespace NOTIFICATIONS {

TelegramChannelStateView readTelegramChannelState(const NotificationSettingsService* settings) {
    TelegramChannelStateView state;
    if (!settings) {
        return state;
    }

    RTC::NotificationData snapshot{};
    state.settingsAvailable = settings->snapshot(snapshot);
    if (!state.settingsAvailable) {
        return state;
    }

    state.enabled = snapshot.telegramEnabled;
    state.configured = snapshot.isTelegramReady();
    state.sendReady = snapshot.isTelegramReady() && snapshot.hasChatId();
    strlcpy(state.chatId, snapshot.chatId, sizeof(state.chatId));
    return state;
}

WebhookChannelStateView readWebhookChannelState(const NotificationSettingsService* settings) {
    WebhookChannelStateView state;
    if (!settings) {
        return state;
    }

    RTC::NotificationData snapshot{};
    state.settingsAvailable = settings->snapshot(snapshot);
    if (!state.settingsAvailable) {
        return state;
    }

    state.enabled = snapshot.webhookEnabled;
    state.configured = snapshot.isWebhookReady();
    strlcpy(state.url, snapshot.webhookUrl, sizeof(state.url));
    return state;
}

PushoverChannelStateView readPushoverChannelState(const NotificationSettingsService* settings) {
    PushoverChannelStateView state;
    if (!settings) {
        return state;
    }

    RTC::NotificationData snapshot{};
    state.settingsAvailable = settings->snapshot(snapshot);
    if (!state.settingsAvailable) {
        return state;
    }

    state.enabled = snapshot.pushoverEnabled;
    state.configured = snapshot.isPushoverReady();
    strlcpy(state.userKey, snapshot.pushoverUserKey, sizeof(state.userKey));
    strlcpy(state.apiToken, snapshot.pushoverApiToken, sizeof(state.apiToken));
    return state;
}

}  // namespace NOTIFICATIONS
