#pragma once

#include <PsychicHttpServer.h>

#include "../NotificationTestJobScheduler.h"

namespace API {

class TelegramTestSender;
class WebhookTestSender;
class PushoverTestSender;

namespace Handlers {

class NotificationTestHandler {
public:
    NotificationTestHandler(TelegramTestSender* telegramTestSender,
                            WebhookTestSender* webhookTestSender,
                            PushoverTestSender* pushoverTestSender);

    bool begin();
    void shutdown();

    esp_err_t handleTelegramTest(PsychicRequest* request);
    esp_err_t handleWebhookTest(PsychicRequest* request);
    esp_err_t handlePushoverTest(PsychicRequest* request);

private:
    TelegramTestSender* _telegramTestSender = nullptr;
    WebhookTestSender* _webhookTestSender = nullptr;
    PushoverTestSender* _pushoverTestSender = nullptr;
    NotificationTestJobScheduler _testJobScheduler;
};

}  // namespace Handlers
}  // namespace API
