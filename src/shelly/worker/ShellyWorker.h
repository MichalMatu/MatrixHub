#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <atomic>
#include "../ShellyTypes.h"
#include "../ShellyConfig.h"

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
     * Queue a relay control command.
     * @param id Device ID
     * @param turnOn true = ON, false = OFF
     * @return true if queued successfully
     */
    bool queueCommand(const char* id, bool turnOn);

    /**
     * Check if worker is running.
     */
    bool isRunning() const { return _taskHandle != nullptr; }

private:
    static void taskEntry(void* param);
    void taskLoop();
    void processCommand(const ShellyCommand& cmd);
    void destroyResources();
    bool reclaimFinishedTaskIfNeeded();

    ShellyDeviceManager& _deviceManager;
    ShellyRelayController& _relayController;
    std::atomic<bool>& _running;
    
    QueueHandle_t _commandQueue = nullptr;
    uint8_t* _queueStorage = nullptr;
    StaticQueue_t* _queueBuffer = nullptr;
    TaskHandle_t _taskHandle;
    StackType_t* _taskStack;
    StaticTask_t* _taskBuffer;
    std::atomic<bool> _isTaskFinished {false};
};

} // namespace SHELLY
