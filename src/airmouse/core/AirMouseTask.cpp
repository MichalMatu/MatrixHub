#include "AirMouseTask.h"

#include <esp_heap_caps.h>
#include "../../system/logging/Logging.h"
#include "../../config/System.h"

// Note: Requires RtcConfigLoader for warm boot check
#include "../../system/rtc/RtcConfigLoader.h"

#undef LOG_TAG
#define LOG_TAG "AirMouseTask"

namespace AIRMOUSE {

namespace {

bool waitForTaskExitReady(const std::atomic<bool>& exitReady, TickType_t waitTicks) {
    if (exitReady.load(std::memory_order_acquire)) {
        return true;
    }

    if (waitTicks == 0) {
        return false;
    }

    const TickType_t pollStep = pdMS_TO_TICKS(10) > 0 ? pdMS_TO_TICKS(10) : 1;
    TickType_t waited = 0;
    while (!exitReady.load(std::memory_order_acquire) && waited < waitTicks) {
        vTaskDelay(pollStep);
        waited += pollStep;
    }

    return exitReady.load(std::memory_order_acquire);
}

bool waitForTaskSuspended(TaskHandle_t taskHandle, TickType_t waitTicks) {
    if (taskHandle == nullptr) {
        return true;
    }

    if (eTaskGetState(taskHandle) == eSuspended) {
        return true;
    }

    const TickType_t pollStep = pdMS_TO_TICKS(10) > 0 ? pdMS_TO_TICKS(10) : 1;
    TickType_t waited = 0;
    while (eTaskGetState(taskHandle) != eSuspended && waited < waitTicks) {
        vTaskDelay(pollStep);
        waited += pollStep;
    }

    return eTaskGetState(taskHandle) == eSuspended;
}

} // namespace

AirMouseTask::AirMouseTask(AirMouseController* controller, AirMouseConfigAdapter* configAdapter)
    : _controller(controller), _configAdapter(configAdapter) {
}

AirMouseTask::~AirMouseTask() {
    stop();
    while (_taskHandle != nullptr) {
        if (reapStoppedTask(pdMS_TO_TICKS(10))) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    cleanupResources();
}

bool AirMouseTask::allocatePsramStack() {
    if (_stackBuffer) return true;
    // Stack goes to PSRAM to protect internal DRAM.
    _stackBuffer = (StackType_t*)heap_caps_malloc(CONFIG::TASKS::STACK_AIR_MOUSE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!_stackBuffer) {
        LOGE("Failed to allocate PSRAM stack");
        return false;
    }
    return true;
}

void AirMouseTask::freePsramStack() {
    if (_stackBuffer) {
        heap_caps_free(_stackBuffer);
        _stackBuffer = nullptr;
    }
}

void AirMouseTask::cleanupResources() {
    freePsramStack();
    if (_tcb) {
        heap_caps_free(_tcb);
        _tcb = nullptr;
    }
}

bool AirMouseTask::begin(SYSTEM::TaskWatchdog* watchdog) {
    if (_taskHandle) {
        if ((_stopRequested.load(std::memory_order_acquire) ||
             _taskExitReady.load(std::memory_order_acquire)) &&
            !reapStoppedTask(0)) {
            LOGW("AirMouse task is still stopping");
            return false;
        }
        if (_taskHandle) {
            LOGW("Already running");
            return true;
        }
    }

    if (!_controller || !_configAdapter) {
        LOGE("Missing controller or configAdapter in Task");
        return false;
    }

    _watchdog = watchdog;
    _taskExitReady.store(false, std::memory_order_release);

    if (!allocatePsramStack()) return false;

    if (!_tcb) {
        // TCB must be in internal memory (FreeRTOS requirement).
        _tcb = (StaticTask_t*)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!_tcb) {
            LOGE("Failed to allocate TCB");
            cleanupResources();
            return false;
        }
    }

    _stopRequested.store(false, std::memory_order_release);

    _taskHandle = xTaskCreateStaticPinnedToCore(
        taskLoop, "AirMouse", CONFIG::TASKS::STACK_AIR_MOUSE,
        this, CONFIG::TASKS::PRIO_AIR_MOUSE, _stackBuffer, _tcb, CONFIG::TASKS::CORE_AIR_MOUSE
    );

    if (!_taskHandle) {
        LOGE("Failed to create task");
        cleanupResources();
        return false;
    }

    LOGI("Started (PSRAM stack)");
    return true;
}

void AirMouseTask::stop() {
    if (!_taskHandle) {
        _taskExitReady.store(false, std::memory_order_release);
        _stopRequested.store(false, std::memory_order_release);
        cleanupResources();
        return;
    }

    if (xTaskGetCurrentTaskHandle() == _taskHandle) {
        LOGW("stop() called from within AirMouse task! Requesting graceful exit.");
        _stopRequested.store(true, std::memory_order_release);
        return;
    }

    LOGI("Stopping...");
    const bool alreadyStopping = _stopRequested.exchange(true, std::memory_order_acq_rel);
    (void)xTaskAbortDelay(_taskHandle);

    const TickType_t waitTicks =
        alreadyStopping ? 0 : pdMS_TO_TICKS(TIMEOUT::TASK_SHUTDOWN_MS);
    if (!reapStoppedTask(waitTicks)) {
        // Do not force-delete this task on timeout. It drives IMU/HID/jiggler
        // runtime state, so we intentionally keep resources until the worker
        // acknowledges exit and suspends itself.
        if (!alreadyStopping) {
            LOGW("AirMouse task did not stop cleanly - keeping resources allocated");
        }
        return;
    }

    LOGI("Stopped (resources freed)");
}

bool AirMouseTask::reapStoppedTask(TickType_t waitTicks) {
    if (_taskHandle == nullptr) {
        _taskExitReady.store(false, std::memory_order_release);
        _stopRequested.store(false, std::memory_order_release);
        cleanupResources();
        return true;
    }

    if (!_stopRequested.load(std::memory_order_acquire)) {
        return false;
    }

    if (!waitForTaskExitReady(_taskExitReady, waitTicks)) {
        return false;
    }

    const TickType_t settleWait = pdMS_TO_TICKS(50);
    if (!waitForTaskSuspended(_taskHandle, settleWait)) {
        return false;
    }

    // Reap only after the task has reported exit and reached vTaskSuspend().
    // This preserves the graceful-shutdown contract and avoids deleting the
    // worker while it may still be inside controller or watchdog code.
    vTaskDelete(_taskHandle);
    _taskHandle = nullptr;
    _taskExitReady.store(false, std::memory_order_release);
    _stopRequested.store(false, std::memory_order_release);
    cleanupResources();
    return true;
}

void AirMouseTask::taskLoop(void* param) {
    AirMouseTask* instance = (AirMouseTask*)param;
    if (instance) {
        instance->runTask();
    }
    vTaskSuspend(nullptr);
}

void AirMouseTask::runTask() {
    LOGI("HWM: %u", uxTaskGetStackHighWaterMark(nullptr));

    if (_watchdog) {
        _watchdog->registerCurrentTask();
    }

    // Allow IMU and filters to stabilize; warm boot shortens this delay.
    const bool warmBootFastPath = RTC::wasWarmBootPathUsed();
    const uint32_t startupDelayMs = warmBootFastPath
        ? CONFIG::TASKS::AIR_MOUSE_STARTUP_DELAY_WARM_BOOT_MS
        : CONFIG::TASKS::AIR_MOUSE_STARTUP_DELAY_COLD_BOOT_MS;
    LOGI("Startup delay: Waiting %lu ms for stabilization (%s boot)...",
         static_cast<unsigned long>(startupDelayMs),
         warmBootFastPath ? "warm" : "cold");
         
    const uint32_t stepMs = 50;
    uint32_t waitedMs = 0;
    while (waitedMs < startupDelayMs) {
        if (_stopRequested.load(std::memory_order_acquire)) {
            if (_watchdog) _watchdog->unregisterCurrentTask();
            _taskExitReady.store(true, std::memory_order_release);
            vTaskSuspend(nullptr);
            return;
        }
        const uint32_t sleepMs = (startupDelayMs - waitedMs < stepMs)
            ? (startupDelayMs - waitedMs)
            : stepMs;
        if (_watchdog) _watchdog->reset();
        vTaskDelay(pdMS_TO_TICKS(sleepMs));
        waitedMs += sleepMs;
    }
    
    // Initial calibration after boot delay.
    _controller->calibrate();

    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t intervalTicks = pdMS_TO_TICKS(CONFIG::TASKS::AIR_MOUSE_INTERVAL_MS);
    const TickType_t jigglerOnlyIntervalTicks = pdMS_TO_TICKS(CONFIG::TASKS::AIR_MOUSE_JIGGLER_ONLY_INTERVAL_MS);
    TickType_t currentIntervalTicks = intervalTicks;
    AirMouseConfigAdapter::RuntimeCfg runtimeCfg{};
    (void)_configAdapter->getRuntimeConfig(runtimeCfg);

    while (!_stopRequested.load(std::memory_order_acquire)) {
        bool enabled = _configAdapter->isMovementEnabled() || _configAdapter->isClickEnabled();
        AirMouseConfigAdapter::RuntimeCfg latestRuntimeCfg{};
        if (_configAdapter->getRuntimeConfig(latestRuntimeCfg)) {
            runtimeCfg = latestRuntimeCfg;
        }

        if (!enabled && runtimeCfg.jigglerMode == RTC::MouseJigglerMode::JIGGLER_OFF) {
            if (_watchdog) _watchdog->reset();
            vTaskDelay(pdMS_TO_TICKS(CONFIG::AIR_MOUSE::LOOPS::DISABLED_SLEEP_MS));
            lastWakeTime = xTaskGetTickCount(); // reset cadence after long sleep
            continue;
        }
        
        uint32_t now = millis();
        
        // Core logic (config updates, jiggler, IMU processing, HID actions).
        _controller->processLoopStep(now);

        LOG_STACK_PERIODIC(CONFIG::TASKS::STACK_AIR_MOUSE);
        if (_watchdog) _watchdog->reset();

        const TickType_t loopIntervalTicks = _configAdapter->needsImu() ? intervalTicks : jigglerOnlyIntervalTicks;
        if (loopIntervalTicks != currentIntervalTicks) {
            currentIntervalTicks = loopIntervalTicks;
            lastWakeTime = xTaskGetTickCount();
        }
        vTaskDelayUntil(&lastWakeTime, currentIntervalTicks);
    }
    
    if (_watchdog) _watchdog->unregisterCurrentTask();
    _taskExitReady.store(true, std::memory_order_release);
    vTaskSuspend(nullptr);
}

} // namespace AIRMOUSE
