#include "NotificationConfigStore.h"

#include <cstdlib>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../../config/App.h"
#include "../../system/logging/Logging.h"
#include <new>
#include <esp_heap_caps.h>
#include "../../system/memory/SystemAllocator.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../system/rtc/RtcDefaultValues.h"
#include "../../system/utils/ScopeLock.h"

#undef LOG_TAG
#define LOG_TAG "NotifCfg"

namespace NOTIFICATIONS {
namespace CONFIG_STORE {
namespace {

RTC::NotificationData* s_store = nullptr;
bool s_loggedFallback = false;

TickType_t configLockTimeoutTicks() {
    return xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED
               ? 0
               : pdMS_TO_TICKS(100);
}

RTC::NotificationData makeFactoryDefaults() {
    RTC::NotificationData cfg{};
    cfg.telegramEnabled = RTC::Defaults::Notification::TelegramEnabled;
    cfg.webhookEnabled = RTC::Defaults::Notification::WebhookEnabled;
    cfg.commandsEnabled = RTC::Defaults::Notification::CommandsEnabled;
    cfg.pushoverEnabled = RTC::Defaults::Notification::PushoverEnabled;
    strlcpy(cfg.botToken, RTC::Defaults::Notification::BotToken, sizeof(cfg.botToken));
    strlcpy(cfg.chatId, RTC::Defaults::Notification::ChatId, sizeof(cfg.chatId));
    strlcpy(cfg.webhookUrl, RTC::Defaults::Notification::WebhookUrl, sizeof(cfg.webhookUrl));
    return cfg;
}

RTC::NotificationSummaryData makeSummary(const RTC::NotificationData& cfg) {
    RTC::NotificationSummaryData summary{};
    summary.telegramEnabled = cfg.telegramEnabled;
    summary.webhookEnabled = cfg.webhookEnabled;
    summary.pushoverEnabled = cfg.pushoverEnabled;
    summary.commandsEnabled = cfg.commandsEnabled;
    summary.hasBotToken = cfg.botToken[0] != '\0';
    summary.hasChatId = cfg.chatId[0] != '\0';
    summary.hasWebhookUrl = cfg.webhookUrl[0] != '\0';
    summary.hasPushoverCreds = cfg.pushoverUserKey[0] != '\0' && cfg.pushoverApiToken[0] != '\0';
    return summary;
}

void syncRtcSummaryLocked(const RTC::NotificationData& cfg) {
    const RTC::NotificationSummaryData summary = makeSummary(cfg);
    RTC::updateConfigLocked([&](RTC::ConfigStore& store) {
        store.notification = summary;
    });
}

RTC::NotificationData& requireStore() {
    if (!s_store) {
        RTC::NotificationData* psramStore = SYSTEM::MEMORY::allocInPsram<RTC::NotificationData>();
        if (psramStore) {
            *psramStore = makeFactoryDefaults();
            s_store = psramStore;
        } else {
            // Keep the DRAM fallback lazy. A permanent .bss copy of this store
            // would defeat the PSRAM-first memory strategy for notification
            // settings, especially because the URLs/tokens make this type large.
            void* mem = heap_caps_malloc(sizeof(RTC::NotificationData), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (mem) {
                s_store = new(mem) RTC::NotificationData();
                *s_store = makeFactoryDefaults();
                if (!s_loggedFallback) {
                    LOGW("Notification config store PSRAM allocation failed; using internal heap fallback");
                    s_loggedFallback = true;
                }
            } else {
                LOGE("Notification config store allocation failed entirely (PSRAM and Internal)");
                std::abort();
            }
        }
    }
    return *s_store;
}

}  // namespace

RTC::NotificationData copy() {
    RTC::NotificationData snapshot{};
    withConfig([&](const RTC::NotificationData& cfg) {
        snapshot = cfg;
    });
    return snapshot;
}

void withConfig(const std::function<void(const RTC::NotificationData&)>& reader) {
    RTC::NotificationData& cfg = requireStore();
    SemaphoreHandle_t lock = RTC::getLock();
    if (!lock) {
        LOGW("withConfig: RTC lock not initialized, returning unlocked notification config");
        reader(cfg);
        return;
    }

    SYSTEM::ScopeLock guard(lock, configLockTimeoutTicks());
    if (!guard.isLocked()) {
        LOGW("withConfig: notification config lock timeout");
        return;
    }

    reader(cfg);
}

bool update(const std::function<void(RTC::NotificationData&)>& updater) {
    RTC::NotificationData& cfg = requireStore();
    SemaphoreHandle_t lock = RTC::getLock();
    if (!lock) {
        LOGE("update: RTC lock not initialized, skipping notification config update");
        return false;
    }

    SYSTEM::ScopeLock guard(lock, configLockTimeoutTicks());
    if (!guard.isLocked()) {
        LOGW("update: notification config lock timeout");
        return false;
    }

    updater(cfg);
    syncRtcSummaryLocked(cfg);
    return true;
}

}  // namespace CONFIG_STORE
}  // namespace NOTIFICATIONS
