/**
 * @file TelegramNotifier.cpp
 * @brief Telegram notification sender (queues to worker)
 */

#include "TelegramNotifier.h"
#include "../settings/NotificationChannelStateView.h"
#include "queue/MessageQueue.h"
#include "TelegramSendValidator.h"
#include "../../system/logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "TgNotify"

namespace NOTIFICATIONS {

TelegramNotifier::TelegramNotifier(NotificationSettingsService* settings, ::TELEGRAM::MessageQueue* queue)
    : _settings(settings)
    , _queue(queue) {
}

bool TelegramNotifier::isConfigured() const {
    return readTelegramChannelState(_settings).sendReady;
}

bool TelegramNotifier::isEnabled() const {
    return readTelegramChannelState(_settings).enabled;
}

TelegramSendResult TelegramNotifier::sendMessage(const char* text, size_t textLen) {
    TelegramSendResult result;
    const auto channel = readTelegramChannelState(_settings);

    TELEGRAM::TelegramSendValidationInput input;
    input.settingsAvailable = channel.settingsAvailable;
    input.enabled = channel.enabled;
    input.configured = channel.configured;
    input.chatId = channel.chatId;
    input.textLen = textLen;
    input.missingSettingsError = "settings_not_set";

    const auto validation = TELEGRAM::TelegramSendValidator::validate(input);
    if (!validation.ok) {
        result.error = validation.error;
        return result;
    }

    LOGD("Queuing notification (%zu chars)", textLen);

    if (_queue && _queue->enqueue(validation.chatId, text, textLen)) {
        result.queued = true;
        LOGD_THROTTLED(TASK_MONITOR::INTERVAL_TELEGRAM_DISP_MS, "Queued OK");
    } else {
        result.error = _queue ? "queue_full" : "queue_unavailable";
        LOGW("Queue unavailable/full, message dropped");
    }
    
    return result;
}

}  // namespace NOTIFICATIONS
