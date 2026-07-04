/**
 * @file TaskWatchdog.h
 * @brief FreeRTOS Task Watchdog integration for ESP-IDF
 * 
 * Provides centralized task watchdog management:
 * - Register/unregister tasks with hardware watchdog
 * - Automatic heartbeat tracking
 * - Graceful handling of task lifecycle
 * 
 * Usage:
 *   // In task function:
 *   TaskWatchdog::registerCurrentTask();
 *   while (running) {
 *       // ... work ...
 *       TaskWatchdog::reset();  // Heartbeat
 *   }
 *   TaskWatchdog::unregisterCurrentTask();
 */

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#ifndef NATIVE_BUILD
#include <esp_task_wdt.h>
#endif

namespace SYSTEM {

class TaskWatchdog {
public:
    static TaskWatchdog& instance() {
        // Intentional singleton: the ESP-IDF task watchdog is a process-wide
        // hardware/runtime facility shared across many tasks. Keeping one
        // global coordinator here is simpler and clearer than threading it
        // through nearly every long-running task as a normal DI service.
        //
        // Leave this as-is unless the platform abstraction changes
        // fundamentally. DI would not create meaningful multiple watchdog
        // instances; it would only add boilerplate around one device-global
        // hardware facility.
        static TaskWatchdog _instance;
        return _instance;
    }

    /**
     * Initialize the Task Watchdog Timer with default settings.
     * Should be called once during system startup.
     * 
     * @param timeoutSec Watchdog timeout in seconds (default: 30s)
     * @param panicOnTimeout If true, trigger ESP32 panic on WDT timeout
     */
    void begin(uint32_t timeoutSec = 30, bool panicOnTimeout = true);
    
    /**
     * Register the current task with the watchdog.
     * Must be called from within the task to be monitored.
     * 
     * @return true if registration succeeded
     */
    bool registerCurrentTask();
    
    /**
     * Register a specific task handle with the watchdog.
     * 
     * @param taskHandle Handle of the task to register
     * @return true if registration succeeded
     */
    bool registerTask(TaskHandle_t taskHandle);
    
    /**
     * Unregister the current task from the watchdog.
     * Should be called before task deletion.
     * 
     * @return true if unregistration succeeded
     */
    bool unregisterCurrentTask();
    
    /**
     * Unregister a specific task from the watchdog.
     * 
     * @param taskHandle Handle of the task to unregister
     * @return true if unregistration succeeded
     */
    bool unregisterTask(TaskHandle_t taskHandle);
    
    /**
     * Reset (feed) the watchdog for the current task.
     * Must be called periodically from within the registered task.
     * 
     * @return true if reset succeeded
     */
    bool reset();
    
    /**
     * Check if watchdog is initialized
     */
    bool isInitialized() const { return _initialized; }
    
    /**
     * Get the configured timeout in seconds
     */
    uint32_t getTimeoutSec() const { return _timeoutSec; }
    
private:
    TaskWatchdog() = default;
    TaskWatchdog(const TaskWatchdog&) = delete;
    TaskWatchdog& operator=(const TaskWatchdog&) = delete;

    bool _initialized = false;
    uint32_t _timeoutSec = 30;
    // Track whether the task that called `begin()` was registered with WDT
    // to avoid attempting to register it again later (which logs errors).
    bool _mainTaskRegistered = false;
    TaskHandle_t _mainTaskHandle = nullptr;
};

}  // namespace SYSTEM
