#include "MacroEngine.h"

#include "../../config/System.h"
#include "../../system/logging/Logging.h"
#include "../../system/utils/ScopeLock.h"

#ifndef NATIVE_BUILD
#include "../../keyboard/KeyboardService.h"
#endif

#undef LOG_TAG
#define LOG_TAG "MacroEngine"

namespace MACROS {

MacroEngine::MacroEngine() : _taskHandle(nullptr), _stopSignal(false), _stateCallback(nullptr) {
    _mutex = xSemaphoreCreateMutex();
}

MacroEngine::~MacroEngine() {
    stop();
    if (_mutex) vSemaphoreDelete(_mutex);
    
    if (_taskStack) { heap_caps_free(_taskStack); _taskStack = nullptr; }
    if (_taskTcb) { heap_caps_free(_taskTcb); _taskTcb = nullptr; }
}

void MacroEngine::start() {
    // Race Condition Fix: Lock mutex before checking/creating task
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
    if (lock.isLocked()) {
        if (_taskHandle) {
            LOGW("Engine task already running");
            return;
        }

        if (!_taskStack) {
            // CRITICAL: Task stack for static tasks MUST remain in Internal DRAM (MALLOC_CAP_INTERNAL).
            // Moving this to PSRAM/SPIRAM triggers xPortCheckValidTCBMem assertion failure and crash.
            _taskStack = (StackType_t*)heap_caps_malloc(CONFIG::TASKS::STACK_MACRO, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        }
        if (!_taskStack) {
            LOGE("Failed to allocate DRAM stack for MacroTask");
            return;
        }

        if (!_taskTcb) {
            _taskTcb = (StaticTask_t*)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        }
        if (!_taskTcb) {
            LOGE("Failed to allocate TCB for MacroTask");
            heap_caps_free(_taskStack);
            _taskStack = nullptr;
            return;
        }

        TaskHandle_t h = xTaskCreateStaticPinnedToCore(
            taskFunction,    
            "MacroTask",     
            CONFIG::TASKS::STACK_MACRO,
            this,            
            CONFIG::TASKS::PRIO_MACRO,               
            _taskStack,
            _taskTcb,
            CONFIG::TASKS::CORE_MACRO
        );
        _taskHandle = h;
        _stopSignal.store(false, std::memory_order_release);
        lock.unlock();
        
        if (!h) {
            LOGE("Failed to create MacroTask");
            if (_taskStack) { heap_caps_free(_taskStack); _taskStack = nullptr; }
            if (_taskTcb) { heap_caps_free(_taskTcb); _taskTcb = nullptr; }
            return;
        }
        LOGI("MacroEngine started");
    } else {
        LOGE("Could not obtain lock to start MacroEngine");
    }
}

void MacroEngine::stop() {
    TaskHandle_t handle = nullptr;
    {
        SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
        if (lock.isLocked()) {
            handle = _taskHandle;
        } else {
            LOGE("Could not obtain lock to stop MacroEngine");
            return;
        }
    }
    if (!handle) return;
    
    LOGI("Stopping MacroEngine...");
    _stopSignal.store(true, std::memory_order_release);
    xTaskNotifyGive(handle); // Wake up task if waiting

    bool taskSuspended = false;
    // Wait for the worker to finish executeLoop() and suspend itself.
    for (int i = 0; i < 200; i++) {
        if (!isTaskAlive(handle)) {
            break;
        }
#if defined(INCLUDE_eTaskGetState) && (INCLUDE_eTaskGetState == 1)
        if (eTaskGetState(handle) == eSuspended) {
            taskSuspended = true;
            break;
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (isTaskAlive(handle)) {
        if (!taskSuspended) {
            LOGE("CRITICAL: MacroEngine stuck. Force deleting task.");
        }
        vTaskDelete(handle);
    }
    cleanupTaskResources();
    LOGI("MacroEngine stopped");

    // FAIL-SAFE: Always release keys when stopping
    if(_keyboardService) _keyboardService->releaseAll();
    
    _stopSignal.store(false, std::memory_order_release);

    // Ensure visible state reflects that the engine is no longer running.
    {
        SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
        if (lock.isLocked()) {
            _state.status = MacroStatus::IDLE;
            _state.currentLine = 0;
            _state.currentScript.clear();
            _state.lastError.clear();
            _state.startTime = 0;
        }
    }
    notifyState();
}

// ... skipped startScript ...

// ... skipped getState ...

bool MacroEngine::startScript(const char* filename) {
    // Detect stale RUNNING state when the task is no longer alive.
    {
        SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
        if (lock.isLocked()) {
            if (!isTaskAlive(_taskHandle) && _state.status == MacroStatus::RUNNING) {
                LOGW("Stale RUNNING state detected. Resetting to IDLE.");
                _state.status = MacroStatus::IDLE;
                _taskHandle = nullptr;
            }
        } else {
            return false;
        }
    }

    // Lazy start
    {
        SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
        if (lock.isLocked()) {
            if (!isTaskAlive(_taskHandle)) {
                lock.unlock();
                start();
            }
        } else {
            return false;
        }
    }

    bool started = false;
    TaskHandle_t handle = nullptr;
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
    if (lock.isLocked()) {
        if (_state.status == MacroStatus::RUNNING) {
            // Guard against stale state when task died unexpectedly.
            if (isTaskAlive(_taskHandle)) {
                return false; // Already running
            }
            _state.status = MacroStatus::IDLE;
        }

        if (!isTaskAlive(_taskHandle)) {
            LOGE("MacroEngine task not available; cannot start script");
            return false;
        }
        
        _state.currentScript = filename;
        _state.status = MacroStatus::RUNNING;
        _state.currentLine = 0;
        _state.lastError.clear();
        _state.startTime = millis();
        _stopSignal.store(false, std::memory_order_release);
        
        handle = _taskHandle;
        lock.unlock();
        started = true;
        if (handle) xTaskNotifyGive(handle);
    }
    
    if (started) notifyState();
    return started;
}

void MacroEngine::stopScript() {
    _stopSignal = true; // Signals inner loop to break
}

void MacroEngine::clearState(bool notify) {
    {
        SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
        if (!lock.isLocked()) return;
        _state.status = MacroStatus::IDLE;
        _state.currentLine = 0;
        _state.currentScript.clear();
        _state.lastError.clear();
        _state.startTime = 0;
    }
    if (notify) {
        notifyState();
    }
}

MacroState MacroEngine::getState() {
    MacroState copy;
    copy.status = MacroStatus::IDLE;
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
    if (lock.isLocked()) {
        copy = _state;
    }
    return copy;
}

void MacroEngine::setStateCallback(StateCallback cb) {
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
    if (lock.isLocked()) {
        _stateCallback = cb;
    }
}

void MacroEngine::notifyState() {
     StateCallback cb = nullptr;
     MacroState copy;
     
     SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(50));
     if (lock.isLocked()) {
         if (_stateCallback) {
             cb = _stateCallback;
             copy = _state;
         }
     }
     
     if (cb) cb(copy);
}

bool MacroEngine::executionLimitExceeded(uint32_t scriptStartMs, uint32_t executedCommands, PsramString& error) const {
    if (executedCommands > LIMITS::MAX_EXPANDED_COMMANDS) {
        error = "Macro command limit exceeded";
        return true;
    }

    const uint32_t elapsedMs = millis() - scriptStartMs;
    if (elapsedMs > LIMITS::MAX_RUNTIME_MS) {
        error = "Macro runtime limit exceeded";
        return true;
    }

    error.clear();
    return false;
}

bool MacroEngine::isTaskAlive(TaskHandle_t handle) const {
    if (!handle) return false;
#if defined(INCLUDE_eTaskGetState) && (INCLUDE_eTaskGetState == 1)
    eTaskState state = eTaskGetState(handle);
    return (state != eDeleted && state != eInvalid);
#else
    return true;
#endif
}

void MacroEngine::cleanupTaskResources() {
    {
        SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
        if (lock.isLocked()) {
            _taskHandle = nullptr;
        }
    }
    if (_taskStack) { heap_caps_free(_taskStack); _taskStack = nullptr; }
    if (_taskTcb) { heap_caps_free(_taskTcb); _taskTcb = nullptr; }
}

} // namespace MACROS
