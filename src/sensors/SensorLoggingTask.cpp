#include "SensorLoggingTask.h"
#include <atomic>

#include "../config/App.h"
#include "../config/System.h"
#include "../system/logging/Logging.h"

#include "runtime/SensorCommandQueue.h"
#include "runtime/SensorState.h"
#include "task/SensorTaskLoop.h"
#include "logging/SensorBinaryLogger.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>

#undef LOG_TAG
#define LOG_TAG "Telemetry"

TaskHandle_t SensorLoggingTask::_taskHandle = nullptr;
bool SensorLoggingTask::_initialized = false;
std::atomic<uint32_t> SensorLoggingTask::_lastReadTime_ms(0);
std::atomic<uint32_t> SensorLoggingTask::_lastLogTime_ms(0);
std::atomic<bool> SensorLoggingTask::_shouldRun(false);
StackType_t* SensorLoggingTask::_taskStack = nullptr;
StaticTask_t* SensorLoggingTask::_taskBuffer = nullptr;
SensorLoggingTask::UpdateCallback SensorLoggingTask::_updateCallback = nullptr;
SemaphoreHandle_t SensorLoggingTask::_callbackMutex = nullptr;
SemaphoreHandle_t SensorLoggingTask::_stopAck = nullptr;

namespace {
    SENSORS::SensorTaskLoop s_taskLoop;

    bool waitForTaskSuspended(TaskHandle_t taskHandle, SemaphoreHandle_t stopAck, TickType_t waitTicks) {
        if (taskHandle == nullptr) {
            return true;
        }

        if (eTaskGetState(taskHandle) == eSuspended) {
            return true;
        }

        if (stopAck != nullptr) {
            if (xSemaphoreTake(stopAck, waitTicks) != pdTRUE &&
                eTaskGetState(taskHandle) != eSuspended) {
                return false;
            }
        } else if (waitTicks > 0) {
            return false;
        }

        const TickType_t pollStep = pdMS_TO_TICKS(10) > 0 ? pdMS_TO_TICKS(10) : 1;
        const TickType_t settleWait = pdMS_TO_TICKS(50);
        TickType_t waited = 0;

        while (eTaskGetState(taskHandle) != eSuspended && waited < settleWait) {
            vTaskDelay(pollStep);
            waited += pollStep;
        }

        return eTaskGetState(taskHandle) == eSuspended;
    }
}

void SensorLoggingTask::setSensorService(SENSORS::ISensorService* sensorService) {
    s_taskLoop.setSensorService(sensorService);
}

void SensorLoggingTask::setAlarmService(ALARMS::AlarmService* alarmService) {
    s_taskLoop.setAlarmService(alarmService);
}

