#include "HeartbeatWorker.h"

#include "../../../config/System.h"
#include "../../logging/Logging.h"
#include "../../network/NetworkReadiness.h"
#include "../../rtc/RtcConfig.h"
#include "../../watchdog/TaskWatchdog.h"
#include "HeartbeatConfigStore.h"
#include "HeartbeatConfigSanitizer.h"
#include "HeartbeatSnapshot.h"

#include <esp_heap_caps.h>

#undef LOG_TAG
#define LOG_TAG "Heartbt"

namespace SYSTEM {
namespace HEARTBEAT_DETAIL {
namespace {

HeartbeatWorker s_worker;

} // namespace

HeartbeatWorker& heartbeatWorker() {
    return s_worker;
}

void HeartbeatWorker::begin() {
    const bool wasInitialized = _initialized.exchange(true, std::memory_order_acq_rel);
    if (wasInitialized) {
        return;
    }

    checkState();
}

bool HeartbeatWorker::isTaskDeleted(TaskHandle_t handle) {
    if (!handle) {
        return true;
    }

    const eTaskState state = eTaskGetState(handle);
    return state == eDeleted || state == eInvalid;
}

bool HeartbeatWorker::reclaimExitedTaskIfNeeded() {
    const TaskHandle_t stoppingHandle = _stoppingTaskHandle.load(std::memory_order_acquire);
    if (!stoppingHandle || !isTaskDeleted(stoppingHandle)) {
        return false;
    }

    // The worker self-deletes, so the outer lifecycle owns the backing static
    // buffers and reclaims them only after FreeRTOS confirms the task is gone.
    _stoppingTaskHandle.store(nullptr, std::memory_order_release);

    if (_taskStack) {
        heap_caps_free(_taskStack);
        _taskStack = nullptr;
    }
    if (_taskBuffer) {
        heap_caps_free(_taskBuffer);
        _taskBuffer = nullptr;
    }

    return true;
}

void HeartbeatWorker::notifyWorker() {
    const TaskHandle_t taskHandle = _taskHandle.load(std::memory_order_acquire);
    if (taskHandle) {
        xTaskNotifyGive(taskHandle);
    }
}

bool HeartbeatWorker::ensureTaskRunning() {
    if (reclaimExitedTaskIfNeeded()) {
        LOGI("Recovered Heartbeat task resources after delayed self-delete");
    }

    if (_taskHandle.load(std::memory_order_acquire)) {
        return true;
    }

    if (_stoppingTaskHandle.load(std::memory_order_acquire)) {
        LOGW("Heartbeat task is still stopping");
        return false;
    }

    _shouldRun.store(true, std::memory_order_release);
    _pingCycleActive.store(false, std::memory_order_release);
    _currentCycleOffset.store(0, std::memory_order_release);

    if (!_taskStack) {
        _taskStack = static_cast<StackType_t*>(
            heap_caps_malloc(CONFIG::TASKS::STACK_HEARTBEAT, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    if (!_taskBuffer) {
        _taskBuffer = static_cast<StaticTask_t*>(
            heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }

    TaskHandle_t createdHandle = nullptr;
    if (_taskStack && _taskBuffer) {
        createdHandle = xTaskCreateStaticPinnedToCore(
            taskEntry,
            "Heartbeat",
            CONFIG::TASKS::STACK_HEARTBEAT,
            this,
            CONFIG::TASKS::PRIO_HEARTBEAT,
            _taskStack,
            _taskBuffer,
            CONFIG::TASKS::CORE_HEARTBEAT);
    }

    _taskHandle.store(createdHandle, std::memory_order_release);
    if (createdHandle) {
        LOGI("Started Heartbeat worker (stack=%u)", CONFIG::TASKS::STACK_HEARTBEAT);
        return true;
    }

    LOGE("Failed to create Heartbeat task");
    _shouldRun.store(false, std::memory_order_release);

    if (_taskStack) {
        heap_caps_free(_taskStack);
        _taskStack = nullptr;
    }
    if (_taskBuffer) {
        heap_caps_free(_taskBuffer);
        _taskBuffer = nullptr;
    }

    return false;
}

bool HeartbeatWorker::waitForNotification(uint32_t waitMs) {
    if (waitMs == 0) {
        return false;
    }

    // Heartbeat intentionally sleeps in long windows:
    // - startup delay is 60 s,
    // - idle slices can reach 10 s.
    // After registering this worker with TWDT we must not block for the full
    // wait duration, otherwise the watchdog would fire even though the task is
    // healthy and simply waiting for its next slot/notification.
    //
    // Slice the wait into short chunks and feed TWDT between them. That keeps
    // real supervision for active HTTP/TLS work while preserving the worker's
    // original long-delay behavior.
    constexpr uint32_t kWatchdogSliceMs = 1000;
    uint32_t remainingMs = waitMs;
    while (remainingMs > 0) {
        uint32_t sliceMs = remainingMs > kWatchdogSliceMs ? kWatchdogSliceMs : remainingMs;
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(sliceMs)) > 0) {
            return true;
        }

        (void)SYSTEM::TaskWatchdog::instance().reset();

        if (remainingMs <= sliceMs) {
            break;
        }
        remainingMs -= sliceMs;
    }

    return false;
}

void HeartbeatWorker::clearRunIntent() {
    _shouldRun.store(false, std::memory_order_release);
    _pingCycleActive.store(false, std::memory_order_release);
}

bool HeartbeatWorker::waitForTaskLoopExit() const {
    // This wait only covers the "foreground" part of shutdown: the loop has
    // stopped touching transport/runtime state and cleared _taskHandle.
    uint32_t start = millis();
    while (_taskHandle.load(std::memory_order_acquire) != nullptr &&
           (millis() - start < TIMEOUT::HEARTBEAT_STOP_MS)) {
        vTaskDelay(pdMS_TO_TICKS(TIMEOUT::HEARTBEAT_STOP_POLL_MS));
    }

    return _taskHandle.load(std::memory_order_acquire) == nullptr;
}

bool HeartbeatWorker::waitForDeferredCleanup() const {
    // The task self-deletes in finishTaskExit(). That leaves a short window
    // where _taskHandle is already nullptr but FreeRTOS has not yet reported
    // the stopped handle as deleted. Wait for that explicit second phase so the
    // caller can log which side of shutdown stalled.
    uint32_t start = millis();
    while (true) {
        const TaskHandle_t stoppingHandle = _stoppingTaskHandle.load(std::memory_order_acquire);
        if (!stoppingHandle || isTaskDeleted(stoppingHandle)) {
            return true;
        }

        if ((millis() - start) >= TIMEOUT::HEARTBEAT_STOP_MS) {
            return false;
        }

        vTaskDelay(pdMS_TO_TICKS(TIMEOUT::HEARTBEAT_STOP_POLL_MS));
    }
}

void HeartbeatWorker::checkState() {
    bool needsRunning = false;
    const bool updated = SYSTEM::HEARTBEAT_CONFIG::update([&](RTC::HeartbeatData& heartbeat) {
        const HeartbeatSanitizeResult result = sanitizeConfig(heartbeat);
        needsRunning = result.hasActiveSlots;
    });

    if (!updated) {
        LOGW("Heartbeat checkState skipped because config lock timed out");
        return;
    }

    if (!needsRunning) {
        requestStopAsync();
        return;
    }

    if (!ensureTaskRunning()) {
        return;
    }

    notifyWorker();
}

bool HeartbeatWorker::requestPingCycle(const HeartbeatSnapshot* snapshot) {
    HeartbeatSnapshot localSnapshot{};
    const HeartbeatSnapshot* resolvedSnapshot = snapshot;
    if (!resolvedSnapshot) {
        if (!loadHeartbeatSnapshot(localSnapshot)) {
            return false;
        }
        resolvedSnapshot = &localSnapshot;
    }

    if (!resolvedSnapshot->hasActiveSlots()) {
        return false;
    }

    const bool alreadyActive = _pingCycleActive.exchange(true, std::memory_order_acq_rel);
    if (!alreadyActive) {
        _currentCycleOffset.store(0, std::memory_order_release);
    }

    notifyWorker();
    return true;
}

bool HeartbeatWorker::pingNow() {
    checkState();

    if (!_taskHandle.load(std::memory_order_acquire) ||
        _stoppingTaskHandle.load(std::memory_order_acquire)) {
        return false;
    }

    if (!NETWORK::isWifiReadyForHttp()) {
        return false;
    }

    return requestPingCycle();
}

void HeartbeatWorker::requestStopAsync() {
    if (reclaimExitedTaskIfNeeded()) {
        _transport.release();
        return;
    }

    // Publish "we are stopping" before touching transport state so the worker
    // loop, ping scheduling and any concurrent pingNow()/checkState() calls all
    // converge on the same intent immediately.
    clearRunIntent();

    if (!_taskHandle.load(std::memory_order_acquire) &&
        !_stoppingTaskHandle.load(std::memory_order_acquire)) {
        _transport.release();
        return;
    }

    // Production hardening:
    // Heartbeat HTTP GETs are synchronous and can sit inside HTTPClient/TLS for
    // up to the slot timeout. Publish shouldRun=false first, then actively
    // tear down the current socket so shutdown/reconfigure paths are not forced
    // to wait for the full HTTP timeout budget before the worker can exit.
    _transport.cancelActiveIo();
    notifyWorker();
}

void HeartbeatWorker::stop() {
    if (reclaimExitedTaskIfNeeded()) {
        _transport.release();
        return;
    }

    if (!_taskHandle.load(std::memory_order_acquire) &&
        !_stoppingTaskHandle.load(std::memory_order_acquire)) {
        _transport.release();
        clearRunIntent();
        return;
    }

    const TaskHandle_t activeHandle = _taskHandle.load(std::memory_order_acquire)
                                          ? _taskHandle.load(std::memory_order_acquire)
                                          : _stoppingTaskHandle.load(std::memory_order_acquire);
    if (activeHandle && xTaskGetCurrentTaskHandle() == activeHandle) {
        clearRunIntent();
        return;
    }

    requestStopAsync();

    // Heartbeat stops in two phases:
    // 1. the task loop acknowledges shouldRun=false and clears _taskHandle,
    // 2. the task self-deletes, which leaves cleanup to the next caller.
    // Keep those waits separate so logs tell us which phase stalled.
    if (!waitForTaskLoopExit()) {
        LOGE("Heartbeat task is still blocked after %u ms; keeping buffers allocated",
             static_cast<unsigned>(TIMEOUT::HEARTBEAT_STOP_MS));
        return;
    }

    if (!waitForDeferredCleanup()) {
        LOGE("Heartbeat task exit acknowledged but cleanup is still pending");
        return;
    }

    if (!reclaimExitedTaskIfNeeded()) {
        LOGE("Heartbeat task exited but deferred cleanup could not reclaim resources");
        return;
    }

    _transport.release();
}

void HeartbeatWorker::taskEntry(void* param) {
    auto* self = static_cast<HeartbeatWorker*>(param);
    if (!self) {
        vTaskDelete(nullptr);
        return;
    }

    self->taskLoop();
}

void HeartbeatWorker::finishTaskExit() {
    _transport.release();
    clearRunIntent();
    _currentCycleOffset.store(0, std::memory_order_release);
    // Unregister before self-delete for the same reason as notif_worker:
    // prevent TWDT from holding a dead task entry after the worker exits.
    (void)SYSTEM::TaskWatchdog::instance().unregisterCurrentTask();
    // From this point on the worker is logically gone for callers, even though
    // FreeRTOS still needs a moment to report the self-deleted task as dead.
    _stoppingTaskHandle.store(xTaskGetCurrentTaskHandle(), std::memory_order_release);
    _taskHandle.store(nullptr, std::memory_order_release);
    vTaskDelete(nullptr);
}

void HeartbeatWorker::taskLoop() {
    LOGI("Loop started on core %d", xPortGetCoreID());
    LOG_STACK_BUDGET(CONFIG::TASKS::STACK_BUDGET_HEARTBEAT);

    auto& watchdog = SYSTEM::TaskWatchdog::instance();
    // Regression chain:
    // - Feb 26/27, 2026: UnifiedHttpClient started feeding TWDT around blocking
    //   HTTP operations.
    // - Mar 21, 2026: Heartbeat was refactored into its own worker task that
    //   uses UnifiedHttpClient.
    // That combination meant Heartbeat could call TaskWatchdog::reset() from a
    // task that had never registered with TWDT. Register here so Heartbeat
    // benefits from the same supervision model as the other long-lived workers.
    if (watchdog.isInitialized() && watchdog.registerCurrentTask()) {
        (void)watchdog.reset();
    }

    const bool wokenDuringStartup = waitForNotification(HEARTBEAT::STARTUP_DELAY_MS);
    if (!_shouldRun.load(std::memory_order_acquire)) {
        finishTaskExit();
        return;
    }

    if (!wokenDuringStartup) {
        requestPingCycle();
    }

    HeartbeatSnapshot cycleSnapshot{};
    bool cycleSnapshotLoaded = false;

    while (_shouldRun.load(std::memory_order_acquire)) {
        if (_pingCycleActive.load(std::memory_order_acquire)) {
            const uint8_t cycleOffset = _currentCycleOffset.load(std::memory_order_acquire);
            if (!cycleSnapshotLoaded || cycleOffset == 0) {
                if (!loadHeartbeatSnapshot(cycleSnapshot)) {
                    waitForNotification(HEARTBEAT::WAIT_MIN_MS);
                    continue;
                }
                cycleSnapshotLoaded = true;
            }

            if (!cycleSnapshot.hasActiveSlots() || cycleOffset >= cycleSnapshot.activeCount) {
                _currentCycleOffset.store(0, std::memory_order_release);
                _lastPingMs = millis();
                _pingCycleActive.store(false, std::memory_order_release);
                cycleSnapshotLoaded = false;
                LOGV_THROTTLED(TASK_MONITOR::STACK_LOG_INTERVAL_MS, "Heartbeat ping cycle complete");
                continue;
            }

            const auto& activeSlot = cycleSnapshot.activeSlots[cycleOffset];
            const RTC::HeartbeatSlot& slot = activeSlot.slot;
            const uint8_t slotIndex = activeSlot.index;

            _currentCycleOffset.store(cycleOffset + 1, std::memory_order_release);

            LOGD_THROTTLED(TASK_MONITOR::STACK_LOG_INTERVAL_MS,
                           "Slot %u [%s]: pinging",
                           slotIndex,
                           slot.name);

            const bool ok = _transport.ping(slot, HEARTBEAT::HTTP_TIMEOUT_MS, HEARTBEAT::HTTP_RETRIES);
            if (ok) {
                LOGD_THROTTLED(TASK_MONITOR::STACK_LOG_INTERVAL_MS,
                               "Slot %u [%s]: OK",
                               slotIndex,
                               slot.name);
                RTC::runtimeStats.heartbeatSlots[slotIndex].successCount++;
            } else {
                LOGW_THROTTLED(TASK_MONITOR::STACK_LOG_INTERVAL_MS,
                               "Slot %u [%s]: failed",
                               slotIndex,
                               slot.name);
                RTC::runtimeStats.heartbeatSlots[slotIndex].failCount++;
            }
            RTC::runtimeStats.heartbeatSlots[slotIndex].lastPingMs = millis();
            // Refresh once after each HTTP ping too. The transport/client
            // layers already feed during blocking operations; this marks the
            // successful return to the outer scheduling loop.
            (void)watchdog.reset();

            if (_shouldRun.load(std::memory_order_acquire)) {
                waitForNotification(HEARTBEAT::SLOT_STAGGER_MS);
            }
            continue;
        }

        const uint32_t now = millis();
        _transport.releaseIfIdle(now, HEARTBEAT::IDLE_CLIENT_RELEASE_MS);

        HeartbeatSnapshot idleSnapshot{};
        if (!loadHeartbeatSnapshot(idleSnapshot)) {
            waitForNotification(HEARTBEAT::WAIT_MIN_MS);
            continue;
        }

        const uint32_t intervalMs = idleSnapshot.intervalMs;
        const uint32_t elapsedMs = now - _lastPingMs;
        uint32_t waitMs = elapsedMs < intervalMs ? (intervalMs - elapsedMs) : 0;
        if (waitMs > HEARTBEAT::WAIT_SLICE_MS) {
            waitMs = HEARTBEAT::WAIT_SLICE_MS;
        }
        if (waitMs < HEARTBEAT::WAIT_MIN_MS) {
            waitMs = HEARTBEAT::WAIT_MIN_MS;
        }

        if (waitForNotification(waitMs)) {
            continue;
        }

        LOG_STACK_BUDGET_PERIODIC(CONFIG::TASKS::STACK_BUDGET_HEARTBEAT);

        if (!idleSnapshot.hasActiveSlots()) {
            LOGI("Heartbeat config no longer has active slots, stopping worker");
            _shouldRun.store(false, std::memory_order_release);
            break;
        }

        if ((millis() - _lastPingMs) >= intervalMs && !requestPingCycle(&idleSnapshot)) {
            _lastPingMs = millis();
        }
    }

    finishTaskExit();
}

} // namespace HEARTBEAT_DETAIL
} // namespace SYSTEM
