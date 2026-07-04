#include "NotificationTestJobScheduler.h"

#include "senders/PushoverTestSender.h"
#include "senders/TelegramTestSender.h"
#include "senders/WebhookTestSender.h"
#include "../../config/App.h"
#include "../../config/System.h"
#include "../../system/errors/ErrorCodes.h"
#include "../../system/logging/Logging.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../system/watchdog/TaskWatchdog.h"
#include "../common/TestRequestRateLimiter.h"

#include <cstring>
#include <memory>
#include <new>

namespace API {

namespace {

void waitForIdleAndDeleteSemaphore(SemaphoreHandle_t& semaphore, const char* label) {
    if (!semaphore) {
        return;
    }

    // These API-triggered notification tests run as detached background tasks,
    // but they still borrow registry/API-owned senders and transports. Tear
    // down waits for the channel semaphore so those helpers cannot be destroyed
    // under an in-flight test request during reboot/service shutdown.
    const TickType_t waitTicks = pdMS_TO_TICKS(TIMEOUT::NOTIF_WORKER_STOP_MS);
    if (xSemaphoreTake(semaphore, waitTicks) != pdTRUE) {
        LOGE("[%s] Timed out waiting for notification test task before API teardown", label);
        return;
    }

    vSemaphoreDelete(semaphore);
    semaphore = nullptr;
}

struct TestJobContext;
using TestExecutor = void (*)(const TestJobContext& job);
// Detached tasks receive a raw pointer from FreeRTOS, but ownership inside this
// module is RAII-based. That keeps every exit path (alloc failure, rate limit,
// task creation failure, normal task completion) on the same cleanup model.
using TestJobPtr = std::unique_ptr<TestJobContext>;

struct TestJobContext {
    // Request payload is copied once and owned by the job until the detached
    // task finishes. If memory corruption or double-free shows up here again,
    // inspect createTestJobContext()/releaseTestJobContext() before the senders.
    std::unique_ptr<char[]> content;
    // Cache payload length at enqueue time so detached workers never need to
    // recompute it from shared memory during execution or failure cleanup.
    size_t contentLength = 0;
    SemaphoreHandle_t semaphore = nullptr;
    TestExecutor executor = nullptr;
    const char* logLabel = "NotifAPI";
    TelegramTestSender* telegramSender = nullptr;
    WebhookTestSender* webhookSender = nullptr;
    PushoverTestSender* pushoverSender = nullptr;
};

class ScopedTaskWatchdogRegistration {
public:
    ScopedTaskWatchdogRegistration() {
        _registered = SYSTEM::TaskWatchdog::instance().registerCurrentTask();
        if (_registered) {
            (void)SYSTEM::TaskWatchdog::instance().reset();
        }
    }

    ~ScopedTaskWatchdogRegistration() {
        if (_registered) {
            (void)SYSTEM::TaskWatchdog::instance().unregisterCurrentTask();
        }
    }

