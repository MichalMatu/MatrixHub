#ifdef UNIT_TEST

#include <unity.h>

#include "../../test/stubs/PsychicHttpServer.h"
#include "../../test/stubs/PsychicJson.h"
#include "../../test/stubs/PsychicRequest.h"
#include "../../test/stubs/freertos/task.h"
#include "../../test/stubs/freertos/semphr.h"

#define SVK_TAG "Test"

#define private public
#include "../../src/api/common/TestRequestRateLimiter.h"
#include "../../src/api/notifications/handlers/NotificationTestHandler.h"
#undef private

#include "../../src/config/App.h"
#include "../../src/system/errors/ErrorCodes.h"
#include "../../src/system/logging/Logging.h"
#include "../../src/system/rtc/RtcConfig.h"
#include "../../src/system/watchdog/TaskWatchdog.h"

namespace {

bool gTelegramConfigured = true;
bool gWebhookConfigured = true;
bool gPushoverConfigured = true;

API::TelegramTestSender* telegramSender() {
    return reinterpret_cast<API::TelegramTestSender*>(0x1);
}

API::WebhookTestSender* webhookSender() {
    return reinterpret_cast<API::WebhookTestSender*>(0x1);
}

API::PushoverTestSender* pushoverSender() {
    return reinterpret_cast<API::PushoverTestSender*>(0x1);
}

void resetRateLimiter() {
    API::TESTS::testRequestRateLimiter()._busyUntilMs.store(0, std::memory_order_relaxed);
}

void releaseHandlerSchedulerSemaphores(API::Handlers::NotificationTestHandler& handler) {
    if (handler._testJobScheduler._telegramSemaphore) {
        xSemaphoreGive(handler._testJobScheduler._telegramSemaphore);
    }
    if (handler._testJobScheduler._webhookSemaphore) {
        xSemaphoreGive(handler._testJobScheduler._webhookSemaphore);
    }
    if (handler._testJobScheduler._pushoverSemaphore) {
        xSemaphoreGive(handler._testJobScheduler._pushoverSemaphore);
    }
}

PsychicRequest jsonRequest(const std::string& body) {
    PsychicRequest request;
    request.setContentType("application/json");
    request.setBody(body);
    return request;
}

}  // namespace

namespace LOG {

Settings Logging::_settings{};

void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    (void)level;
    (void)tag;
    (void)fmt;
}

}  // namespace LOG

namespace RTC {

RtcRuntimeStats runtimeStats{};

}  // namespace RTC

namespace SYSTEM {

void TaskWatchdog::begin(uint32_t timeoutSec, bool panicOnTimeout) {
    (void)panicOnTimeout;
    _timeoutSec = timeoutSec;
    _initialized = true;
}

bool TaskWatchdog::registerCurrentTask() {
    return true;
}

bool TaskWatchdog::registerTask(TaskHandle_t taskHandle) {
    (void)taskHandle;
    return true;
}

bool TaskWatchdog::unregisterCurrentTask() {
    return true;
}

bool TaskWatchdog::unregisterTask(TaskHandle_t taskHandle) {
    (void)taskHandle;
    return true;
}

bool TaskWatchdog::reset() {
    return true;
}

}  // namespace SYSTEM

#include "../../src/api/notifications/NotificationTestJobScheduler.cpp"
#include "../../src/api/notifications/handlers/NotificationTestHandler.cpp"

namespace API {

TelegramTestSender::TelegramTestSender(NotificationSettingsService* settingsService,
                                       TELEGRAM::TelegramClient* client,
                                       TELEGRAM::TelegramWorker* worker)
    : _settingsService(settingsService)
    , _client(client)
    , _worker(worker) {}

SendTestResult TelegramTestSender::sendTest(const char* text, size_t textLen) {
    (void)text;
    (void)textLen;
    return {};
}

bool TelegramTestSender::isConfigured() const {
    return gTelegramConfigured;
}

WebhookTestSender::WebhookTestSender(NotificationSettingsService* settingsService,
                                     NOTIFICATIONS::WebhookTransportService* transport)
    : _settingsService(settingsService)
    , _transport(transport) {}

WebhookSendTestResult WebhookTestSender::sendTest(const char* payload, size_t payloadLen) {
    (void)payload;
    (void)payloadLen;
    return {};
}

bool WebhookTestSender::isConfigured() const {
    return gWebhookConfigured;
}

PushoverTestSender::PushoverTestSender(NotificationSettingsService* settingsService,
                                       NOTIFICATIONS::PushoverTransportService* transport)
    : _settingsService(settingsService)
    , _transport(transport) {}

PushoverSendTestResult PushoverTestSender::sendTest(const char* message, size_t messageLen) {
    (void)message;
    (void)messageLen;
    return {};
}

bool PushoverTestSender::isConfigured() const {
    return gPushoverConfigured;
}

}  // namespace API

