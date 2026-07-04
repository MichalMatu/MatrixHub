/**
 * @file SensorTaskLoop.cpp
 * @brief FreeRTOS task loop implementation
 */

#include "SensorTaskLoop.h"
#include "../runtime/SensorCommandQueue.h"
#include "../runtime/SensorSnapshotHealth.h"
#include "../runtime/SensorState.h"
#include "../model/ISensorService.h"
#include "../logging/SensorBinaryLogger.h"

#include "../../alarms/AlarmService.h"
#include "../../system/Application.h"
#include "../../system/watchdog/TaskWatchdog.h"
#include "../../config/App.h"
#include "../../system/logging/Logging.h"

#include <cstring>
#include <atomic>

#undef LOG_TAG
#define LOG_TAG "TelemetryLoop"

namespace SENSORS {

namespace {

bool hasReachedDeadline(uint32_t nowMs, uint32_t deadlineMs) {
    return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}

bool waitUntilNextReadSlot(uint32_t deadlineMs, std::atomic<bool>* shouldRunFlag) {
    while (shouldRunFlag && shouldRunFlag->load(std::memory_order_acquire)) {
        const uint32_t nowMs = millis();
        if (hasReachedDeadline(nowMs, deadlineMs)) {
            return true;
        }

        uint32_t waitMs = deadlineMs - nowMs;
        if (waitMs > SENSOR::TASK_LOOP::POLL_STEP_MS) {
            waitMs = SENSOR::TASK_LOOP::POLL_STEP_MS;
        }
        if (waitMs == 0) {
            return true;
        }

        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(waitMs)) > 0) {
            // Wake early for stop/force-read requests and let the loop decide
            // whether to consume a queued command or exit.
            return true;
        }

        SYSTEM::TaskWatchdog::instance().reset();
    }

    return false;
}

bool shouldLogThrottled(uint32_t nowMs, uint32_t& lastLogMs, uint32_t intervalMs, uint32_t& suppressed) {
    if (lastLogMs == 0 || nowMs - lastLogMs >= intervalMs) {
        lastLogMs = nowMs;
        return true;
    }

    if (suppressed < UINT32_MAX) {
        ++suppressed;
    }
    return false;
}

uint32_t consumeSuppressed(uint32_t& suppressed) {
    const uint32_t value = suppressed;
    suppressed = 0;
    return value;
}

uint32_t advanceReadDeadline(uint32_t previousDeadlineMs, uint32_t nowMs) {
    uint32_t nextDeadlineMs = previousDeadlineMs + SENSOR::READ_INTERVAL_MS;

    // Skip over missed slots instead of attempting catch-up reads back-to-back.
    // That keeps the loop aligned with the sensor cadence without spamming
    // immediate NO_DATA polls after one slow iteration.
    while (hasReachedDeadline(nowMs, nextDeadlineMs)) {
        nextDeadlineMs += SENSOR::READ_INTERVAL_MS;
    }

    return nextDeadlineMs;
}

}  // namespace

void SensorTaskLoop::setControlState(TaskHandle_t* handle, std::atomic<bool>* shouldRun,
                                     std::atomic<uint32_t>* lastReadTime, std::atomic<uint32_t>* lastLogTime,
                                     std::function<void(const SensorSnapshot&, bool)>* updateCallback,
                                     SemaphoreHandle_t* updateCallbackMutex,
                                     SemaphoreHandle_t* stopAck) {
    _pTaskHandle = handle;
    _pShouldRun = shouldRun;
    _pLastReadTime = lastReadTime;
    _pLastLogTime = lastLogTime;
    _pUpdateCallback = updateCallback;
    _pUpdateCallbackMutex = updateCallbackMutex;
    _pStopAck = stopAck;
}

void SensorTaskLoop::setSensorService(ISensorService* sensorService) {
    _pSensorService = sensorService;
}

void SensorTaskLoop::setAlarmService(ALARMS::AlarmService* alarmService) {
    _pAlarmService = alarmService;
}

