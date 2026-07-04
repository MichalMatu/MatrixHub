#ifdef UNIT_TEST

#include <unity.h>

#include "../../test/stubs/PsychicHttpServer.h"
#include "../../test/stubs/PsychicJson.h"
#include "../../test/stubs/PsychicRequest.h"
#include "../../test/stubs/freertos/task.h"
#include "../../test/stubs/freertos/semphr.h"

#include <cstring>

#define SVK_TAG "Test"

#define private public
#include "../../src/api/common/TestRequestRateLimiter.h"
#include "../../src/api/notifications/NotificationTestJobScheduler.h"
#undef private

#include "../../src/config/System.h"
#include "../../src/system/logging/Logging.h"
#include "../../src/system/rtc/RtcConfig.h"
#include "../../src/system/watchdog/TaskWatchdog.h"

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

namespace {

API::SendTestResult gTelegramResult{};
API::WebhookSendTestResult gWebhookResult{};
API::PushoverSendTestResult gPushoverResult{};
uint32_t gTelegramSendCount = 0;
uint32_t gWebhookSendCount = 0;
uint32_t gPushoverSendCount = 0;
size_t gTelegramContentLen = 0;
size_t gWebhookContentLen = 0;
size_t gPushoverContentLen = 0;
char gTelegramContent[96] = {};
char gWebhookContent[128] = {};
char gPushoverContent[96] = {};

void captureContent(char* target, size_t targetSize, const char* content, size_t contentLen) {
    if (!target || targetSize == 0) {
        return;
    }
    const size_t copyLen = (contentLen < targetSize - 1) ? contentLen : targetSize - 1;
    if (copyLen > 0 && content) {
        memcpy(target, content, copyLen);
    }
    target[copyLen] = '\0';
}

void resetSenderStubs() {
    gTelegramResult = {};
    gWebhookResult = {};
    gPushoverResult = {};
    gTelegramSendCount = 0;
    gWebhookSendCount = 0;
    gPushoverSendCount = 0;
    gTelegramContentLen = 0;
    gWebhookContentLen = 0;
    gPushoverContentLen = 0;
    memset(gTelegramContent, 0, sizeof(gTelegramContent));
    memset(gWebhookContent, 0, sizeof(gWebhookContent));
    memset(gPushoverContent, 0, sizeof(gPushoverContent));
}

}  // namespace

namespace API {

TelegramTestSender::TelegramTestSender(NotificationSettingsService* settingsService,
                                       TELEGRAM::TelegramClient* client,
                                       TELEGRAM::TelegramWorker* worker)
    : _settingsService(settingsService)
    , _client(client)
    , _worker(worker) {}

SendTestResult TelegramTestSender::sendTest(const char* text, size_t textLen) {
    gTelegramSendCount++;
    gTelegramContentLen = textLen;
    captureContent(gTelegramContent, sizeof(gTelegramContent), text, textLen);
    return gTelegramResult;
}

bool TelegramTestSender::isConfigured() const {
    return true;
}

WebhookTestSender::WebhookTestSender(NotificationSettingsService* settingsService,
                                     NOTIFICATIONS::WebhookTransportService* transport)
    : _settingsService(settingsService)
    , _transport(transport) {}

WebhookSendTestResult WebhookTestSender::sendTest(const char* payload, size_t payloadLen) {
    gWebhookSendCount++;
    gWebhookContentLen = payloadLen;
    captureContent(gWebhookContent, sizeof(gWebhookContent), payload, payloadLen);
    return gWebhookResult;
}

bool WebhookTestSender::isConfigured() const {
    return true;
}

PushoverTestSender::PushoverTestSender(NotificationSettingsService* settingsService,
                                       NOTIFICATIONS::PushoverTransportService* transport)
    : _settingsService(settingsService)
    , _transport(transport) {}

PushoverSendTestResult PushoverTestSender::sendTest(const char* message, size_t messageLen) {
    gPushoverSendCount++;
    gPushoverContentLen = messageLen;
    captureContent(gPushoverContent, sizeof(gPushoverContent), message, messageLen);
    return gPushoverResult;
}

bool PushoverTestSender::isConfigured() const {
    return true;
}

}  // namespace API

