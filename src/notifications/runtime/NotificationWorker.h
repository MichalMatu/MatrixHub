#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../../config/System.h"
#include <atomic>

namespace TELEGRAM {
class TelegramClient;
class TelegramWorker;
}

namespace NOTIFICATIONS {
class PushoverTransportService;
class PushoverWorker;
class WebhookTransportService;
class WebhookWorker;

/**
 * @brief Result of stop() indicating how the shutdown completed.
 */
enum class StopStatus {
    ALREADY_STOPPED,   ///< Worker was not running when stop() was called
    STOPPED,           ///< Worker exited cleanly within the timeout
    TIMEOUT_LOOP,      ///< Worker loop did not exit (blocked in network I/O)
    TIMEOUT_CLEANUP,   ///< Loop exited but FreeRTOS task cleanup timed out
};

/**
 * @class NotificationWorker
 * @brief Unified worker task for all notification services (Telegram, Webhook, Pushover)
 * 
 * Replaces separate workers to save DRAM.
 * - Single FreeRTOS task with DRAM stack
 * - Coordinated polling and queue processing
 * - Centralized stack monitoring
 */
class NotificationWorker {
public:
    NotificationWorker(TELEGRAM::TelegramClient* telegramClient,
                       TELEGRAM::TelegramWorker* telegramWorker,
                       NOTIFICATIONS::WebhookWorker* webhookWorker,
                       NOTIFICATIONS::PushoverWorker* pushoverWorker,
                       NOTIFICATIONS::WebhookTransportService* webhookTransport,
                       NOTIFICATIONS::PushoverTransportService* pushoverTransport,
                       std::atomic<bool>* cancelToken);
    ~NotificationWorker();

    void begin();
    StopStatus stop();
    bool isRunning() const;

    /// Expose the cancellation flag so channel workers can thread it into HTTP clients.
    std::atomic<bool>* cancelFlag() { return _cancelToken; }

private:
    bool shouldRun() const;
    void setShouldRun(bool value);
    void applyCancelFlag(std::atomic<bool>* flag);
    void clearCancelFlags();
    static bool isTaskDeleted(TaskHandle_t handle);
    TaskHandle_t activeTaskHandle() const;
    bool hasTrackedTask() const;
    bool hasLiveTask() const;
    bool isCurrentTaskWorker() const;
    bool allocateTaskStorage();
    void freeTaskStorage();
    bool processCycle();
    void sleepAfterCycle(bool didWork);
    bool reclaimExitedTaskIfNeeded();
    bool recoverExitedTaskIfNeeded(const char* successMessage);
    bool waitForLoopExit() const;
    bool waitForTaskCleanup() const;
    static void recordStopTimeout(const char* message, uint32_t elapsedMs);
    void handOffTaskToSelfDelete();
    static void taskLoop(void* param);

    TELEGRAM::TelegramClient* _telegramClient = nullptr;
    TELEGRAM::TelegramWorker* _telegramWorker = nullptr;
    NOTIFICATIONS::WebhookWorker* _webhookWorker = nullptr;
    NOTIFICATIONS::PushoverWorker* _pushoverWorker = nullptr;
    NOTIFICATIONS::WebhookTransportService* _webhookTransport = nullptr;
    NOTIFICATIONS::PushoverTransportService* _pushoverTransport = nullptr;
    
    TaskHandle_t _taskHandle = nullptr;
    TaskHandle_t _stoppingTaskHandle = nullptr;
    StackType_t* _taskStack = nullptr;
    StaticTask_t* _taskBuffer = nullptr;
    // Shared runtime cancellation token owned by ServiceRegistry when available.
    // The worker uses a private fallback only to keep the class safe in tests or
    // ad-hoc construction outside the normal composition root.
    std::atomic<bool>* _cancelToken = nullptr;
    std::atomic<bool> _ownedCancelToken{false};

    // Stack configuration
    static constexpr uint32_t kStackBytes = CONFIG::TASKS::STACK_NOTIFICATION_WORKER; // Bytes (ESP-IDF Xtensa StackType_t = uint8_t)
    static constexpr UBaseType_t kPriority = CONFIG::TASKS::PRIO_NOTIFICATION;
};

} // namespace NOTIFICATIONS