void SensorTaskLoop::taskLoop(void* parameter) {
    auto* instance = static_cast<SensorTaskLoop*>(parameter);
    if (instance) {
        instance->run();
    }
    LOGI("Sensor logging task loop suspending (safe for deletion)");
    if (instance) {
        instance->signalStopAck();
    }
    vTaskSuspend(nullptr);
}

void SensorTaskLoop::signalStopAck() {
    if (_pStopAck && *_pStopAck) {
        (void)xSemaphoreGive(*_pStopAck);
    }
}

void SensorTaskLoop::run() {
    LOGI("Loop started (continuous polling mode, stack=%u)", SENSOR::STACK_SIZE);
    
    // Initial stack measurement
    LOG_STACK_BUDGET(CONFIG::TASKS::STACK_BUDGET_SENSOR_LOGGING);
    
    // Register with Task Watchdog early so the stabilization phase is monitored
    SYSTEM::TaskWatchdog::instance().registerCurrentTask();
    
    if (_pLastReadTime) _pLastReadTime->store(0);
    if (_pLastLogTime) _pLastLogTime->store(millis());
    
    // Initialize logging buffer (PSRAM)
    SensorBinaryLogger::begin();
    
    // Initialize sensor in background task (takes ~1000ms)
    // This prevents blocking the main thread during boot
    if (_pSensorService) {
        if (!_pSensorService->begin()) {
            LOGE("Sensor service init failed in task");
        }
    } else {
        LOGE("No sensor service injected!");
    }

    auto shouldRun = [&]() -> bool {
        return _pShouldRun && _pShouldRun->load();
    };

    // Wait for first measurement to stabilize (SCD4x needs ~5s after start)
    LOGI("Waiting for first measurement...");
    uint32_t stabilizeStart = millis();
    bool firstDataReady = false;
    uint32_t nextReadDeadlineMs = 0;
    while (shouldRun() && (millis() - stabilizeStart < SENSOR::TASK_LOOP::STABILIZATION_TIMEOUT_MS)) {
        SensorSnapshot snap;
        PhaseStatus readStatus;
        if (_pSensorService) {
            _pSensorService->readAll(snap, readStatus);
        } else {
            readStatus.ok = false;
            readStatus.error_code = "NO_SERVICE";
        }
        
        if (readStatus.ok) {
            SensorSnapshot prev = SensorState::getSnapshot();
            snap.seq = prev.seq + 1;
            SensorState::updateAfterRead(snap, readStatus);
            LOGI("First measurement ready: CO2=%u ppm, T=%.1f°C, RH=%.1f%%", 
                 snap.co2, snap.temp, snap.humid);
            firstDataReady = true;
            if (_pLastReadTime) _pLastReadTime->store(millis());
            break;
        }
        // Feed watchdog while waiting for sensor stabilization
        SYSTEM::TaskWatchdog::instance().reset();
        vTaskDelay(pdMS_TO_TICKS(SENSOR::TASK_LOOP::STABILIZATION_POLL_MS));
    }

    if (!shouldRun()) {
        goto exit_loop;
    }
    
    if (!firstDataReady) {
        LOGW("Stabilization timeout - continuing without initial data");
    }
    
    // (Already registered earlier) Watchdog is active for main loop
    
    // Reset per-task static state on each start
    static uint8_t consecutiveFailures = 0;
    static bool firstFlashAttempted = false;
    static uint32_t lastCallbackDropMs = 0;
    static uint32_t lastReadErrorLogMs = 0;
    static uint32_t suppressedReadErrorLogs = 0;
    static uint32_t lastSelfHealLogMs = 0;
    static uint32_t suppressedSelfHealLogs = 0;
    nextReadDeadlineMs = millis();
    consecutiveFailures = 0;
    firstFlashAttempted = false;
    lastCallbackDropMs = 0;
    lastReadErrorLogMs = 0;
    suppressedReadErrorLogs = 0;
    lastSelfHealLogMs = 0;
    suppressedSelfHealLogs = 0;

    // Main loop: continuous polling
    while (shouldRun()) {
        if (!waitUntilNextReadSlot(nextReadDeadlineMs, _pShouldRun)) {
            break;
        }

        uint32_t now = millis();
        
        // Check for commands (still support force commands)
        SensorTaskCommand cmd;
        bool forceLog = false;
        if (SensorCommandQueue::tryReceive(cmd)) {
            LOGD("Command received: %d", cmd);
            if (cmd == CMD_FORCE_LOG || cmd == CMD_FORCE_READ_AND_LOG) {
                forceLog = true;
            }
        }
        
        // Continuous polling: keep the read loop aligned with the SCD4x cadence
        // even if downstream work (alarms/UI/logging) occasionally runs long.
        SensorSnapshot snap;
        PhaseStatus readStatus;
        
        LOG_PROFILE_START(i2cStart);
        if (_pSensorService) {
            _pSensorService->readAll(snap, readStatus);
        } else {
             readStatus.ok = false;
             readStatus.error_code = "NO_SERVICE";
        }
        LOG_PROFILE_END_SMART(i2cStart, "I2C readAll", TASK_MONITOR::INTERVAL_I2C_READ_MS, TASK_MONITOR::THRESHOLD_I2C_READ_US);

        if (readStatus.ok) {
            const uint32_t suppressedErrors = consumeSuppressed(suppressedReadErrorLogs);
            if (suppressedErrors > 0) {
                LOGI("Sensor reads recovered after %lu suppressed error logs",
                     static_cast<unsigned long>(suppressedErrors));
            }
            lastReadErrorLogMs = 0;
            lastSelfHealLogMs = 0;
            suppressedSelfHealLogs = 0;
            consecutiveFailures = 0;
            // New data available - update state
            SensorSnapshot prev = SensorState::getSnapshot();
            snap.seq = prev.seq + 1;
            SensorState::updateAfterRead(snap, readStatus);
            if (_pLastReadTime) _pLastReadTime->store(now);

            // Retain every successful sensor sample in PSRAM history; flash logging
            // remains decimated and is handled separately below.
            SensorBinaryLogger::pushSnapshot(snap);

            // Submit fresh sensor data for later alarm evaluation. This task
            // no longer runs the alarm pipeline synchronously; it only refreshes
            // the shared "latest state" snapshot and keeps moving.
            if (_pAlarmService) {
                ALARMS::AlarmInputData input;
                input.co2 = static_cast<float>(snap.co2);
                input.temperature = snap.temp;
                input.humidity = snap.humid;
                input.wifiVariance = NAN;  // WiFi variance handled separately
                _pAlarmService->submitInput(input);
            }
            
            // Notify WebSocket observers (via SensorLoggingTask callback)
            std::function<void(const SensorSnapshot&, bool)> localCallback = nullptr;
            if (_pUpdateCallback) {
                if (_pUpdateCallbackMutex && *_pUpdateCallbackMutex) {
                    if (xSemaphoreTake(*_pUpdateCallbackMutex, pdMS_TO_TICKS(SENSOR::TASK_LOOP::UPDATE_CALLBACK_LOCK_TIMEOUT_MS)) == pdTRUE) {
                        localCallback = *_pUpdateCallback;
                        xSemaphoreGive(*_pUpdateCallbackMutex);
                    } else if (now - lastCallbackDropMs >= TASK_MONITOR::SLOW_LOG_THROTTLE_MS) {
                        LOGW("Update callback mutex busy - dropping UI update");
                        lastCallbackDropMs = now;
                    }
                } else {
                    localCallback = *_pUpdateCallback;
                }
            }
            if (localCallback) {
                localCallback(snap, true);
            }
        }
        // NO_DATA is normal between 5 s SCD4x updates, so we usually skip it
        // silently. If the newest retained sample ages past the freshness
        // timeout, promote that long-running condition to an explicit STALE
        // state once so live dashboards stop looking healthy forever.
        else if (readStatus.error_code && strcmp(readStatus.error_code, "NO_DATA") == 0) {
            SensorSnapshot currentSnap = SensorState::getSnapshot();
            PhaseStatus currentState = SensorState::getLastReadStatus();
            const bool alreadyStale =
                !currentState.ok &&
                currentState.error_code &&
                strcmp(currentState.error_code, "STALE") == 0;

            if (currentSnap.seq > 0 &&
                shouldPromoteNoDataToStale(currentSnap.timestamp_ms,
                                           now,
                                           SENSOR::SNAPSHOT_TIMEOUT_MS,
                                           alreadyStale)) {
                PhaseStatus staleStatus{};
                staleStatus.ok = false;
                staleStatus.error_code = "STALE";
                // Do not mutate the retained sample itself here. We want the UI
                // to keep the last known values visible, but with an explicit
                // unhealthy bit and stale age instead of pretending a new read
                // succeeded.
                SensorState::updateAfterReadFailure(staleStatus);

                LOGW("Sensor snapshot stale after %lu ms without fresh data",
                     static_cast<unsigned long>(now - currentSnap.timestamp_ms));

                std::function<void(const SensorSnapshot&, bool)> localCallback = nullptr;
                if (_pUpdateCallback) {
                    if (_pUpdateCallbackMutex && *_pUpdateCallbackMutex) {
                        if (xSemaphoreTake(*_pUpdateCallbackMutex, pdMS_TO_TICKS(SENSOR::TASK_LOOP::UPDATE_CALLBACK_LOCK_TIMEOUT_MS)) == pdTRUE) {
                            localCallback = *_pUpdateCallback;
                            xSemaphoreGive(*_pUpdateCallbackMutex);
                        } else if (now - lastCallbackDropMs >= TASK_MONITOR::SLOW_LOG_THROTTLE_MS) {
                            LOGW("Update callback mutex busy - dropping UI stale update");
                            lastCallbackDropMs = now;
                        }
                    } else {
                        localCallback = *_pUpdateCallback;
                    }
                }
                if (localCallback) {
                    localCallback(currentSnap, false);
                }
            }
        }
        // Only log actual read errors
        else if (readStatus.error_code && strcmp(readStatus.error_code, "NO_DATA") != 0) {
            SensorState::updateAfterReadFailure(readStatus);
            if (shouldLogThrottled(
                    now,
                    lastReadErrorLogMs,
                    SENSOR::TASK_LOOP::READ_ERROR_LOG_INTERVAL_MS,
                    suppressedReadErrorLogs)) {
                const uint32_t suppressed = consumeSuppressed(suppressedReadErrorLogs);
                if (suppressed > 0) {
                    LOGW("Read error persists: %s (suppressed %lu repeats)",
                         readStatus.error_code,
                         static_cast<unsigned long>(suppressed));
                } else {
                    LOGW("Read error: %s", readStatus.error_code);
                }
            }

            // Surface hard read failures to live telemetry consumers immediately
            // instead of waiting for a later full snapshot/reconnect. We reuse
            // the last retained sample values on purpose: the dashboard should
            // keep showing the last known reading, but now with an explicit
            // "read failed" state instead of silently looking healthy forever.
            SensorSnapshot currentSnap = SensorState::getSnapshot();
            if (currentSnap.seq > 0) {
                std::function<void(const SensorSnapshot&, bool)> localCallback = nullptr;
                if (_pUpdateCallback) {
                    if (_pUpdateCallbackMutex && *_pUpdateCallbackMutex) {
                        if (xSemaphoreTake(*_pUpdateCallbackMutex, pdMS_TO_TICKS(SENSOR::TASK_LOOP::UPDATE_CALLBACK_LOCK_TIMEOUT_MS)) == pdTRUE) {
                            localCallback = *_pUpdateCallback;
                            xSemaphoreGive(*_pUpdateCallbackMutex);
                        } else if (now - lastCallbackDropMs >= TASK_MONITOR::SLOW_LOG_THROTTLE_MS) {
                            LOGW("Update callback mutex busy - dropping UI error update");
                            lastCallbackDropMs = now;
                        }
                    } else {
                        localCallback = *_pUpdateCallback;
                    }
                }
                if (localCallback) {
                    localCallback(currentSnap, false);
                }
            }
            
            consecutiveFailures++;
            if (consecutiveFailures >= SENSOR::TASK_LOOP::SELF_HEAL_FAILURE_THRESHOLD) {
                const bool logSelfHeal = shouldLogThrottled(
                    now,
                    lastSelfHealLogMs,
                    SENSOR::TASK_LOOP::SELF_HEAL_LOG_INTERVAL_MS,
                    suppressedSelfHealLogs);

                if (logSelfHeal) {
                    const uint32_t suppressed = consumeSuppressed(suppressedSelfHealLogs);
                    if (suppressed > 0) {
                        LOGW("Self-healing reinit after %d failures (suppressed %lu attempts)",
                             consecutiveFailures,
                             static_cast<unsigned long>(suppressed));
                    } else {
                        LOGW("Self-healing reinit after %d failures", consecutiveFailures);
                    }
                }
                
                // Attempt to re-initialize the bus and sensor
                // This call blocks for 1000ms, but we background task so it is safe
                if (_pSensorService && _pSensorService->reinitialize()) {
                    LOGI("Self-healing recovered sensor");
                    consecutiveFailures = 0; 
                } else {
                    if (!_pSensorService) {
                        LOGE("Self-healing failed: sensor service unavailable");
                    } else if (logSelfHeal) {
                        LOGW("Self-healing did not recover sensor");
                    }
                    // Backoff: don't spam re-init every loop, wait longer
                    consecutiveFailures = 0; // Reset to try again after another 10 failures
                    
                    // [FIX] Safe delay with watchdog reset to prevent crash at ~1m 39s
                    const uint32_t backoffMs = SENSOR::TASK_LOOP::SELF_HEAL_BACKOFF_MS;
                    const uint32_t stepMs = SENSOR::TASK_LOOP::SELF_HEAL_BACKOFF_STEP_MS;
                    uint32_t waited = 0;
                    while (waited < backoffMs && _pShouldRun && _pShouldRun->load()) {
                        vTaskDelay(pdMS_TO_TICKS(stepMs));
                        waited += stepMs;
                        SYSTEM::TaskWatchdog::instance().reset();
                    }
                }
            }
        }
        
        // Periodic logging (every LOG_INTERVAL_MS) or forced
        bool periodicLogDue = false;
        if (_pLastLogTime) {
            periodicLogDue = (now - _pLastLogTime->load() >= SENSOR::LOG_INTERVAL_MS);
        }
        
        bool shouldForceFlash = forceLog || !firstFlashAttempted;

        if (forceLog || periodicLogDue || shouldForceFlash) {
            SensorSnapshot logSnap = SensorState::getSnapshot();

            // Only log if we have valid data (seq > 0 means we had at least one read)
            if (logSnap.seq > 0) {
                LOGV_THROTTLED(TASK_MONITOR::INTERVAL_FLASH_WRITE_MS, "WRITE_START");
                PhaseStatus writeStatus;
                
                // Allow forcing flash write if explicitly requested OR it's the first write of the session
                bool forceFlash = shouldForceFlash;
                
                SensorBinaryLogger::writeSnapshot(logSnap, writeStatus, forceFlash);
                SensorState::updateAfterWrite(writeStatus);
                
                if (writeStatus.ok) {
                    LOGD_THROTTLED(TASK_MONITOR::INTERVAL_FLASH_WRITE_MS,
                                   "WRITE_OK (%.0f ms)",
                                   (float)writeStatus.duration_ms);
                    if (_pLastLogTime) _pLastLogTime->store(millis());
                } else {
                    LOGE("WRITE_FAIL: %s", writeStatus.error_code);
                }

                if (forceFlash && (!writeStatus.error_code || strcmp(writeStatus.error_code, "FS_BUSY") != 0)) {
                    firstFlashAttempted = true;
                }
            }
        }

        // Stack monitoring
        LOG_STACK_BUDGET_PERIODIC(CONFIG::TASKS::STACK_BUDGET_SENSOR_LOGGING);
        
        // Feed the watchdog - signals task is still responsive
        SYSTEM::TaskWatchdog::instance().reset();
        
        nextReadDeadlineMs = advanceReadDeadline(nextReadDeadlineMs, millis());
    }
    
exit_loop:
    // Unregister from watchdog before exit
    SYSTEM::TaskWatchdog::instance().unregisterCurrentTask();

    LOGI("Sensor logging task loop exiting");
}

}  // namespace SENSORS
