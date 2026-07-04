#pragma once

#include <Arduino.h>

#include "../../../system/rtc/types/RtcNotificationTypes.h"

class NotificationSettingsService;

namespace TELEGRAM {

struct TelegramRuntimeConfigView {
    static constexpr size_t kBotTokenCapacity = sizeof(RTC::NotificationData{}.botToken);
    static constexpr size_t kChatIdCapacity = sizeof(RTC::NotificationData{}.chatId);

    bool settingsAvailable = false;
    bool enabled = false;
    bool configured = false;
    bool commandsEnabled = false;
    bool hasChatId = false;
    char botToken[kBotTokenCapacity] = {0};
    char chatId[kChatIdCapacity] = {0};
};

TelegramRuntimeConfigView readTelegramRuntimeConfig(const NotificationSettingsService* settings);

}  // namespace TELEGRAM