void setUp(void) {
    TEST_STUBS::FREERTOS::resetTaskCreateStub();
    TEST_STUBS::FREERTOS::resetSemaphoreStubs();
    resetRateLimiter();
    RTC::runtimeStats = {};
    gTelegramConfigured = true;
    gWebhookConfigured = true;
    gPushoverConfigured = true;
}

void tearDown(void) {}

void test_telegram_empty_text_returns_400_with_configured_flag() {
    API::Handlers::NotificationTestHandler handler(telegramSender(), webhookSender(), pushoverSender());
    TEST_ASSERT_TRUE(handler.begin());
    PsychicRequest request = jsonRequest("{\"text\":\"\"}");

    const esp_err_t err = handler.handleTelegramTest(&request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(400, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find(ErrorCodes::Input::EMPTY_TEXT) != std::string::npos);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"configured\":true") != std::string::npos);
}

void test_telegram_too_long_returns_400_and_max_len() {
    API::Handlers::NotificationTestHandler handler(telegramSender(), webhookSender(), pushoverSender());
    TEST_ASSERT_TRUE(handler.begin());

    const std::string oversized(APP::NOTIFICATIONS::TELEGRAM_MAX_TEXT_LEN + 1, 'a');
    PsychicRequest request = jsonRequest(std::string("{\"text\":\"") + oversized + "\"}");

    const esp_err_t err = handler.handleTelegramTest(&request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(400, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find(ErrorCodes::Input::TEXT_TOO_LONG) != std::string::npos);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"max_len\"") != std::string::npos);
}

void test_telegram_busy_returns_429_when_channel_lock_is_held() {
    API::Handlers::NotificationTestHandler handler(telegramSender(), webhookSender(), pushoverSender());
    TEST_ASSERT_TRUE(handler.begin());
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(handler._testJobScheduler._telegramSemaphore, 0));
    PsychicRequest request = jsonRequest("{\"text\":\"ping\"}");

    const esp_err_t err = handler.handleTelegramTest(&request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(429, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find(ErrorCodes::Busy::TELEGRAM_TEST_IN_PROGRESS) != std::string::npos);
    releaseHandlerSchedulerSemaphores(handler);
}

void test_telegram_message_field_fallback_queues_task() {
    API::Handlers::NotificationTestHandler handler(telegramSender(), webhookSender(), pushoverSender());
    TEST_ASSERT_TRUE(handler.begin());
    PsychicRequest request = jsonRequest("{\"message\":\"ping\"}");

    const esp_err_t err = handler.handleTelegramTest(&request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(200, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"status\":\"queued\"") != std::string::npos);
    TEST_ASSERT_EQUAL_STRING("tg_test", TEST_STUBS::FREERTOS::lastTaskName);
    releaseHandlerSchedulerSemaphores(handler);
}

void test_telegram_invalid_json_reports_not_configured_when_sender_is_not_send_ready() {
    gTelegramConfigured = false;
    API::Handlers::NotificationTestHandler handler(telegramSender(), webhookSender(), pushoverSender());
    TEST_ASSERT_TRUE(handler.begin());
    PsychicRequest request = jsonRequest("{");

    const esp_err_t err = handler.handleTelegramTest(&request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(400, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find(ErrorCodes::Input::INVALID_JSON) != std::string::npos);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"configured\":false") != std::string::npos);
}

