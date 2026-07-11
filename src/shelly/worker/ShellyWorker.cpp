#include "ShellyWorker.h"
#include "../../config/App.h"  // TIMEOUT::* constants
#include "../../config/System.h"
#include "../device/ShellyDeviceManager.h"
#include "../control/ShellyRelayController.h"
#include "../../system/logging/Logging.h"
#include "../../system/health/heap/HeapMonitor.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../system/utils/ScopeLock.h"
#include "../../system/watchdog/TaskWatchdog.h"
#include "../ShellyPeerRevision.h"

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
    , _desiredMutex(xSemaphoreCreateMutexStatic(&_desiredMutexBuffer))
    , _wakeQueue(nullptr)
    , _taskHandle(nullptr)
    , _taskStack(nullptr)
    , _taskBuffer(nullptr) {
    if (!_desiredMutex) {
        LOGE("Failed to create desired-state mutex");
    }
}

ShellyWorker::~ShellyWorker() {
    // The task owns `this`, the relay controller and device manager references.
    // A bounded operational stop may time out while HTTP unwinds, but object
    // destruction cannot continue safely until the task reaches its suspended
    // hand-off point. Prefer a stalled teardown over freeing those dependencies
    // underneath a live task.
    while (!stop()) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

bool ShellyWorker::reclaimFinishedTaskIfNeeded() {
    if (!_taskLifecycle.isReclaimable(_taskHandle != nullptr)) {
        return false;
    }

    // stop() normally reclaims the suspended task immediately after it signals
    // completion. Keep this helper so a later begin()/stop() can recover
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
        if (_taskLifecycle.requestRestart(true)) {
            const uint8_t wake = 1;
            if (_wakeQueue) {
                (void)xQueueSend(_wakeQueue, &wake, 0);
            }
            xTaskAbortDelay(_taskHandle);
            LOGI("Worker restart joined delayed stop");
            return true;
        }
        if (reclaimFinishedTaskIfNeeded()) {
            LOGI("Recovered Shelly worker at restart boundary");
        } else {
            LOGW("Worker task state unavailable for restart");
            return false;
        }
    }

    if (!_desiredMutex) {
        LOGE("Cannot start without desired-state mutex");
        return false;
    }

    {
        SYSTEM::ScopeLock desiredLock(_desiredMutex, portMAX_DELAY);
        if (!desiredLock.isLocked()) {
            LOGE("Failed to lock desired state during start");
            return false;
        }

        if (!_wakeQueue) {
            // Desired state is held in the fixed ledger. This one-byte queue
            // only nudges the worker, so saturation is harmless and expected.
            // Keep both buffers inline: a one-byte wake hint must never make a
            // boot-time alarm reconcile depend on transient PSRAM availability.
            _wakeQueue = xQueueCreateStatic(kWorkerWakeQueueSize, sizeof(uint8_t),
                                            _wakeQueueStorage, &_wakeQueueBuffer);
            if (!_wakeQueue) {
                LOGE("Failed to create worker wake queue");
                return false;
            }
        }
    }

    if (!_taskStack) {
        // Reverted change: LwIP TCP ISN requires hardware stack in internal DRAM!
        _taskStack = (StackType_t*)heap_caps_malloc(kWorkerStackSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        _taskBuffer = (StaticTask_t*)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    _taskLifecycle.prepareStart();

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

    _taskLifecycle.cancelRestartRequest();
    _running.store(false, std::memory_order_release);
    xTaskAbortDelay(handle);
    _relayController.cancelActiveIo();

    const unsigned long startWait = millis();
    while (_taskLifecycle.isLive(true)) {
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

bool ShellyWorker::queueCommand(const char* id,
                                bool turnOn,
                                uint64_t peerRevision,
                                TickType_t mutexTimeout) {
    if (!id || id[0] == '\0') {
        LOGW("Invalid device ID (null or empty)");
        return false;
    }
    
    if (!_desiredMutex) {
        LOGE("Desired-state mutex not initialized");
        return false;
    }

    uint32_t generation = 0;
    bool wakeQueued = false;
    {
        // The ledger critical section is bounded to a few fixed-size copies;
        // never hold it across network or device-manager calls.
        SYSTEM::ScopeLock lock(_desiredMutex, mutexTimeout);
        if (!lock.isLocked() ||
            !_desiredStates.upsert(id, turnOn, peerRevision, millis(), &generation)) {
            LOGW("Unable to store desired state: %s", id);
            return false;
        }

        if (_wakeQueue) {
            const uint8_t wake = 1;
            wakeQueued = xQueueSend(_wakeQueue, &wake, 0) == pdTRUE;
        }
    }

    LOGD("Desired %s -> %s (generation=%lu, wake=%s)",
         id, turnOn ? "ON" : "OFF", (unsigned long)generation,
         wakeQueued ? "queued" : "coalesced");
    return true;
}

bool ShellyWorker::parkCommand(const char* id,
                               bool turnOn,
                               uint64_t peerRevision,
                               TickType_t mutexTimeout) {
    if (!id || id[0] == '\0' || !_desiredMutex) {
        return false;
    }

    SYSTEM::ScopeLock lock(_desiredMutex, mutexTimeout);
    if (!lock.isLocked() ||
        !_desiredStates.upsert(id, turnOn, peerRevision, millis()) ||
        !_desiredStates.park(id, peerRevision)) {
        LOGW("Unable to park desired state: %s", id);
        return false;
    }
    return true;
}

void ShellyWorker::rebindDevice(const char* id, uint64_t peerRevision) {
    if (!id || !_desiredMutex) {
        return;
    }

    SYSTEM::ScopeLock lock(_desiredMutex, portMAX_DELAY);
    if (!lock.isLocked() || !_desiredStates.rebind(id, peerRevision, millis())) {
        return;
    }

    if (_wakeQueue) {
        const uint8_t wake = 1;
        (void)xQueueSend(_wakeQueue, &wake, 0);
    }
}

void ShellyWorker::parkDevice(const char* id, uint64_t peerRevision) {
    if (!id || !_desiredMutex) {
        return;
    }

    SYSTEM::ScopeLock lock(_desiredMutex, portMAX_DELAY);
    if (lock.isLocked()) {
        (void)_desiredStates.park(id, peerRevision);
    }
}

void ShellyWorker::forgetDevice(const char* id) {
    if (!id || !_desiredMutex) {
        return;
    }

    SYSTEM::ScopeLock lock(_desiredMutex, portMAX_DELAY);
    if (lock.isLocked()) {
        (void)_desiredStates.remove(id);
    }
}

void ShellyWorker::taskEntry(void* param) {
    ShellyWorker* self = static_cast<ShellyWorker*>(param);
    do {
        self->taskLoop();
    } while (self->_taskLifecycle.finishOrRestart());
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
        _taskLifecycle.acknowledgeLive();
        if (watchdogRegistered) {
            (void)watchdog.reset();
        }

        // Drain wake hints, then read the authoritative latest state from the
        // ledger. More hints than the queue can hold are intentionally merged.
        uint8_t wake = 0;
        while (_wakeQueue && xQueueReceive(_wakeQueue, &wake, 0) == pdTRUE) {
        }
        processDesiredCommands();
        if (watchdogRegistered) {
            (void)watchdog.reset();
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

        // Block briefly on the hint queue so a command wakes the worker without
        // making correctness depend on whether that one-slot queue is full.
        if (watchdogRegistered) {
            (void)watchdog.reset();
        }
        uint8_t wakeHint = 0;
        if (!_wakeQueue ||
            xQueueReceive(_wakeQueue, &wakeHint, pdMS_TO_TICKS(100)) != pdTRUE) {
            // xQueueReceive already provided the bounded delay when the queue
            // exists. This fallback is only for a missing runtime queue.
            if (!_wakeQueue) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
    }
    
    if (watchdogRegistered) {
        (void)watchdog.unregisterCurrentTask();
    }
    LOGI("Task loop exiting");
}

void ShellyWorker::processDesiredCommands() {
    while (_running.load(std::memory_order_acquire)) {
        ShellyDesiredStateLease lease;
        {
            SYSTEM::ScopeLock lock(_desiredMutex, portMAX_DELAY);
            if (!lock.isLocked() || !_desiredStates.beginNextReady(millis(), lease)) {
                return;
            }
        }

        LOGD("Processing desired state: %s generation=%lu", lease.id,
             (unsigned long)lease.generation);

        ShellyDevice commandPeer;
        const ShellyDeviceLookupResult commandLookup =
            _deviceManager.lookupDevice(lease.id, commandPeer);
        if (commandLookup != ShellyDeviceLookupResult::Found ||
            !commandPeer.enabled ||
            shellyPeerRevision(commandPeer) != lease.peerRevision) {
            SYSTEM::ScopeLock lock(_desiredMutex, portMAX_DELAY);
            if (lock.isLocked()) {
                if (commandLookup == ShellyDeviceLookupResult::Busy) {
                    // A mutex timeout says nothing about whether the peer still
                    // exists. Keep the exact generation and retry it later.
                    (void)_desiredStates.complete(
                        lease.id, lease.generation, false, millis());
                } else if (commandLookup == ShellyDeviceLookupResult::Found) {
                    if (commandPeer.enabled) {
                        (void)_desiredStates.rebindIfGeneration(
                            lease.id,
                            lease.generation,
                            shellyPeerRevision(commandPeer),
                            millis());
                        (void)_desiredStates.complete(
                            lease.id, lease.generation, false, millis());
                    } else {
                        (void)_desiredStates.parkIfGeneration(
                            lease.id,
                            lease.generation,
                            shellyPeerRevision(commandPeer));
                    }
                } else {
                    // NotFound is definitive, but only for the lease we looked
                    // up. A newer generation may already have been admitted
                    // and must survive this old decision.
                    (void)_desiredStates.removeIfGeneration(
                        lease.id, lease.generation);
                    (void)_desiredStates.complete(
                        lease.id, lease.generation, false, millis());
                }
            }
            continue;
        }

        LOG_PROFILE_START(setStart);
        const bool success = _relayController.setRelay(commandPeer, lease.value);
        LOG_PROFILE_END_SMART(setStart, "Shelly setRelay", TASK_MONITOR::INTERVAL_SHELLY_RELAY_MS, TASK_MONITOR::THRESHOLD_SHELLY_RELAY_US);

        ShellyDesiredStateCompletion completion = ShellyDesiredStateCompletion::Ignored;

        // A peer may be removed, disabled, or readdressed while HTTP is in
        // flight. Rebind before consuming the ACK so the old request can never
        // mark the new peer configuration as applied.
        ShellyDevice latestPeer;
        const ShellyDeviceLookupResult latestLookup =
            _deviceManager.lookupDevice(lease.id, latestPeer);
        {
            SYSTEM::ScopeLock lock(_desiredMutex, portMAX_DELAY);
            if (lock.isLocked()) {
                bool completionSuccess = success;
                if (latestLookup == ShellyDeviceLookupResult::Busy) {
                    // Even a successful HTTP response is not an ACK until the
                    // peer can be revalidated under DeviceManager's mutex.
                    completionSuccess = false;
                } else if (latestLookup != ShellyDeviceLookupResult::Found) {
                    (void)_desiredStates.removeIfGeneration(
                        lease.id, lease.generation);
                } else if (!latestPeer.enabled) {
                    (void)_desiredStates.parkIfGeneration(
                        lease.id,
                        lease.generation,
                        shellyPeerRevision(latestPeer));
                } else if (shellyPeerRevision(latestPeer) != lease.peerRevision) {
                    (void)_desiredStates.rebindIfGeneration(
                        lease.id,
                        lease.generation,
                        shellyPeerRevision(latestPeer),
                        millis());
                }
                completion = _desiredStates.complete(
                    lease.id, lease.generation, completionSuccess, millis());
            }
        }

        if (success) {
            RTC::runtimeStats.shellyCmdExecuted++;
            // Only the ACK for the current generation may publish observed
            // command state. A superseded in-flight request is transport-only.
            if (completion == ShellyDesiredStateCompletion::Applied) {
                (void)_deviceManager.updateCommandState(
                    lease.id, lease.value, true, &commandPeer);
            }
        } else {
            RTC::runtimeStats.shellyCmdFailed++;
        }
    }
}

void ShellyWorker::destroyResources() {
    if (_desiredMutex) {
        SYSTEM::ScopeLock lock(_desiredMutex, portMAX_DELAY);
        if (lock.isLocked()) {
            if (_wakeQueue) {
                vQueueDelete(_wakeQueue);
                _wakeQueue = nullptr;
            }
        }
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
