#include "TelegramRuntimeConfigView.h"

#include "../../settings/NotificationSettingsService.h"

namespace TELEGRAM {

TelegramRuntimeConfigView readTelegramRuntimeConfig(const NotificationSettingsService* settings) {
    TelegramRuntimeConfigView config;
    if (!settings) {
        return config;
    }

    RTC::NotificationData snapshot{};
    config.settingsAvailable = settings->snapshot(snapshot);
    if (!config.settingsAvailable) {
        return config;
    }

    config.enabled = snapshot.telegramEnabled;
    config.configured = snapshot.isTelegramReady();
    config.commandsEnabled = snapshot.commandsEnabled;
    config.hasChatId = snapshot.hasChatId();

    strlcpy(config.botToken, snapshot.botToken, sizeof(config.botToken));
    strlcpy(config.chatId, snapshot.chatId, sizeof(config.chatId));

    return config;
}

}  // namespace TELEGRAM