    void heartbeat() const {
        if (_registered) {
            (void)SYSTEM::TaskWatchdog::instance().reset();
        }
    }

private:
    bool _registered = false;
};

bool ensureBinarySemaphore(SemaphoreHandle_t& semaphore, const char* label) {
    if (semaphore) {
        return true;
    }

    semaphore = xSemaphoreCreateBinary();
    if (!semaphore) {
        LOGE("Failed to create %s semaphore", label);
        return false;
    }

    xSemaphoreGive(semaphore);
    return true;
}

TestJobContext* createTestJobContext(const char* content,
                                     SemaphoreHandle_t semaphore,
                                     TestExecutor executor,
                                     const char* logLabel) {
    if (!content) {
        return nullptr;
    }

    // Allocate the context and its payload under one local RAII owner first.
    // Only release ownership after the object is fully initialized so partial
    // failures cannot leak memory. If alloc failures start leaving semaphores
    // taken, debug the queueJob() -> createTestJobContext() path.
    TestJobPtr job(new (std::nothrow) TestJobContext());
    if (!job) {
        return nullptr;
    }

    const size_t contentLength = strlen(content);
    std::unique_ptr<char[]> copiedContent(new (std::nothrow) char[contentLength + 1]);
    if (!copiedContent) {
        return nullptr;
    }

    memcpy(copiedContent.get(), content, contentLength + 1);

    job->semaphore = semaphore;
    job->executor = executor;
    job->logLabel = logLabel ? logLabel : "NotifAPI";
    job->telegramSender = nullptr;
    job->webhookSender = nullptr;
    job->pushoverSender = nullptr;
    job->contentLength = contentLength;
    job->content = std::move(copiedContent);
    return job.release();
}

void releaseTestJobContext(TestJobContext* job) {
    if (!job) {
        return;
    }

    // Rewrap the raw FreeRTOS-owned pointer immediately so every caller gets
    // identical cleanup. The semaphore is released only after the payload and
    // context are gone, which keeps "channel busy" semantics tied to job life.
    TestJobPtr ownedJob(job);
    const SemaphoreHandle_t semaphore = job->semaphore;
    ownedJob.reset();
    if (semaphore) {
        xSemaphoreGive(semaphore);
    }
}

void executeTelegramTest(const TestJobContext& job) {
    if (!job.telegramSender || !job.content) {
        return;
    }

    // Use the cached length captured at enqueue time. If sender behavior ever
    // diverges between channels, compare the copied payload/length here first.
    job.telegramSender->sendTest(job.content.get(), job.contentLength);
}

void executeWebhookTest(const TestJobContext& job) {
    if (!job.webhookSender || !job.content) {
        return;
    }

    const auto result = job.webhookSender->sendTest(job.content.get(), job.contentLength);
    if (result.success) {
        RTC::runtimeStats.webhookSent++;
        RTC::runtimeStats.webhookLastSendMs = millis();
        RTC::runtimeStats.webhookLastHttpCode = static_cast<int16_t>(result.httpCode);
    } else {
        RTC::runtimeStats.webhookFailed++;
        RTC::runtimeStats.webhookLastSendMs = millis();
        RTC::runtimeStats.webhookLastHttpCode = static_cast<int16_t>(result.httpCode);
    }
}

void executePushoverTest(const TestJobContext& job) {
    if (!job.pushoverSender || !job.content) {
        return;
    }
    job.pushoverSender->sendTest(job.content.get(), job.contentLength);
}

void notificationTestTask(void* pvParameters) {
    auto* job = static_cast<TestJobContext*>(pvParameters);
    const char* logLabel = (job && job->logLabel) ? job->logLabel : "NotifAPI";

    {
        ScopedTaskWatchdogRegistration watchdog;
        if (job && job->executor) {
            LOGI("[%s] Background task started", logLabel);
            job->executor(*job);
        }

        watchdog.heartbeat();

        releaseTestJobContext(job);
        LOGI("[%s] Background task finished", logLabel);
    }
    vTaskDelete(nullptr);
}

bool launchTestTask(const char* taskName, uint32_t stackBytes, TestJobContext* job) {
    if (!job) {
        return false;
    }

    // Manual notification tests now use dedicated runtime tuning instead of a
    // generic background profile. If diagnostics become flaky under TLS load,
    // inspect TaskConfig's *_NOTIFICATION_TEST constants before changing logic.
    const BaseType_t res = xTaskCreatePinnedToCore(notificationTestTask,
                                                   taskName,
                                                   stackBytes,
                                                   job,
                                                   CONFIG::TASKS::PRIO_NOTIFICATION_TEST,
                                                   nullptr,
                                                   CONFIG::TASKS::CORE_NOTIFICATION_TEST);
    return res == pdPASS;
}

NotificationTestScheduleResult queueJob(const char* taskName,
                                        uint32_t stackBytes,
                                        SemaphoreHandle_t semaphore,
                                        const char* content,
                                        TestExecutor executor,
                                        const char* logLabel,
                                        TelegramTestSender* telegramSender,
                                        WebhookTestSender* webhookSender,
                                        PushoverTestSender* pushoverSender) {
    if (!semaphore) {
        return {NotificationTestScheduleStatus::ServiceUnavailable, 0};
    }

    if (xSemaphoreTake(semaphore, 0) != pdTRUE) {
        return {NotificationTestScheduleStatus::Busy, 0};
    }

    TestJobContext* job = createTestJobContext(content, semaphore, executor, logLabel);
    if (!job) {
        xSemaphoreGive(semaphore);
        return {NotificationTestScheduleStatus::AllocFailed, 0};
    }

    job->telegramSender = telegramSender;
    job->webhookSender = webhookSender;
    job->pushoverSender = pushoverSender;

    if (!TESTS::testRequestRateLimiter().tryAcquire()) {
        const uint32_t retryAfterMs = TESTS::testRequestRateLimiter().remainingMs();
        // The job already owns the copied payload at this point, so use the
        // same release helper as normal completion to keep failure cleanup and
        // "busy" semaphore semantics identical.
        releaseTestJobContext(job);
        return {NotificationTestScheduleStatus::RateLimited, retryAfterMs};
    }

    if (!launchTestTask(taskName, stackBytes, job)) {
        releaseTestJobContext(job);
        return {NotificationTestScheduleStatus::TaskCreateFailed, 0};
    }

    return {NotificationTestScheduleStatus::Queued, 0};
}

}  // namespace

NotificationTestJobScheduler::~NotificationTestJobScheduler() {
    shutdown();
}

bool NotificationTestJobScheduler::begin() {
    const bool telegramOk = ensureBinarySemaphore(_telegramSemaphore, "Telegram test");
    const bool webhookOk = ensureBinarySemaphore(_webhookSemaphore, "Webhook test");
    const bool pushoverOk = ensureBinarySemaphore(_pushoverSemaphore, "Pushover test");
    return telegramOk && webhookOk && pushoverOk;
}

void NotificationTestJobScheduler::shutdown() {
    // The semaphores double as "channel currently running a detached test task".
    // Waiting on them here keeps teardown honest: ServiceRegistry/API shutdown
    // should not free senders/transports while an already-queued test is still
    // executing in the background.
    waitForIdleAndDeleteSemaphore(_telegramSemaphore, "TelegramAPI");
    waitForIdleAndDeleteSemaphore(_webhookSemaphore, "WebhookAPI");
    waitForIdleAndDeleteSemaphore(_pushoverSemaphore, "PushoverAPI");
}

NotificationTestScheduleResult NotificationTestJobScheduler::scheduleTelegram(const char* text,
                                                                              TelegramTestSender* sender) {
    if (!sender) {
        return {NotificationTestScheduleStatus::ServiceUnavailable, 0};
    }

    return queueJob("tg_test",
                    CONFIG::TASKS::STACK_NOTIFICATION_TEST,
                    _telegramSemaphore,
                    text,
                    executeTelegramTest,
                    "TelegramAPI",
                    sender,
                    nullptr,
                    nullptr);
}

NotificationTestScheduleResult NotificationTestJobScheduler::scheduleWebhook(const char* payload,
                                                                             WebhookTestSender* sender) {
    if (!sender) {
        return {NotificationTestScheduleStatus::ServiceUnavailable, 0};
    }

    return queueJob("wh_test",
                    CONFIG::TASKS::STACK_NOTIFICATION_TEST,
                    _webhookSemaphore,
                    payload,
                    executeWebhookTest,
                    "WebhookAPI",
                    nullptr,
                    sender,
                    nullptr);
}

NotificationTestScheduleResult NotificationTestJobScheduler::schedulePushover(const char* message,
                                                                              PushoverTestSender* sender) {
    if (!sender) {
        return {NotificationTestScheduleStatus::ServiceUnavailable, 0};
    }

    return queueJob("po_test",
                    CONFIG::TASKS::STACK_NOTIFICATION_TEST,
                    _pushoverSemaphore,
                    message,
                    executePushoverTest,
                    "PushoverAPI",
                    nullptr,
                    nullptr,
                    sender);
}

}  // namespace API