bool SensorLoggingTask::setUpdateCallback(UpdateCallback callback) {
    if (_callbackMutex) {
        if (xSemaphoreTake(_callbackMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            _updateCallback = callback;
            xSemaphoreGive(_callbackMutex);
            return true;
        }
        LOGE("Timeout taking callback mutex");
        return false;
    }
    LOGE("setUpdateCallback called before initialization");
    return false;
}

bool SensorLoggingTask::prepare() {
    return ensureInitialized();
}

void SensorLoggingTask::begin() {
    if (_shouldRun.load(std::memory_order_acquire)) {
        LOGW("Task already running");
        return;
    }
    if (_taskHandle != nullptr) {
        if (!_shouldRun.load(std::memory_order_acquire)) {
            (void)reapStoppedTask(0);
        }
        if (_taskHandle != nullptr) {
            LOGW("Task handle still present - refusing to start");
            return;
        }
    }
    
    if (!ensureInitialized()) {
        LOGE("Init failed");
        return;
    }

    _shouldRun.store(true, std::memory_order_release);
    
    // Pass control state to task loop
    s_taskLoop.setControlState(&_taskHandle, &_shouldRun,
                               &_lastReadTime_ms, &_lastLogTime_ms,
                               &_updateCallback, &_callbackMutex, &_stopAck);

    if (_stopAck) {
        // Clear any stale signal from a previous run
        (void)xSemaphoreTake(_stopAck, 0);
    }
    


    // Stack in DRAM (not PSRAM) — this task does I2C + LittleFS I/O which
    // can conflict with PSRAM cache during flash operations. TCB also in DRAM.
    if (!_taskStack) {
         _taskStack = (StackType_t*)heap_caps_malloc(SENSOR::STACK_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
         _taskBuffer = (StaticTask_t*)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    
    if (_taskStack && _taskBuffer) {
        _taskHandle = xTaskCreateStaticPinnedToCore(
            SENSORS::SensorTaskLoop::taskLoop,
            "SensorLogger",
            SENSOR::STACK_SIZE,
            &s_taskLoop,
            CONFIG::TASKS::PRIO_SENSOR_LOGGING,
            _taskStack,
            _taskBuffer,
            CONFIG::TASKS::CORE_SENSOR_LOGGING
        );
    } else {
        LOGE("Failed to allocate DRAM for SensorStack — task will NOT start");
        if (_taskStack) heap_caps_free(_taskStack);
        if (_taskBuffer) heap_caps_free(_taskBuffer);
        _taskStack = nullptr;
        _taskBuffer = nullptr;
        _shouldRun.store(false, std::memory_order_release);
        return;
    }

    if (_taskHandle == nullptr) {
        LOGE("Failed to create task");
        if (_taskStack) { heap_caps_free(_taskStack); _taskStack = nullptr; }
        if (_taskBuffer) { heap_caps_free(_taskBuffer); _taskBuffer = nullptr; }
        _shouldRun.store(false, std::memory_order_release);
        return;
    }
    
    LOGI("Task started");
}

void SensorLoggingTask::stop() {
    if (!_taskHandle && !_shouldRun.load(std::memory_order_acquire)) {
        destroyTaskResources();
        return;
    }
    
    LOGI("Stopping sensor logging task...");
    if (_taskHandle && xTaskGetCurrentTaskHandle() == _taskHandle) {
        LOGW("stop() called from within SensorLogger task! Requesting graceful exit.");
        _shouldRun.store(false, std::memory_order_release);
        return;
    }
    const bool wasRunning = _shouldRun.exchange(false, std::memory_order_acq_rel);
    if (_taskHandle != nullptr) {
        xTaskNotifyGive(_taskHandle);
        (void)xTaskAbortDelay(_taskHandle);
    }

    const TickType_t waitTicks =
        wasRunning ? pdMS_TO_TICKS(TIMEOUT::TASK_SHUTDOWN_MS) : 0;
    if (!reapStoppedTask(waitTicks)) {
        if (wasRunning) {
            LOGW("Sensor logging task did not stop cleanly - keeping resources allocated");
        }
        return;
    }

    LOGI("Task stopped. Stack Freed safely.");
}

bool SensorLoggingTask::reapStoppedTask(TickType_t waitTicks) {
    if (_taskHandle == nullptr) {
        destroyTaskResources();
        return true;
    }

    if (_shouldRun.load(std::memory_order_acquire)) {
        return false;
    }

    if (!waitForTaskSuspended(_taskHandle, _stopAck, waitTicks)) {
        return false;
    }

    // Delete/free only after the worker reached vTaskSuspend(). This task runs
    // sensor I2C, logging, alarms, and BLE update callbacks, so timeout-based
    // force delete would risk corrupting shared runtime state during shutdown.
    vTaskDelete(_taskHandle);
    destroyTaskResources();
    return true;
}

void SensorLoggingTask::destroyTaskResources() {
    _taskHandle = nullptr;
    _shouldRun.store(false, std::memory_order_release);

    if (_taskStack) {
        heap_caps_free(_taskStack);
        _taskStack = nullptr;
    }
    if (_taskBuffer) {
        heap_caps_free(_taskBuffer);
        _taskBuffer = nullptr;
    }
}

bool SensorLoggingTask::isRunning() {
    return _taskHandle != nullptr && _shouldRun.load(std::memory_order_acquire);
}

bool SensorLoggingTask::ensureInitialized() {
    if (_initialized) {
        return true;
    }

    if (!_callbackMutex) {
        _callbackMutex = xSemaphoreCreateMutex();
        if (!_callbackMutex) {
            LOGE("Failed to create callback mutex");
            return false;
        }
    }
    if (!_stopAck) {
        _stopAck = xSemaphoreCreateBinary();
        if (!_stopAck) {
            LOGE("Failed to create stop ack semaphore");
            return false;
        }
    }

    if (!SENSORS::SensorState::ensureInitialized()) {
        return false;
    }
    if (!SENSORS::SensorCommandQueue::ensureInitialized()) {
        return false;
    }
    // Scd4x init moved to task loop (async) to prevent blocking boot for 1000ms
    // Sensor is now initialized via ISensorService::begin() inside SensorTaskLoop

    _initialized = true;
    return true;
}


void SensorLoggingTask::sendCommand(SensorTaskCommand cmd) {
    SENSORS::SensorCommandQueue::send(cmd);
    if (_taskHandle != nullptr) {
        xTaskNotifyGive(_taskHandle);
    }
}

SensorSnapshot SensorLoggingTask::getSnapshot() {
    return SENSORS::SensorState::getSnapshot();
}

PhaseStatus SensorLoggingTask::getLastReadStatus() {
    return SENSORS::SensorState::getLastReadStatus();
}

PhaseStatus SensorLoggingTask::getLastWriteStatus() {
    return SENSORS::SensorState::getLastWriteStatus();
}

SensorSnapshot SensorLoggingTask::getLastGoodSnapshot() {
    return SENSORS::SensorState::getLastGoodSnapshot();
}

ErrorInfo SensorLoggingTask::getLastErrorInfo() {
    return SENSORS::SensorState::getLastErrorInfo();
}