namespace {

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

void releaseSchedulerSemaphores(API::NotificationTestJobScheduler& scheduler) {
    if (scheduler._telegramSemaphore) {
        xSemaphoreGive(scheduler._telegramSemaphore);
    }
    if (scheduler._webhookSemaphore) {
        xSemaphoreGive(scheduler._webhookSemaphore);
    }
    if (scheduler._pushoverSemaphore) {
        xSemaphoreGive(scheduler._pushoverSemaphore);
    }
}

void runLastCreatedTask() {
    TEST_ASSERT_NOT_NULL(TEST_STUBS::FREERTOS::lastTaskFunction);
    TEST_ASSERT_NOT_NULL(TEST_STUBS::FREERTOS::lastTaskParameter);
    const TaskFunction_t taskFunction = TEST_STUBS::FREERTOS::lastTaskFunction;
    void* const taskParameter = TEST_STUBS::FREERTOS::lastTaskParameter;
    taskFunction(taskParameter);
}

}  // namespace

void setUp(void) {
    TEST_STUBS::FREERTOS::resetTaskCreateStub();
    TEST_STUBS::FREERTOS::resetSemaphoreStubs();
    TEST_STUBS::ARDUINO::millisValue = 0;
    resetRateLimiter();
    resetSenderStubs();
    RTC::runtimeStats = {};
}

void tearDown(void) {}

void test_begin_creates_all_channel_semaphores() {
    API::NotificationTestJobScheduler scheduler;

    TEST_ASSERT_TRUE(scheduler.begin());
    TEST_ASSERT_NOT_NULL(scheduler._telegramSemaphore);
    TEST_ASSERT_NOT_NULL(scheduler._webhookSemaphore);
    TEST_ASSERT_NOT_NULL(scheduler._pushoverSemaphore);
}

void test_schedule_telegram_queues_task_with_expected_runtime_params() {
    API::NotificationTestJobScheduler scheduler;
    TEST_ASSERT_TRUE(scheduler.begin());

    const auto result = scheduler.scheduleTelegram("ping", telegramSender());

    TEST_ASSERT_TRUE(result.isQueued());
    TEST_ASSERT_EQUAL_STRING("tg_test", TEST_STUBS::FREERTOS::lastTaskName);
    TEST_ASSERT_EQUAL_UINT32(CONFIG::TASKS::STACK_NOTIFICATION_TEST,
                             TEST_STUBS::FREERTOS::lastTaskStackDepth);
    TEST_ASSERT_EQUAL_UINT32(CONFIG::TASKS::PRIO_NOTIFICATION_TEST,
                             TEST_STUBS::FREERTOS::lastTaskPriority);
    TEST_ASSERT_EQUAL_INT(CONFIG::TASKS::CORE_NOTIFICATION_TEST,
                          TEST_STUBS::FREERTOS::lastTaskCore);
    releaseSchedulerSemaphores(scheduler);
}

void test_schedule_telegram_returns_busy_when_channel_semaphore_is_taken() {
    API::NotificationTestJobScheduler scheduler;
    TEST_ASSERT_TRUE(scheduler.begin());
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(scheduler._telegramSemaphore, 0));

    const auto result = scheduler.scheduleTelegram("ping", telegramSender());

    TEST_ASSERT_EQUAL_INT(static_cast<int>(API::NotificationTestScheduleStatus::Busy),
                          static_cast<int>(result.status));
    releaseSchedulerSemaphores(scheduler);
}

