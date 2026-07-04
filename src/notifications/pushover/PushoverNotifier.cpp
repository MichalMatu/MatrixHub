#include "PushoverNotifier.h"
#include "PushoverSendValidator.h"
#include "PushoverWorker.h"
#include "../settings/NotificationChannelStateView.h"
#include "../../config/System.h"
#include <system/logging/Logging.h>

#undef LOG_TAG
#define LOG_TAG "Pushover"

namespace NOTIFICATIONS {

PushoverNotifier::PushoverNotifier(NotificationSettingsService* settings, PushoverWorker* worker)
    : _settings(settings), _worker(worker) {}

bool PushoverNotifier::isConfigured() const {
    return readPushoverChannelState(_settings).configured;
}

bool PushoverNotifier::isEnabled() const {
    return readPushoverChannelState(_settings).enabled;
}

PushoverSendResult PushoverNotifier::sendMessage(const char* message, size_t len) {
    PushoverSendResult result;
    const auto channel = readPushoverChannelState(_settings);

    PUSHOVER::PushoverSendValidationInput input;
    input.settingsAvailable = channel.settingsAvailable;
    input.enabled = channel.enabled;
    input.configured = channel.configured;
    input.userKey = channel.userKey;
    input.apiToken = channel.apiToken;

    const auto validation = PUSHOVER::PushoverSendValidator::validate(input);
    if (!validation.ok) {
        result.error = validation.error;
        return result;
    }

    if (!message || len == 0 || len >= CONFIG::NOTIFICATIONS::PUSHOVER::MAX_MESSAGE_LEN) {
        result.error = "invalid_length";
        return result;
    }

    if (_worker && _worker->enqueue(message)) {
        result.queued = true;
    } else {
        result.error = _worker ? "queue_full" : "worker_not_set";
    }

    return result;
}

} // namespace NOTIFICATIONS
