/**
 * @file WebhookTestSender.cpp
 * @brief Webhook test sender with synchronous delivery
 *
 * Sends test webhook messages directly via HTTPClient (for HTTP) or 
 * TelegramClient (for HTTPS), bypassing the queue for immediate feedback.
 */

#include "WebhookTestSender.h"
#include "../../../notifications/runtime/WebhookTransportService.h"
#include "../../../notifications/settings/NotificationChannelStateView.h"
#include "../../../notifications/webhook/WebhookSendValidator.h"
#include "../../../system/logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "WebHookTest"

namespace API {

WebhookTestSender::WebhookTestSender(NotificationSettingsService* settingsService,
                                     NOTIFICATIONS::WebhookTransportService* transport)
    : _settingsService(settingsService), _transport(transport) {
    LOGI("WebhookTestSender Initialized");
}

bool WebhookTestSender::isConfigured() const {
    return NOTIFICATIONS::readWebhookChannelState(_settingsService).configured;
}

WebhookSendTestResult WebhookTestSender::sendTest(const char* payload, size_t payloadLen) {
    WebhookSendTestResult result = {};
    const auto channel = NOTIFICATIONS::readWebhookChannelState(_settingsService);

    NOTIFICATIONS::WEBHOOK::WebhookSendValidationInput input;
    input.settingsAvailable = channel.settingsAvailable;
    input.enabled = channel.enabled;
    input.configured = channel.configured;
    input.url = channel.url;
    input.payloadLen = payloadLen;
    input.missingSettingsError = "not_initialized";

    const auto validation = NOTIFICATIONS::WEBHOOK::WebhookSendValidator::validate(input);
    if (!validation.ok) {
        result.error = validation.error;
        return result;
    }
    
    if (!_transport) {
        result.error = "transport_unavailable";
        return result;
    }

    // Manual test sends bypass the runtime cancel token on purpose so this
    // endpoint can still provide a deterministic answer while the background
    // notification worker is stopping or being reconfigured.
    auto dispatchResult = _transport->dispatchWithoutCancel(validation.url, payload);

    result.success = dispatchResult.success;
    result.httpCode = dispatchResult.httpCode;
    result.error = dispatchResult.error;
    
    // Copy response if any
    if (dispatchResult.response[0] != '\0') {
         strncpy(result.response, dispatchResult.response, sizeof(result.response) - 1);
         result.response[sizeof(result.response) - 1] = '\0';
    } else if (result.error) {
         // If we have an error string but no response body, perhaps use the error as response text for API
         strncpy(result.response, result.error, sizeof(result.response) - 1);
         result.response[sizeof(result.response) - 1] = '\0';
    }
    
    if (result.success) {
        LOGI("Test sent successfully (HTTP %d)", result.httpCode);
    } else {
        LOGW("Test failed: %s (HTTP %d)", result.error ? result.error : "unknown", result.httpCode);
    }

    return result;
}

}  // namespace API