void test_schedule_telegram_returns_rate_limited_when_global_cooldown_is_active() {
    API::NotificationTestJobScheduler scheduler;
    TEST_ASSERT_TRUE(scheduler.begin());
    TEST_ASSERT_TRUE(API::TESTS::testRequestRateLimiter().tryAcquire());

    const auto result = scheduler.scheduleTelegram("ping", telegramSender());

    TEST_ASSERT_EQUAL_INT(static_cast<int>(API::NotificationTestScheduleStatus::RateLimited),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT32(API::TESTS::GLOBAL_COOLDOWN_MS, result.retryAfterMs);
}

void test_schedule_telegram_releases_semaphore_after_task_create_failure() {
    API::NotificationTestJobScheduler scheduler;
    TEST_ASSERT_TRUE(scheduler.begin());

    TEST_STUBS::FREERTOS::taskCreateResult = pdFAIL;
    const auto failed = scheduler.scheduleTelegram("ping", telegramSender());

    TEST_ASSERT_EQUAL_INT(static_cast<int>(API::NotificationTestScheduleStatus::TaskCreateFailed),
                          static_cast<int>(failed.status));

    TEST_STUBS::FREERTOS::taskCreateResult = pdPASS;
    resetRateLimiter();
    const auto queued = scheduler.scheduleTelegram("retry", telegramSender());

    TEST_ASSERT_TRUE(queued.isQueued());
    releaseSchedulerSemaphores(scheduler);
}

void test_schedule_webhook_and_pushover_use_channel_specific_task_names() {
    API::NotificationTestJobScheduler scheduler;
    TEST_ASSERT_TRUE(scheduler.begin());

    const auto webhook = scheduler.scheduleWebhook("{\"ok\":true}", webhookSender());

    TEST_ASSERT_TRUE(webhook.isQueued());
    TEST_ASSERT_EQUAL_STRING("wh_test", TEST_STUBS::FREERTOS::lastTaskName);
    TEST_ASSERT_EQUAL_UINT32(CONFIG::TASKS::STACK_NOTIFICATION_TEST,
                             TEST_STUBS::FREERTOS::lastTaskStackDepth);

    runLastCreatedTask();
    resetRateLimiter();

    const auto pushover = scheduler.schedulePushover("hello", pushoverSender());

    TEST_ASSERT_TRUE(pushover.isQueued());
    TEST_ASSERT_EQUAL_STRING("po_test", TEST_STUBS::FREERTOS::lastTaskName);
    TEST_ASSERT_EQUAL_UINT32(CONFIG::TASKS::STACK_NOTIFICATION_TEST,
                             TEST_STUBS::FREERTOS::lastTaskStackDepth);
    releaseSchedulerSemaphores(scheduler);
}

void test_webhook_background_job_records_success_stats_and_releases_semaphore() {
    API::NotificationTestJobScheduler scheduler;
    TEST_ASSERT_TRUE(scheduler.begin());
    TEST_STUBS::ARDUINO::millisValue = 4242;
    gWebhookResult.success = true;
    gWebhookResult.httpCode = 202;

    const auto result = scheduler.scheduleWebhook("{\"ok\":true}", webhookSender());

    TEST_ASSERT_TRUE(result.isQueued());
    TEST_ASSERT_EQUAL(pdFALSE, xSemaphoreTake(scheduler._webhookSemaphore, 0));

    runLastCreatedTask();

    TEST_ASSERT_EQUAL_UINT32(1, gWebhookSendCount);
    TEST_ASSERT_EQUAL_UINT32(strlen("{\"ok\":true}"), gWebhookContentLen);
    TEST_ASSERT_EQUAL_STRING("{\"ok\":true}", gWebhookContent);
    TEST_ASSERT_EQUAL_UINT32(1, RTC::runtimeStats.webhookSent);
    TEST_ASSERT_EQUAL_UINT32(0, RTC::runtimeStats.webhookFailed);
    TEST_ASSERT_EQUAL_UINT32(4242, RTC::runtimeStats.webhookLastSendMs);
    TEST_ASSERT_EQUAL_INT16(202, RTC::runtimeStats.webhookLastHttpCode);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(scheduler._webhookSemaphore, 0));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreGive(scheduler._webhookSemaphore));
}

