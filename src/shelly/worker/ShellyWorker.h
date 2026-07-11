#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <atomic>
#include "../ShellyTypes.h"
#include "../ShellyConfig.h"
#include "ShellyDesiredStateLedger.h"
#include "ShellyWorkerLifecycleState.h"

namespace SHELLY {

class ShellyDeviceManager;
class ShellyRelayController;

/**
 * Background worker task for Shelly command processing and polling.
 * Runs in a dedicated FreeRTOS task.
 */
class ShellyWorker {
public:
    ShellyWorker(ShellyDeviceManager& deviceManager,
                 ShellyRelayController& relayController,
                 std::atomic<bool>& runningFlag);
    ~ShellyWorker();

    /**
     * Start the worker task.
     * @return true if task started successfully
     */
    bool start();

    /**
     * Stop the worker task gracefully.
     */
    bool stop();

    /**
     * Store the latest desired relay state and wake the worker if possible.
     * A saturated wake queue does not discard the stored state.
     */
    bool queueCommand(const char* id,
                      bool turnOn,
                      uint64_t peerRevision,
                      TickType_t mutexTimeout = portMAX_DELAY);

    /** Retain the latest desired state without executing it while disabled. */
    bool parkCommand(const char* id,
                     bool turnOn,
                     uint64_t peerRevision,
                     TickType_t mutexTimeout = portMAX_DELAY);

    /** Move retained intent to a changed network peer/configuration. */
    void rebindDevice(const char* id, uint64_t peerRevision);

    /** Keep the last desired value dormant until this device is enabled. */
    void parkDevice(const char* id, uint64_t peerRevision);

    /** Remove pending state when a configured device is disabled or removed. */
    void forgetDevice(const char* id);

    /**
     * Check if worker is running.
     */
    bool isRunning() const { return _taskLifecycle.isLive(_taskHandle != nullptr); }
    bool hasTask() const { return _taskHandle != nullptr; }

private:
    static void taskEntry(void* param);
    void taskLoop();
    void processDesiredCommands();
    void destroyResources();
    bool reclaimFinishedTaskIfNeeded();

    ShellyDeviceManager& _deviceManager;
    ShellyRelayController& _relayController;
    std::atomic<bool>& _running;
    
    ShellyDesiredStateLedger _desiredStates;
    StaticSemaphore_t _desiredMutexBuffer{};
    SemaphoreHandle_t _desiredMutex = nullptr;

    QueueHandle_t _wakeQueue = nullptr;
    uint8_t _wakeQueueStorage[kWorkerWakeQueueSize]{};
    StaticQueue_t _wakeQueueBuffer{};
    TaskHandle_t _taskHandle;
    StackType_t* _taskStack;
    StaticTask_t* _taskBuffer;
    ShellyWorkerLifecycleState _taskLifecycle;
};

} // namespace SHELLY