void test_webhook_empty_content_queues_default_payload() {
    API::Handlers::NotificationTestHandler handler(telegramSender(), webhookSender(), pushoverSender());
    TEST_ASSERT_TRUE(handler.begin());
    PsychicRequest request = jsonRequest("{}");

    const esp_err_t err = handler.handleWebhookTest(&request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(200, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"status\":\"queued\"") != std::string::npos);
    TEST_ASSERT_EQUAL_STRING("wh_test", TEST_STUBS::FREERTOS::lastTaskName);
    releaseHandlerSchedulerSemaphores(handler);
}

void test_webhook_busy_returns_429_with_configured_flag() {
    API::Handlers::NotificationTestHandler handler(telegramSender(), webhookSender(), pushoverSender());
    TEST_ASSERT_TRUE(handler.begin());
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(handler._testJobScheduler._webhookSemaphore, 0));
    PsychicRequest request = jsonRequest("{}");

    const esp_err_t err = handler.handleWebhookTest(&request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(429, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find(ErrorCodes::Busy::RESOURCE_LOCKED) != std::string::npos);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"configured\":true") != std::string::npos);
    releaseHandlerSchedulerSemaphores(handler);
}

void test_pushover_not_configured_returns_400() {
    gPushoverConfigured = false;
    API::Handlers::NotificationTestHandler handler(telegramSender(), webhookSender(), pushoverSender());
    TEST_ASSERT_TRUE(handler.begin());
    PsychicRequest request = jsonRequest("{\"message\":\"hello\"}");

    const esp_err_t err = handler.handlePushoverTest(&request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(400, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find(ErrorCodes::Config::NOT_CONFIGURED) != std::string::npos);
}

void test_pushover_empty_message_uses_default_and_queues() {
    API::Handlers::NotificationTestHandler handler(telegramSender(), webhookSender(), pushoverSender());
    TEST_ASSERT_TRUE(handler.begin());
    PsychicRequest request = jsonRequest("{\"message\":\"\"}");

    const esp_err_t err = handler.handlePushoverTest(&request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(200, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"status\":\"queued\"") != std::string::npos);
    TEST_ASSERT_EQUAL_STRING("po_test", TEST_STUBS::FREERTOS::lastTaskName);
    releaseHandlerSchedulerSemaphores(handler);
}

void test_pushover_rate_limited_returns_retry_after_and_configured_flag() {
    API::Handlers::NotificationTestHandler handler(telegramSender(), webhookSender(), pushoverSender());
    TEST_ASSERT_TRUE(handler.begin());
    TEST_ASSERT_TRUE(API::TESTS::testRequestRateLimiter().tryAcquire());
    PsychicRequest request = jsonRequest("{\"message\":\"hello\"}");

    const esp_err_t err = handler.handlePushoverTest(&request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(429, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find(API::TESTS::kBusyErrorCode) != std::string::npos);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"configured\":true") != std::string::npos);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"retry_after_ms\"") != std::string::npos);
}

void test_missing_telegram_sender_returns_500() {
    API::Handlers::NotificationTestHandler handler(nullptr, webhookSender(), pushoverSender());
    TEST_ASSERT_TRUE(handler.begin());
    PsychicRequest request = jsonRequest("{\"text\":\"hello\"}");

    const esp_err_t err = handler.handleTelegramTest(&request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(500, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find(ErrorCodes::Service::TELEGRAM_SETTINGS_UNAVAILABLE) != std::string::npos);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_telegram_empty_text_returns_400_with_configured_flag);
    RUN_TEST(test_telegram_too_long_returns_400_and_max_len);
    RUN_TEST(test_telegram_busy_returns_429_when_channel_lock_is_held);
    RUN_TEST(test_telegram_message_field_fallback_queues_task);
    RUN_TEST(test_telegram_invalid_json_reports_not_configured_when_sender_is_not_send_ready);
    RUN_TEST(test_webhook_empty_content_queues_default_payload);
    RUN_TEST(test_webhook_busy_returns_429_with_configured_flag);
    RUN_TEST(test_pushover_not_configured_returns_400);
    RUN_TEST(test_pushover_empty_message_uses_default_and_queues);
    RUN_TEST(test_pushover_rate_limited_returns_retry_after_and_configured_flag);
    RUN_TEST(test_missing_telegram_sender_returns_500);
    return UNITY_END();
}

#endif  // UNIT_TEST
