#pragma once

#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "HeartbeatSnapshot.h"
#include "HeartbeatTransport.h"

namespace SYSTEM {
namespace HEARTBEAT_DETAIL {

class HeartbeatWorker {
public:
    void begin();
    bool pingNow();
    void checkState();
    void stop();

private:
    static void taskEntry(void* param);
    void taskLoop();
    bool ensureTaskRunning();
    bool reclaimExitedTaskIfNeeded();
    static bool isTaskDeleted(TaskHandle_t handle);
    bool requestPingCycle(const HeartbeatSnapshot* snapshot = nullptr);
    void requestStopAsync();
    void notifyWorker();
    static bool waitForNotification(uint32_t waitMs);
    // Stop paths flip both flags together often enough that keeping the intent
    // reset in one helper is clearer than open-coding the pair everywhere.
    void clearRunIntent();
    // Phase 1 of stop(): wait until the worker loop notices shouldRun=false and
    // drops _taskHandle, which means no more foreground work is running.
    bool waitForTaskLoopExit() const;
    // Phase 2 of stop(): wait until the self-deleting task reaches the final
    // eDeleted/eInvalid state so the caller can reclaim static task buffers.
    bool waitForDeferredCleanup() const;
    void finishTaskExit();

    uint32_t _lastPingMs = 0;
    std::atomic<uint8_t> _currentCycleOffset{0};
    std::atomic<bool> _initialized{false};
    std::atomic<bool> _shouldRun{false};
    std::atomic<bool> _pingCycleActive{false};
    std::atomic<TaskHandle_t> _taskHandle{nullptr};
    std::atomic<TaskHandle_t> _stoppingTaskHandle{nullptr};
    StackType_t* _taskStack = nullptr;
    StaticTask_t* _taskBuffer = nullptr;
    HeartbeatTransport _transport{&_shouldRun};
};

HeartbeatWorker& heartbeatWorker();

} // namespace HEARTBEAT_DETAIL
} // namespace SYSTEM
