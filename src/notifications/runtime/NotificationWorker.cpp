#include "NotificationWorker.h"
#include "../telegram/client/TelegramClient.h"
#include "../telegram/runtime/TelegramWorker.h"
#include "../webhook/WebhookWorker.h"
#include "../pushover/PushoverWorker.h"
#include "PushoverTransportService.h"
#include "WebhookTransportService.h"
#include "../../system/logging/Logging.h"
#include "../../system/watchdog/TaskWatchdog.h"
#include "../../config/System.h"
#include "../../system/rtc/RtcConfig.h"

#undef LOG_TAG
#define LOG_TAG "NotifWorker"

#include <esp_heap_caps.h>
#include <atomic>

namespace NOTIFICATIONS {

namespace {

void yieldBetweenNotificationChannels() {
    constexpr uint32_t kDelayMs = CONFIG::NOTIFICATIONS::WORKER::INTER_CHANNEL_YIELD_MS;
    if (kDelayMs == 0) {
        return;
    }

    // taskYIELD() would only help peers at the same priority. We want to give
    // lower-priority HTTP/WS tasks on CORE_PRO a chance to run between heavy
    // TLS/HTTP sends, so use at least one scheduler tick of delay.
    TickType_t delayTicks = pdMS_TO_TICKS(kDelayMs);
    if (delayTicks == 0) {
        delayTicks = 1;
    }
    vTaskDelay(delayTicks);
}

} // namespace

bool NotificationWorker::shouldRun() const {
    return _cancelToken && _cancelToken->load(std::memory_order_acquire);
}

void NotificationWorker::setShouldRun(bool value) {
    if (_cancelToken) {
        _cancelToken->store(value, std::memory_order_release);
    }
}

void NotificationWorker::applyCancelFlag(std::atomic<bool>* flag) {
    // Keep every transport on the same view of "runtime is active". The expected
    // behavior is that begin() wires one shared token everywhere, and stop()
    // either flips that token to false or clears it after the task has stopped.
    if (_telegramClient) {
        _telegramClient->setCancelFlag(flag);
    }
    if (_webhookWorker) {
        _webhookWorker->setCancelFlag(flag);
    }
    if (_pushoverWorker) {
        _pushoverWorker->setCancelFlag(flag);
    }
    if (_webhookTransport) {
        _webhookTransport->setCancelFlag(flag);
    }
    if (_pushoverTransport) {
        _pushoverTransport->setCancelFlag(flag);
    }
}

void NotificationWorker::clearCancelFlags() {
    applyCancelFlag(nullptr);
}

bool NotificationWorker::isTaskDeleted(TaskHandle_t handle) {
    if (!handle) {
        return true;
    }

    const eTaskState state = eTaskGetState(handle);
    return state == eDeleted || state == eInvalid;
}

bool NotificationWorker::reclaimExitedTaskIfNeeded() {
    if (!_stoppingTaskHandle || !isTaskDeleted(_stoppingTaskHandle)) {
        return false;
    }

    _stoppingTaskHandle = nullptr;
    freeTaskStorage();

    return true;
}

bool NotificationWorker::recoverExitedTaskIfNeeded(const char* successMessage) {
    if (!reclaimExitedTaskIfNeeded()) {
        return false;
    }

    if (successMessage) {
        LOGI("%s", successMessage);
    }

    return true;
}

TaskHandle_t NotificationWorker::activeTaskHandle() const {
    return _taskHandle ? _taskHandle : _stoppingTaskHandle;
}

bool NotificationWorker::hasTrackedTask() const {
    return _taskHandle != nullptr || _stoppingTaskHandle != nullptr;
}

bool NotificationWorker::hasLiveTask() const {
    return _taskHandle != nullptr ||
           (_stoppingTaskHandle != nullptr && !isTaskDeleted(_stoppingTaskHandle));
}

bool NotificationWorker::isCurrentTaskWorker() const {
    const TaskHandle_t activeHandle = activeTaskHandle();
    return activeHandle && xTaskGetCurrentTaskHandle() == activeHandle;
}

bool NotificationWorker::isRunning() const {
    return hasLiveTask();
}

bool NotificationWorker::waitForLoopExit() const {
    const uint32_t start = millis();
    while (_taskHandle != nullptr &&
           (millis() - start < TIMEOUT::NOTIF_WORKER_STOP_MS)) {
        vTaskDelay(pdMS_TO_TICKS(TIMEOUT::NOTIF_WORKER_STOP_POLL_MS));
    }

    return _taskHandle == nullptr;
}

bool NotificationWorker::waitForTaskCleanup() const {
    const uint32_t start = millis();
    while (_stoppingTaskHandle != nullptr &&
           !isTaskDeleted(_stoppingTaskHandle) &&
           (millis() - start < TIMEOUT::NOTIF_WORKER_STOP_MS)) {
        vTaskDelay(pdMS_TO_TICKS(TIMEOUT::NOTIF_WORKER_STOP_POLL_MS));
    }

    return _stoppingTaskHandle == nullptr || isTaskDeleted(_stoppingTaskHandle);
}

void NotificationWorker::recordStopTimeout(const char* message, uint32_t elapsedMs) {
    RTC::runtimeStats.notifWorkerForcedDeletes++;
    LOGE("%s after %u ms. count=%u",
         message,
         static_cast<unsigned>(elapsedMs),
         RTC::runtimeStats.notifWorkerForcedDeletes);
}

NotificationWorker::NotificationWorker(
    TELEGRAM::TelegramClient* telegramClient,
    TELEGRAM::TelegramWorker* telegramWorker,
    NOTIFICATIONS::WebhookWorker* webhookWorker,
    NOTIFICATIONS::PushoverWorker* pushoverWorker,
    NOTIFICATIONS::WebhookTransportService* webhookTransport,
    NOTIFICATIONS::PushoverTransportService* pushoverTransport,
    std::atomic<bool>* cancelToken)
    : _telegramClient(telegramClient)
    , _telegramWorker(telegramWorker)
    , _webhookWorker(webhookWorker)
    , _pushoverWorker(pushoverWorker)
    , _webhookTransport(webhookTransport)
    , _pushoverTransport(pushoverTransport)
    , _cancelToken(cancelToken ? cancelToken : &_ownedCancelToken) {
}

NotificationWorker::~NotificationWorker() {
    stop();
}

bool NotificationWorker::allocateTaskStorage() {
    if (_taskStack && _taskBuffer) {
        return true;
    }

    _taskStack = static_cast<StackType_t*>(
        heap_caps_malloc(kStackBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    _taskBuffer = static_cast<StaticTask_t*>(
        heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    if (_taskStack && _taskBuffer) {
        return true;
    }

    freeTaskStorage();
    return false;
}

void NotificationWorker::freeTaskStorage() {
    if (_taskStack) {
        heap_caps_free(_taskStack);
        _taskStack = nullptr;
    }
    if (_taskBuffer) {
        heap_caps_free(_taskBuffer);
        _taskBuffer = nullptr;
    }
}

bool NotificationWorker::processCycle() {
    bool didWork = false;

    if (_webhookWorker && _webhookWorker->processCycle()) {
        didWork = true;
        if (shouldRun()) {
            yieldBetweenNotificationChannels();
        }
    }

    if (!shouldRun()) return didWork;

    if (_pushoverWorker && _pushoverWorker->processCycle()) {
        didWork = true;
        if (shouldRun()) {
            yieldBetweenNotificationChannels();
        }
    }

    if (!shouldRun()) return didWork;

    if (_telegramWorker && _telegramWorker->processCycle()) {
        didWork = true;
    }

    return didWork;
}

void NotificationWorker::sleepAfterCycle(bool didWork) {
    const uint32_t delayMs = didWork
                                 ? CONFIG::NOTIFICATIONS::WORKER::ACTIVE_DELAY_MS
                                 : CONFIG::NOTIFICATIONS::WORKER::IDLE_DELAY_MS;

    // Split sleep into short chunks so stop() is observed quickly.
    constexpr uint32_t kChunkMs = 50;
    uint32_t remaining = delayMs;
    while (remaining > 0 && shouldRun()) {
        const uint32_t slice = (remaining < kChunkMs) ? remaining : kChunkMs;
        vTaskDelay(pdMS_TO_TICKS(slice));
        remaining -= slice;
    }
}

void NotificationWorker::begin() {
    LOGI("Starting unified worker");

    recoverExitedTaskIfNeeded("Recovered task resources after delayed self-delete");
    
    if (hasTrackedTask()) {
        LOGW("Already running or stopping");
        return;
    }

    setShouldRun(true);
    _stoppingTaskHandle = nullptr;

    // Arm cancellation before the task starts so even an early failure path
    // still leaves every shared transport in a consistent runtime-owned state.
    applyCancelFlag(_cancelToken);

    // LWIP (TCP/IP stack) requires task stack to be in Internal RAM (DRAM).
    // Attempts to use PSRAM for network tasks result in assertion failure:
    // "lwip_hook_tcp_isn ... (!esp_ptr_external_ram(esp_cpu_get_sp()))"
    // Therefore, we must allocate this stack in DRAM.
    
    if (allocateTaskStorage()) {
        _taskHandle = xTaskCreateStaticPinnedToCore(
            taskLoop,
            "notif_worker",
            kStackBytes, // Size in bytes
            this,
            kPriority,
            _taskStack,
            _taskBuffer,
            CONFIG::TASKS::CORE_NOTIFICATION
        );
    }

    if (!_taskHandle) {
        LOGE("Failed to create unified task (DRAM alloc failed)");
        setShouldRun(false);
        clearCancelFlags();
        freeTaskStorage();
        return;
    }

    LOGI("Started (stack=%u)", kStackBytes);
}

StopStatus NotificationWorker::stop() {
    if (recoverExitedTaskIfNeeded("Stopped gracefully")) {
        clearCancelFlags();
        return StopStatus::ALREADY_STOPPED;
    }

    if (!hasTrackedTask()) {
        clearCancelFlags();
        return StopStatus::ALREADY_STOPPED;
    }

    if (isCurrentTaskWorker()) {
        LOGW("stop() called from within NotificationWorker task! Requesting graceful exit.");
        setShouldRun(false);
        return StopStatus::STOPPED;
    }

    LOGI("Stopping...");
    setShouldRun(false);

    if (_taskHandle) {
        // Wake the worker out of init/idle delay so stop() is not gated by the
        // current sleep window when no network call is in progress.
        (void)xTaskAbortDelay(_taskHandle);
    }

    // The expected shutdown path is:
    // 1) publish cancel=false,
    // 2) break any sleep,
    // 3) tear down idle transports immediately. If Telegram is inside its
    //    serialized TLS/HTTP call, do not close NetworkClientSecure from this
    //    shutdown task; let the active owner unwind on its bounded timeout.
    // 4) only then wait for the loop to exit.
    //
    // If stop() still times out after this point, the bug is likely below the
    // service layer in HTTPClient/WiFiClientSecure/lwIP rather than in queueing.
    if (_telegramClient) {
        _telegramClient->cancelAndReset();
    }
    if (_webhookTransport) {
        _webhookTransport->cancelAndReset();
    }
    if (_pushoverTransport) {
        _pushoverTransport->cancelAndReset();
    }

    const uint32_t loopExitWaitStart = millis();
    if (!waitForLoopExit()) {
        recordStopTimeout("Worker did not exit within bounded stop window",
                          millis() - loopExitWaitStart);
        return StopStatus::TIMEOUT_LOOP;
    }

    const uint32_t cleanupWaitStart = millis();
    if (!waitForTaskCleanup() || !reclaimExitedTaskIfNeeded()) {
        recordStopTimeout("Worker loop exited but task cleanup still pending",
                          millis() - cleanupWaitStart);
        return StopStatus::TIMEOUT_CLEANUP;
    }

    clearCancelFlags();
    LOGI("Stopped");
    return StopStatus::STOPPED;
}

void NotificationWorker::handOffTaskToSelfDelete() {
    _stoppingTaskHandle = xTaskGetCurrentTaskHandle();
    _taskHandle = nullptr;
}

void NotificationWorker::taskLoop(void* param) {
    auto* self = static_cast<NotificationWorker*>(param);
    if (!self) return;
    
    LOGI("Loop started on core %d", xPortGetCoreID());
    LOG_STACK_BUDGET(CONFIG::TASKS::STACK_BUDGET_NOTIFICATION_WORKER);

    auto& watchdog = SYSTEM::TaskWatchdog::instance();
    // Historical context:
    // this worker used to stay outside TWDT on purpose because a blocking TLS
    // handshake had no inner heartbeat path. That assumption changed on
    // March 20, 2026 when the NetworkClientSecure TLS wait hook started
    // feeding TaskWatchdog from inside the handshake/read loops. Without
    // registering notif_worker first, those internal feeds turned into
    // repeated ESP-IDF "task not found" log spam instead of real supervision.
    const bool watchdogRegistered =
        watchdog.isInitialized() && watchdog.registerCurrentTask();
    if (watchdogRegistered) {
        (void)watchdog.reset();
    }

    // Cancellable stabilization delay (50 ms chunks)
    {
        constexpr uint32_t kChunkMs = 50;
        uint32_t remaining = WORKER::INIT_DELAY_MS;
        while (remaining > 0 && self->shouldRun()) {
            const uint32_t slice = (remaining < kChunkMs) ? remaining : kChunkMs;
            vTaskDelay(pdMS_TO_TICKS(slice));
            remaining -= slice;
            // Keep the registration alive during the startup delay too. This
            // makes TWDT supervision continuous from task start instead of only
            // after the first notification channel does work.
            if (watchdogRegistered) {
                (void)watchdog.reset();
            }
        }
    }

    // The runtime intentionally keeps one serialized notification pipeline to stay
    // within the current DRAM budget. Shutdown now propagates a shared cancel token
    // into every transport and actively resets sockets so stop() is bounded by the
    // configured transport timeouts instead of relying on passive unwind alone.
    //
    // If one channel starts starving the others, start debugging from the fixed
    // order here and then inspect the channel transport timeouts/retry behavior.
    while (self->shouldRun()) {
        const bool didWork = self->processCycle();
        // processCycle() may spend time in synchronous HTTP/TLS paths. The
        // lower layers now have their own in-call feeds, but we still refresh
        // once per outer loop so idle/short cycles are supervised as well.
        if (watchdogRegistered) {
            (void)watchdog.reset();
        }

        // Stack Monitoring
        LOG_STACK_BUDGET_PERIODIC(CONFIG::TASKS::STACK_BUDGET_NOTIFICATION_WORKER);

        self->sleepAfterCycle(didWork);
    }

    LOGW("Task loop exited");
    // Remove the task from TWDT before self-delete so the registry does not
    // retain a stale handle for a task that no longer exists.
    if (watchdogRegistered) {
        (void)watchdog.unregisterCurrentTask();
    }
    self->handOffTaskToSelfDelete();
    vTaskDelete(nullptr);
}

} // namespace NOTIFICATIONS
