/**
 * @file TelegramTestSender.cpp
 * @brief Telegram test sender with synchronous delivery
 * 
 * Sends test messages directly via TelegramClient (bypassing queue)
 * to provide immediate feedback with HTTP code and API response.
 */

#include "TelegramTestSender.h"
#include "../../../notifications/settings/NotificationChannelStateView.h"
#include "../../../notifications/telegram/client/TelegramClient.h"
#include "../../../notifications/telegram/runtime/TelegramRuntimeConfigView.h"
#include "../../../notifications/telegram/runtime/TelegramWorker.h"
#include "../../../notifications/telegram/TelegramSendValidator.h"
#include "../../../system/logging/Logging.h"

#include <cstring>

#undef LOG_TAG
#define LOG_TAG "TelegramTest"

namespace API {

TelegramTestSender::TelegramTestSender(NotificationSettingsService* settingsService,
                                       TELEGRAM::TelegramClient* client,
                                       TELEGRAM::TelegramWorker* worker)
    : _settingsService(settingsService), _client(client), _worker(worker) {
    LOGI("TelegramTestSender Initialized");
}

bool TelegramTestSender::isConfigured() const {
    return NOTIFICATIONS::readTelegramChannelState(_settingsService).sendReady;
}

SendTestResult TelegramTestSender::sendTest(const char* text, size_t textLen) {
    SendTestResult result = {};
    const auto config = TELEGRAM::readTelegramRuntimeConfig(_settingsService);

    NOTIFICATIONS::TELEGRAM::TelegramSendValidationInput input;
    input.settingsAvailable = config.settingsAvailable;
    input.enabled = config.enabled;
    input.configured = config.configured;
    input.chatId = config.hasChatId ? config.chatId : nullptr;
    input.textLen = textLen;
    input.missingSettingsError = "not_initialized";

    const auto validation = NOTIFICATIONS::TELEGRAM::TelegramSendValidator::validate(input);
    if (!validation.ok) {
        result.error = validation.error;
        return result;
    }

    LOGI("Sending test message synchronously (%zu chars)", textLen);

    // Send directly via TelegramClient (synchronous, bypasses queue).
    // Match the semantics of Webhook/Pushover test senders: manual diagnostics
    // should not inherit NotificationWorker's runtime cancel token.
    if (!_client) {
        result.error = "client_unavailable";
        return result;
    }

    auto sendResult = _client->sendMessageWithoutCancel(
        config,
        validation.chatId, text, textLen);
    
    result.success = sendResult.success;
    result.httpCode = sendResult.httpCode;
    
    if (sendResult.success) {
        LOGI("Test sent successfully (HTTP %d)", sendResult.httpCode);
        snprintf(result.response, sizeof(result.response), 
                 "{\"ok\":true,\"http_code\":%d}", sendResult.httpCode);
        // Count test messages in worker stats + broadcast via WS
        if (_worker) _worker->recordTestSend();
    } else {
        result.error = sendResult.error ? sendResult.error : "send_failed";
        LOGW("Test send failed: %s (HTTP %d)", result.error, sendResult.httpCode);
        snprintf(result.response, sizeof(result.response), 
                 "{\"ok\":false,\"error\":\"%s\",\"http_code\":%d}",
                 result.error, sendResult.httpCode);
    }
    
    return result;
}

}  // namespace API
