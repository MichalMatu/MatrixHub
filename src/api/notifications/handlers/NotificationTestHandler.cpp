#include "NotificationTestHandler.h"

#include "NotificationTestRequestParser.h"
#include "NotificationTestResponseMapper.h"
#include "../senders/PushoverTestSender.h"
#include "../senders/TelegramTestSender.h"
#include "../senders/WebhookTestSender.h"
#include "../../../system/errors/ErrorCodes.h"

namespace API::Handlers {

NotificationTestHandler::NotificationTestHandler(TelegramTestSender* telegramTestSender,
                                                 WebhookTestSender* webhookTestSender,
                                                 PushoverTestSender* pushoverTestSender)
    : _telegramTestSender(telegramTestSender)
    , _webhookTestSender(webhookTestSender)
    , _pushoverTestSender(pushoverTestSender) {}

bool NotificationTestHandler::begin() {
    return _testJobScheduler.begin();
}

void NotificationTestHandler::shutdown() {
    _testJobScheduler.shutdown();
}

esp_err_t NotificationTestHandler::handleTelegramTest(PsychicRequest* request) {
    const bool configured = _telegramTestSender && _telegramTestSender->isConfigured();

    if (!_telegramTestSender) {
        return Response::error(request,
                               500,
                               ErrorCodes::Service::TELEGRAM_SETTINGS_UNAVAILABLE,
                               [](JsonVariant& root) { root["configured"] = false; });
    }

    SYSTEM::SpiRamJsonDocument doc(LIMITS::API::JSON_DOC::NOTIFICATIONS_TELEGRAM_TEST);
    TelegramTestRequestData parsed = {};
    if (!parseTelegramTestRequest(request, configured, doc, parsed)) {
        return ESP_OK;
    }

    return respondScheduleResult(request,
                                 _testJobScheduler.scheduleTelegram(parsed.text, _telegramTestSender),
                                 {.busyErrorCode = ErrorCodes::Busy::TELEGRAM_TEST_IN_PROGRESS,
                                  .configured = configured,
                                  .includeConfigured = true});
}

esp_err_t NotificationTestHandler::handleWebhookTest(PsychicRequest* request) {
    const bool configured = _webhookTestSender && _webhookTestSender->isConfigured();

    if (!_webhookTestSender) {
        return Response::error(request,
                               500,
                               ErrorCodes::Service::UNAVAILABLE,
                               [](JsonVariant& root) { root["configured"] = false; });
    }

    SYSTEM::SpiRamJsonDocument doc(LIMITS::API::JSON_DOC::NOTIFICATIONS_WEBHOOK_TEST);
    WebhookTestRequestData parsed = {};
    if (!parseWebhookTestRequest(request, configured, doc, parsed)) {
        return ESP_OK;
    }

    return respondScheduleResult(request,
                                 _testJobScheduler.scheduleWebhook(parsed.content, _webhookTestSender),
                                 {.busyErrorCode = ErrorCodes::Busy::RESOURCE_LOCKED,
                                  .configured = configured,
                                  .includeConfigured = true});
}

esp_err_t NotificationTestHandler::handlePushoverTest(PsychicRequest* request) {
    if (!_pushoverTestSender) {
        return Response::error(request, 500, ErrorCodes::Service::UNAVAILABLE);
    }

    if (!_pushoverTestSender->isConfigured()) {
        return Response::error(request, 400, ErrorCodes::Config::NOT_CONFIGURED);
    }

    SYSTEM::SpiRamJsonDocument doc(LIMITS::API::JSON_DOC::NOTIFICATIONS_PUSHOVER_TEST);
    PushoverTestRequestData parsed = {};
    if (!parsePushoverTestRequest(request, doc, parsed)) {
        return ESP_OK;
    }

    return respondScheduleResult(request,
                                 _testJobScheduler.schedulePushover(parsed.message, _pushoverTestSender),
                                 {.busyErrorCode = ErrorCodes::Busy::RESOURCE_LOCKED,
                                  .configured = true,
                                  .includeConfiguredOnRateLimit = true});
}

}  // namespace API::Handlers
