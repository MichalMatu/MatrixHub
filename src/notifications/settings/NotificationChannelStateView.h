#pragma once

#include <cstddef>

#include "../../system/rtc/types/RtcNotificationTypes.h"

class NotificationSettingsService;

namespace NOTIFICATIONS {

struct TelegramChannelStateView {
    static constexpr size_t kChatIdCapacity = sizeof(RTC::NotificationData{}.chatId);

    bool settingsAvailable = false;
    bool enabled = false;
    bool configured = false;
    bool sendReady = false;
    char chatId[kChatIdCapacity] = {0};
};

struct WebhookChannelStateView {
    static constexpr size_t kUrlCapacity = sizeof(RTC::NotificationData{}.webhookUrl);

    bool settingsAvailable = false;
    bool enabled = false;
    bool configured = false;
    char url[kUrlCapacity] = {0};
};

struct PushoverChannelStateView {
    static constexpr size_t kUserKeyCapacity = sizeof(RTC::NotificationData{}.pushoverUserKey);
    static constexpr size_t kApiTokenCapacity = sizeof(RTC::NotificationData{}.pushoverApiToken);

    bool settingsAvailable = false;
    bool enabled = false;
    bool configured = false;
    char userKey[kUserKeyCapacity] = {0};
    char apiToken[kApiTokenCapacity] = {0};
};

TelegramChannelStateView readTelegramChannelState(const NotificationSettingsService* settings);
WebhookChannelStateView readWebhookChannelState(const NotificationSettingsService* settings);
PushoverChannelStateView readPushoverChannelState(const NotificationSettingsService* settings);

}  // namespace NOTIFICATIONS
