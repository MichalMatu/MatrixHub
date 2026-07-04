#include "NotificationsApiService.h"
#include "../../system/power/PowerManager.h"

namespace API {

NotificationsApiService::NotificationsApiService(
    PsychicHttpServer *server, 
    SecurityManager *securityManager, 
    POWER::PowerManager* powerManager,
    TelegramTestSender* telegramTestSender,
    WebhookTestSender* webhookTestSender,
    PushoverTestSender* pushoverTestSender)
    : BaseApiService(server, securityManager, powerManager, "api/notifications")
    , _testHandler(telegramTestSender, webhookTestSender, pushoverTestSender) {}

void NotificationsApiService::begin() {
  _server->on("/api/notifications/telegram/test", HTTP_POST,
              wrapAdmin([this](PsychicRequest *request) {
                return _testHandler.handleTelegramTest(request);
              }));

  _server->on("/api/notifications/webhook/test", HTTP_POST,
              wrapAdmin([this](PsychicRequest *request) {
                return _testHandler.handleWebhookTest(request);
              }));

  _server->on("/api/notifications/pushover/test", HTTP_POST,
              wrapAdmin([this](PsychicRequest *request) {
                return _testHandler.handlePushoverTest(request);
              }));

  (void)_testHandler.begin();
}

void NotificationsApiService::shutdown() {
  _testHandler.shutdown();
}

} // namespace API