void test_webhook_background_job_records_failure_stats_and_releases_semaphore() {
    API::NotificationTestJobScheduler scheduler;
    TEST_ASSERT_TRUE(scheduler.begin());
    TEST_STUBS::ARDUINO::millisValue = 9001;
    gWebhookResult.success = false;
    gWebhookResult.httpCode = 503;
    gWebhookResult.error = "send_failed";

    const auto result = scheduler.scheduleWebhook("{\"ok\":false}", webhookSender());

    TEST_ASSERT_TRUE(result.isQueued());
    TEST_ASSERT_EQUAL(pdFALSE, xSemaphoreTake(scheduler._webhookSemaphore, 0));

    runLastCreatedTask();

    TEST_ASSERT_EQUAL_UINT32(1, gWebhookSendCount);
    TEST_ASSERT_EQUAL_UINT32(strlen("{\"ok\":false}"), gWebhookContentLen);
    TEST_ASSERT_EQUAL_STRING("{\"ok\":false}", gWebhookContent);
    TEST_ASSERT_EQUAL_UINT32(0, RTC::runtimeStats.webhookSent);
    TEST_ASSERT_EQUAL_UINT32(1, RTC::runtimeStats.webhookFailed);
    TEST_ASSERT_EQUAL_UINT32(9001, RTC::runtimeStats.webhookLastSendMs);
    TEST_ASSERT_EQUAL_INT16(503, RTC::runtimeStats.webhookLastHttpCode);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(scheduler._webhookSemaphore, 0));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreGive(scheduler._webhookSemaphore));
}

void test_schedule_methods_return_service_unavailable_without_dependencies() {
    API::NotificationTestJobScheduler scheduler;
    TEST_ASSERT_TRUE(scheduler.begin());

    const auto telegram = scheduler.scheduleTelegram("ping", nullptr);
    const auto webhook = scheduler.scheduleWebhook("{}", nullptr);
    const auto pushover = scheduler.schedulePushover("hello", nullptr);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(API::NotificationTestScheduleStatus::ServiceUnavailable),
                          static_cast<int>(telegram.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(API::NotificationTestScheduleStatus::ServiceUnavailable),
                          static_cast<int>(webhook.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(API::NotificationTestScheduleStatus::ServiceUnavailable),
                          static_cast<int>(pushover.status));
}

void test_destructor_releases_idle_channel_semaphores() {
    {
        API::NotificationTestJobScheduler scheduler;
        TEST_ASSERT_TRUE(scheduler.begin());
        TEST_ASSERT_NOT_NULL(scheduler._telegramSemaphore);
        TEST_ASSERT_NOT_NULL(scheduler._webhookSemaphore);
        TEST_ASSERT_NOT_NULL(scheduler._pushoverSemaphore);
    }

    TEST_ASSERT_TRUE(TEST_STUBS::FREERTOS::binarySemaphores().empty());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_begin_creates_all_channel_semaphores);
    RUN_TEST(test_schedule_telegram_queues_task_with_expected_runtime_params);
    RUN_TEST(test_schedule_telegram_returns_busy_when_channel_semaphore_is_taken);
    RUN_TEST(test_schedule_telegram_returns_rate_limited_when_global_cooldown_is_active);
    RUN_TEST(test_schedule_telegram_releases_semaphore_after_task_create_failure);
    RUN_TEST(test_schedule_webhook_and_pushover_use_channel_specific_task_names);
    RUN_TEST(test_webhook_background_job_records_success_stats_and_releases_semaphore);
    RUN_TEST(test_webhook_background_job_records_failure_stats_and_releases_semaphore);
    RUN_TEST(test_schedule_methods_return_service_unavailable_without_dependencies);
    RUN_TEST(test_destructor_releases_idle_channel_semaphores);
    return UNITY_END();
}

#endif  // UNIT_TEST
