#pragma once

#include "../notifier/AlarmNotifier.h"

namespace NOTIFICATIONS {
class TelegramNotifier;
class WebhookNotifier;
class PushoverNotifier;
}

namespace ALARMS {

class AlarmNotificationBridge {
public:
    static AlarmNotificationBackend build(
        NOTIFICATIONS::TelegramNotifier* telegram,
        NOTIFICATIONS::WebhookNotifier* webhook,
        NOTIFICATIONS::PushoverNotifier* pushover);
};

}  // namespace ALARMS
