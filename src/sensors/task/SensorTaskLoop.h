/**
 * @file SensorTaskLoop.h
 * @brief FreeRTOS task loop for sensor logging
 * 
 * Extracted from SensorLoggingTask.cpp (404 LOC -> modular architecture)
 * Handles continuous sensor polling, alarms, and telemetry updates.
 */

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <cstdint>
#include <functional>
#include "../model/SensorTypes.h"
#include <atomic>

namespace ALARMS { class AlarmService; }
namespace SENSORS { class ISensorService; }

namespace SENSORS {

/**
 * @brief Sensor task loop implementation
 * 
 * Responsibilities:
 * - Continuous SCD4x polling on its own cadence (5 s)
 * - Sensor data propagation to alarms/UI
 * - Periodic binary logging (every LOG_INTERVAL_MS or on demand)
 * - Command processing (force read/log)
 * - Stack usage monitoring
 * 
 * Uses SensorState, SensorCommandQueue, ISensorService,
 * SensorBinaryLogger, and AlarmService.
 */
class SensorTaskLoop {
public:
    SensorTaskLoop() = default;

    /**
     * @brief Main FreeRTOS task loop (entry point trampoline)
     * 
     * Called by xTaskCreate from SensorLoggingTask::begin().
     * Runs until _shouldRun flag is cleared.
     * 
     * @param parameter SensorTaskLoop* instance
     */
    static void taskLoop(void* parameter);
    
    /**
     * @brief Set control flags (task handle, run flag, timers)
     * 
     * Called before starting task to initialize shared state.
     * 
     * @param handle Task handle reference
     * @param shouldRun Run flag reference
     * @param lastReadTime Read timestamp reference
     * @param lastLogTime Log timestamp reference
     */
    void setControlState(TaskHandle_t* handle, std::atomic<bool>* shouldRun,
                         std::atomic<uint32_t>* lastReadTime, std::atomic<uint32_t>* lastLogTime,
                         std::function<void(const SensorSnapshot&, bool)>* updateCallback,
                         SemaphoreHandle_t* updateCallbackMutex,
                         SemaphoreHandle_t* stopAck);

    void setSensorService(ISensorService* sensorService);
    void setAlarmService(ALARMS::AlarmService* alarmService);

private:
    void run();
    void signalStopAck();

    TaskHandle_t* _pTaskHandle = nullptr;
    std::atomic<bool>* _pShouldRun = nullptr;
    std::atomic<uint32_t>* _pLastReadTime = nullptr;
    std::atomic<uint32_t>* _pLastLogTime = nullptr;

    // Service Dependencies
    std::function<void(const SensorSnapshot&, bool)>* _pUpdateCallback = nullptr;
    SemaphoreHandle_t* _pUpdateCallbackMutex = nullptr;
    SemaphoreHandle_t* _pStopAck = nullptr;
    ISensorService* _pSensorService = nullptr;
    ALARMS::AlarmService* _pAlarmService = nullptr;
};

}  // namespace SENSORS
