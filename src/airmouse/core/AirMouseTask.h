#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>

#include "AirMouseController.h"
#include "../../system/watchdog/TaskWatchdog.h"

namespace AIRMOUSE {

/**
 * @brief FreeRTOS task wrapper for Air Mouse processing loop.
 *
 * Responsibilities:
 * - Owns task creation and graceful shutdown.
 * - Places stack in PSRAM (large stack) while keeping TCB in internal RAM.
 * - Delegates the actual logic to AirMouseController::processLoopStep().
 */
class AirMouseTask {
public:
    AirMouseTask(AirMouseController* controller, AirMouseConfigAdapter* configAdapter);
    ~AirMouseTask();

    bool begin(SYSTEM::TaskWatchdog* watchdog);
    void stop();

    bool isRunning() const { return _taskHandle != nullptr; }

private:
    static void taskLoop(void* param); // FreeRTOS entry point
    void runTask();                    // Instance entry point
    bool reapStoppedTask(TickType_t waitTicks);
    
    bool allocatePsramStack();
    void freePsramStack();
    void cleanupResources();

    // External dependencies (not owned)
    AirMouseController* _controller;
    AirMouseConfigAdapter* _configAdapter;
    SYSTEM::TaskWatchdog* _watchdog = nullptr;

    // Task resources
    TaskHandle_t _taskHandle = nullptr;
    StackType_t* _stackBuffer = nullptr;
    StaticTask_t* _tcb = nullptr;

    // Stop/exit coordination between caller and task thread
    std::atomic<bool> _stopRequested{false};
    std::atomic<bool> _taskExitReady{false};
};

} // namespace AIRMOUSE
