#include "ShellyWorker.h"
#include "../../config/App.h"  // TIMEOUT::* constants
#include "../../config/System.h"
#include "../device/ShellyDeviceManager.h"
#include "../control/ShellyRelayController.h"
#include "../../system/logging/Logging.h"
#include "../../system/health/heap/HeapMonitor.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../system/watchdog/TaskWatchdog.h"

#include <esp_heap_caps.h>

#undef LOG_TAG
#define LOG_TAG "ShellyWork"

namespace SHELLY {

// Statistics now stored in RTC::runtimeStats to survive deep sleep

ShellyWorker::ShellyWorker(ShellyDeviceManager& deviceManager,
                           ShellyRelayController& relayController,
                           std::atomic<bool>& runningFlag)
    : _deviceManager(deviceManager)
    , _relayController(relayController)
    , _running(runningFlag)
    , _commandQueue(nullptr)
    , _queueStorage(nullptr)
    , _queueBuffer(nullptr)
    , _taskHandle(nullptr)
    , _taskStack(nullptr)
    , _taskBuffer(nullptr) {
    // Queue creation deferred to start() for lazy loading
}

ShellyWorker::~ShellyWorker() {
    stop();
}

bool ShellyWorker::reclaimFinishedTaskIfNeeded() {
    if (!_taskHandle || !_isTaskFinished.load(std::memory_order_acquire)) {
        return false;
    }

    // stop() normally reclaims the suspended task immediately after it signals
    // _isTaskFinished. Keep this helper so a later begin()/stop() can recover
    // the same worker after a delayed network unwind instead of getting stuck
    // with a stale task handle forever.
    vTaskDelete(_taskHandle);
    _taskHandle = nullptr;
    destroyResources();
    return true;
}

bool ShellyWorker::start() {
    if (reclaimFinishedTaskIfNeeded()) {
        LOGI("Recovered Shelly worker after delayed stop");
    }

    if (_taskHandle) {
        LOGW("Worker already running");
        return false;
    }

    if (!_commandQueue) {
        // Queue storage in PSRAM, controller in internal DRAM
        _queueStorage = (uint8_t*)heap_caps_malloc(kCommandQueueSize * sizeof(ShellyCommand), MALLOC_CAP_SPIRAM);
        _queueBuffer = (StaticQueue_t*)heap_caps_malloc(sizeof(StaticQueue_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!_queueStorage || !_queueBuffer) {
            LOGE("Failed to allocate command queue");
            if (_queueStorage) { heap_caps_free(_queueStorage); _queueStorage = nullptr; }
            if (_queueBuffer) { heap_caps_free(_queueBuffer); _queueBuffer = nullptr; }
            return false;
        }

        _commandQueue = xQueueCreateStatic(kCommandQueueSize, sizeof(ShellyCommand), _queueStorage, _queueBuffer);
        if (!_commandQueue) {
            LOGE("Failed to create command queue");
            heap_caps_free(_queueStorage);
            heap_caps_free(_queueBuffer);
            _queueStorage = nullptr;
            _queueBuffer = nullptr;
            return false;
        }
    }

    if (!_taskStack) {
        // Reverted change: LwIP TCP ISN requires hardware stack in internal DRAM!
        _taskStack = (StackType_t*)heap_caps_malloc(kWorkerStackSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        _taskBuffer = (StaticTask_t*)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    _isTaskFinished.store(false, std::memory_order_release);

    if (_taskStack && _taskBuffer) {
        _taskHandle = xTaskCreateStaticPinnedToCore(
            taskEntry, "ShellyWorker", kWorkerStackSize, this,
            CONFIG::TASKS::PRIO_SHELLY, _taskStack, _taskBuffer, CONFIG::TASKS::CORE_SHELLY
        );
    }

    if (!_taskHandle) {
        LOGE("Failed to create worker task");
        destroyResources();
        return false;
    }

    LOGI("Worker started (stack=%u)", kWorkerStackSize);
    return true;
}

bool ShellyWorker::stop() {
    if (reclaimFinishedTaskIfNeeded()) {
        return true;
    }

    TaskHandle_t handle = _taskHandle;
    if (!handle) {
        return true;
    }

    _running.store(false, std::memory_order_release);
    xTaskAbortDelay(handle);
    _relayController.cancelActiveIo();

    const unsigned long startWait = millis();
    while (!_isTaskFinished.load(std::memory_order_acquire)) {
        vTaskDelay(pdMS_TO_TICKS(20));
        if (millis() - startWait > kShutdownTimeoutMs) {
            LOGE("Worker did not stop within %lu ms; leaving resources intact",
                 (unsigned long)kShutdownTimeoutMs);
            return false;
        }
    }

    vTaskDelete(handle);
    _taskHandle = nullptr;
    destroyResources();

    LOGI("Worker stopped (cmds=%u, fails=%u, polls=%u)",
         RTC::runtimeStats.shellyCmdExecuted, RTC::runtimeStats.shellyCmdFailed, RTC::runtimeStats.shellyPollCycles);
    return true;
}

bool ShellyWorker::queueCommand(const char* id, bool turnOn) {
    if (!id || id[0] == '\0') {
        LOGW("Invalid device ID (null or empty)");
        return false;
    }
    
    if (!_commandQueue) {
        LOGW("Queue not initialized");
        return false;
    }

    ShellyCommand cmd;
    cmd.type = ShellyCommand::SET_RELAY;
    strlcpy(cmd.id, id, sizeof(cmd.id));
    cmd.value = turnOn;

    if (xQueueSend(_commandQueue, &cmd, pdMS_TO_TICKS(50)) != pdTRUE) {
        LOGW("Queue full, dropping: %s", id);
        return false;
    }

    LOGD("Queued %s -> %s (depth: %u)", id, turnOn ? "ON" : "OFF",
         uxQueueMessagesWaiting(_commandQueue));
    return true;
}

void ShellyWorker::taskEntry(void* param) {
    ShellyWorker* self = static_cast<ShellyWorker*>(param);
    self->taskLoop();
    self->_isTaskFinished.store(true, std::memory_order_release);
    vTaskSuspend(nullptr);
}

void ShellyWorker::taskLoop() {
    unsigned long lastPoll = 0;
    size_t nextPollIndex = 0;
    
    // Calculate partial polling interval to distribute load
    // e.g. if 15s total interval and 8 devices, we want to poll one device every ~1.8s
    // But we also want to be responsive. Let's aim to complete a full cycle in kPollIntervalMs.
    // We dynamically adjust 'stepInterval' based on device count.
    
    LOGI("Task loop started (stack=%u)", kWorkerStackSize);
    
    // Initial stack measurement
    LOG_STACK_SIZE(kWorkerStackSize);

    auto& watchdog = SYSTEM::TaskWatchdog::instance();
    // March 2026 note:
    // Shelly used to stay outside TWDT because its HTTP/TLS path could block
    // for seconds. UnifiedHttpClient now feeds TaskWatchdog around blocking
    // connect/read loops, so this worker can finally join the same supervision
    // model as Heartbeat/Notification without false positives during slow I/O.
    const bool watchdogRegistered =
        watchdog.isInitialized() && watchdog.registerCurrentTask();
    if (watchdogRegistered) {
        (void)watchdog.reset();
    }

    while (_running.load()) {
        if (watchdogRegistered) {
            (void)watchdog.reset();
        }

        ShellyCommand cmd;
        
        // 1. Process ALL pending commands first (Highest Priority)
        // We loop until queue is empty to ensure UI responsiveness
        while (xQueueReceive(_commandQueue, &cmd, 0) == pdTRUE) {
            LOGD("Processing: %s", cmd.id);
            processCommand(cmd);
            if (watchdogRegistered) {
                (void)watchdog.reset();
            }
            // Yield between commands to give Shelly time to react
            if (uxQueueMessagesWaiting(_commandQueue) > 0) {
                vTaskDelay(pdMS_TO_TICKS(500)); 
            } else {
                // Short break after the last command before starting next cycle (polling etc)
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }

        // 2. Interleaved Polling
        // Calculate step interval based on current device count
        size_t devCount = _deviceManager.getDeviceCount();
        unsigned long stepInterval = 2000; // Default fallback
        
        if (devCount > 0) {
            // stepInterval is calculated in the loop per-device
            stepInterval = 500; // Minimal yield
        }

        if (devCount > 0 && (millis() - lastPoll > stepInterval)) {
            // Shelly devices are polled over the STA uplink. SoftAP alone is not routeable
            // to the LAN where Shelly devices live, so fail fast and release sockets.
            bool isStaConnected = (WiFi.status() == WL_CONNECTED);

            if (!isStaConnected) {
                _relayController.releaseResources();
                lastPoll = millis();
                nextPollIndex = 0;
                continue;
            }

            // It's time to poll the NEXT device
            
            // Validate index
            if (nextPollIndex >= devCount) nextPollIndex = 0;
            
            ShellyDevice dev;
            if (_deviceManager.getDeviceByIndex(nextPollIndex, dev)) {
                // Calculate device-specific step interval
                unsigned long deviceStepInterval = (kPollIntervalMs / devCount) * dev.pollBackoff;
                if (deviceStepInterval < 500) deviceStepInterval = 500;

                if (dev.enabled && (millis() - lastPoll > deviceStepInterval)) {
                    LOG_PROFILE_START(pollStart);
                    bool success = _relayController.pollDevice(dev);
                    LOG_PROFILE_END_SMART(pollStart, "Shelly individual poll", TASK_MONITOR::INTERVAL_SHELLY_POLL_MS, TASK_MONITOR::THRESHOLD_SHELLY_POLL_US);
                    (void)success;
                    if (watchdogRegistered) {
                        (void)watchdog.reset();
                    }
                    
                    RTC::runtimeStats.shellyPollCycles++; 

                    // Move to next device
                    nextPollIndex++;
                    lastPoll = millis();
                } else if (!dev.enabled) {
                    // Skip disabled
                    nextPollIndex++;
                }
                // Else: not yet time for THIS device, but we stay on this index to check it next loop?
                // Actually, to avoid blocking other devices, if it's not time for this one, we could move to next, 
                // but that would speed up everything. 
                // Better approach: the nextPollIndex should point to the device we WANT to poll.
                // If it's not time for it, we just wait (vTaskDelay at end of loop).
            }
            
            // Memory Usage Monitoring (only effectively once per cycle or so?)
            // We can log it less frequently
        } else if (devCount == 0) {
             // No devices - release any allocated resources (memory cleanup)
             _relayController.releaseResources();
             lastPoll = millis();
        }
            
        // 3. Stack/Heap Monitoring (Periodic)
        LOG_STACK_PERIODIC(kWorkerStackSize);

        // Short delay to yield CPU, but short enough to be responsive to Queue
        if (watchdogRegistered) {
            (void)watchdog.reset();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    if (watchdogRegistered) {
        (void)watchdog.unregisterCurrentTask();
    }
    LOGI("Task loop exiting");
}

void ShellyWorker::processCommand(const ShellyCommand& cmd) {
    switch (cmd.type) {
        case ShellyCommand::SET_RELAY: {
            LOG_PROFILE_START(setStart);
            bool success = _relayController.setRelay(cmd.id, cmd.value);
            LOG_PROFILE_END_SMART(setStart, "Shelly setRelay", TASK_MONITOR::INTERVAL_SHELLY_RELAY_MS, TASK_MONITOR::THRESHOLD_SHELLY_RELAY_US);
            if (success) {
                RTC::runtimeStats.shellyCmdExecuted++;
            } else {
                RTC::runtimeStats.shellyCmdFailed++;
            }
            break;
        }
    }
}

void ShellyWorker::destroyResources() {
    if (_commandQueue) {
        vQueueDelete(_commandQueue);
        _commandQueue = nullptr;
    }
    if (_queueStorage) {
        heap_caps_free(_queueStorage);
        _queueStorage = nullptr;
    }
    if (_queueBuffer) {
        heap_caps_free(_queueBuffer);
        _queueBuffer = nullptr;
    }
    if (_taskStack) {
        heap_caps_free(_taskStack);
        _taskStack = nullptr;
    }
    if (_taskBuffer) {
        heap_caps_free(_taskBuffer);
        _taskBuffer = nullptr;
    }
}

} // namespace SHELLY
