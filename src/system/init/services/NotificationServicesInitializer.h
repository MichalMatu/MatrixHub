#pragma once

#include <atomic>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../../memory/SystemAllocator.h"

class NotificationSettingsService;

namespace ALARMS {
class AlarmService;
}

namespace MACROS {
class MacroService;
}

namespace MATRIX_MANAGER {
class MatrixManagerService;
}

namespace USB_TERMINAL {
class UsbTerminalService;
}

namespace SHELLY {
class ShellyService;
}

class ServiceRegistry;

namespace TELEGRAM {
class TelegramClient;
class MessageQueue;
class TelegramWorker;
struct TelegramCommandRuntimeState;
}

namespace NOTIFICATIONS {
class TelegramNotifier;
class WebhookTransportService;
class WebhookWorker;
class WebhookNotifier;
class PushoverTransportService;
class PushoverWorker;
class PushoverNotifier;
class NotificationWorker;
}

namespace API {
class TelegramTestSender;
class WebhookTestSender;
class PushoverTestSender;
}

class SecurityManager;

class NotificationServicesInitializer {
public:
    struct Deps {
        NotificationSettingsService* notificationSettings{nullptr};
        ALARMS::AlarmService* alarmService{nullptr};
        MACROS::MacroService* macroService{nullptr};
        MATRIX_MANAGER::MatrixManagerService* matrixManager{nullptr};
        SecurityManager* securityManager{nullptr};
        USB_TERMINAL::UsbTerminalService* usbTerminalService{nullptr};
        SHELLY::ShellyService* shellyService{nullptr};
        ServiceRegistry* registry{nullptr};
        SemaphoreHandle_t notifMutex{nullptr};
    };

    struct State {
        struct TelegramChannel {
            std::unique_ptr<TELEGRAM::TelegramClient>& client;
            std::unique_ptr<TELEGRAM::MessageQueue>& queue;
            // Second Telegram DI cleanup: command polling scratch/runtime is now
            // owned by the registry and passed explicitly into TelegramWorker
            // instead of living as a hidden file-static inside the worker.
            SYSTEM::MEMORY::PsramUniquePtr<TELEGRAM::TelegramCommandRuntimeState>& commandRuntime;
            std::unique_ptr<TELEGRAM::TelegramWorker>& worker;
            std::unique_ptr<NOTIFICATIONS::TelegramNotifier>& notifier;
        };

        struct WebhookChannel {
            std::unique_ptr<NOTIFICATIONS::WebhookTransportService>& transport;
            std::unique_ptr<NOTIFICATIONS::WebhookWorker>& worker;
            std::unique_ptr<NOTIFICATIONS::WebhookNotifier>& notifier;
        };

        struct PushoverChannel {
            std::unique_ptr<NOTIFICATIONS::PushoverTransportService>& transport;
            std::unique_ptr<NOTIFICATIONS::PushoverWorker>& worker;
            std::unique_ptr<NOTIFICATIONS::PushoverNotifier>& notifier;
        };

        struct TestSenders {
            std::unique_ptr<API::TelegramTestSender>& telegram;
            std::unique_ptr<API::WebhookTestSender>& webhook;
            std::unique_ptr<API::PushoverTestSender>& pushover;
        };

        TelegramChannel telegram;
        WebhookChannel webhook;
        PushoverChannel pushover;
        std::unique_ptr<NOTIFICATIONS::NotificationWorker>& runtimeWorker;
        TestSenders tests;
        std::atomic<bool>& cancelToken;
    };

    static void initialize(const State& state, const Deps& deps);
};
