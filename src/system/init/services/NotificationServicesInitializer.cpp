#include "NotificationServicesInitializer.h"

#include "../../../alarms/AlarmService.h"
#include "../../../alarms/wiring/AlarmNotificationBridge.h"
#include "../../../api/notifications/senders/PushoverTestSender.h"
#include "../../../api/notifications/senders/TelegramTestSender.h"
#include "../../../api/notifications/senders/WebhookTestSender.h"
#include "../../../notifications/settings/NotificationSettingsService.h"
#include "../../../notifications/runtime/NotificationWorker.h"
#include "../../../notifications/runtime/PushoverTransportService.h"
#include "../../../notifications/runtime/WebhookTransportService.h"
#include "../../../notifications/telegram/TelegramNotifier.h"
#include "../../../notifications/pushover/PushoverNotifier.h"
#include "../../../notifications/pushover/PushoverWorker.h"
#include "../../../notifications/webhook/WebhookNotifier.h"
#include "../../../notifications/webhook/WebhookWorker.h"
#include "../../../notifications/telegram/client/TelegramClient.h"
#include "../../../notifications/telegram/queue/MessageQueue.h"
#include "../../../notifications/telegram/runtime/TelegramCommandRuntimeState.h"
#include "../../../notifications/telegram/runtime/TelegramWorker.h"
#include "../../../system/logging/Logging.h"

#include <cstdlib>

#undef LOG_TAG
#define LOG_TAG "NotifInit"

namespace {

void initializeWebhookChannel(const NotificationServicesInitializer::State::WebhookChannel& channel,
                              const NotificationServicesInitializer::Deps& deps) {
    // Transport is created before the worker so ServiceRegistry owns the actual
    // HTTP client and both runtime + API test sender can reuse the same path.
    channel.transport =
        std::make_unique<NOTIFICATIONS::WebhookTransportService>(deps.notifMutex);
    channel.worker = std::make_unique<NOTIFICATIONS::WebhookWorker>(
        deps.notificationSettings,
        channel.transport.get());
    channel.notifier = std::make_unique<NOTIFICATIONS::WebhookNotifier>(deps.notificationSettings, channel.worker.get());
}

void initializePushoverChannel(const NotificationServicesInitializer::State::PushoverChannel& channel,
                               const NotificationServicesInitializer::Deps& deps) {
    // Same ownership model as webhook: one explicit transport service, then a
    // thin worker on top of it, then a notifier that only queues domain work.
    channel.transport =
        std::make_unique<NOTIFICATIONS::PushoverTransportService>(deps.notifMutex);
    channel.worker = std::make_unique<NOTIFICATIONS::PushoverWorker>(
        deps.notificationSettings,
        channel.transport.get());
    channel.notifier = std::make_unique<NOTIFICATIONS::PushoverNotifier>(deps.notificationSettings, channel.worker.get());
}

void initializeTelegramChannel(const NotificationServicesInitializer::State::TelegramChannel& channel,
                               const NotificationServicesInitializer::Deps& deps) {
    channel.client = std::make_unique<TELEGRAM::TelegramClient>();
    channel.queue = std::make_unique<TELEGRAM::MessageQueue>();
    // Command polling still needs a shared scratch buffer, but it should now
    // have an explicit owner/lifecycle instead of hiding behind a file-static.
    // Target behavior: boot creates exactly one runtime scratch object for the
    // Telegram command path, ServiceRegistry owns it, and TelegramWorker only
    // borrows it through DI during normal runtime cycles.
    channel.commandRuntime =
        SYSTEM::MEMORY::makeUniqueInPsram<TELEGRAM::TelegramCommandRuntimeState>();
    if (!channel.commandRuntime) {
        // Fail fast here instead of letting Telegram boot half-wired with a
        // null command runtime that would later drop inbound command handling.
        LOGE("Failed to allocate Telegram command runtime in PSRAM");
        std::abort();
    }
    TELEGRAM::TelegramWorkerServices services;
    services.alarmService = deps.alarmService;
    services.macroService = deps.macroService;
    services.matrixManager = deps.matrixManager;
    services.securityManager = deps.securityManager;
    services.usbTerminalService = deps.usbTerminalService;
    services.shellyService = deps.shellyService;
    services.registry = deps.registry;
    channel.worker = std::make_unique<TELEGRAM::TelegramWorker>(
        channel.client.get(),
        deps.notificationSettings,
        channel.queue.get(),
        // The worker should not create or own command scratch state anymore.
        channel.commandRuntime.get(),
        services);
    channel.notifier = std::make_unique<NOTIFICATIONS::TelegramNotifier>(
        deps.notificationSettings,
        channel.queue.get());
}

void initializeRuntimeWorker(const NotificationServicesInitializer::State& state) {
    // NotificationWorker is now the single place that fans out the shared
    // runtime cancel token into Telegram/Webhook/Pushover transports.
    state.runtimeWorker = std::make_unique<NOTIFICATIONS::NotificationWorker>(
        state.telegram.client.get(),
        state.telegram.worker.get(),
        state.webhook.worker.get(),
        state.pushover.worker.get(),
        state.webhook.transport.get(),
        state.pushover.transport.get(),
        &state.cancelToken);
}

void initializeTestSenders(const NotificationServicesInitializer::State& state,
                           const NotificationServicesInitializer::Deps& deps) {
    state.tests.telegram = std::make_unique<API::TelegramTestSender>(
        deps.notificationSettings,
        state.telegram.client.get(),
        state.telegram.worker.get());
    // Test senders intentionally reuse the same concrete transports as runtime
    // dispatch so diagnostics exercise the real production path.
    state.tests.webhook = std::make_unique<API::WebhookTestSender>(
        deps.notificationSettings,
        state.webhook.transport.get());
    state.tests.pushover = std::make_unique<API::PushoverTestSender>(
        deps.notificationSettings,
        state.pushover.transport.get());
}

void wireAlarmNotifications(const NotificationServicesInitializer::State& state,
                            const NotificationServicesInitializer::Deps& deps) {
    if (!deps.alarmService) {
        return;
    }

    deps.alarmService->setNotificationBackend(
        ALARMS::AlarmNotificationBridge::build(
            state.telegram.notifier.get(),
            state.webhook.notifier.get(),
            state.pushover.notifier.get()));
}

}  // namespace

void NotificationServicesInitializer::initialize(const State& state, const Deps& deps) {
    initializeWebhookChannel(state.webhook, deps);
    initializePushoverChannel(state.pushover, deps);
    initializeTelegramChannel(state.telegram, deps);
    initializeRuntimeWorker(state);
    initializeTestSenders(state, deps);
    wireAlarmNotifications(state, deps);
}
