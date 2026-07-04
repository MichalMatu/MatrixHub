#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../../config/TaskConfig.h"
#include "LockDiagnostics.h"

namespace SYSTEM {

/**
 * @brief RAII wrapper for FreeRTOS mutex/semaphore
 */
class ScopeLock {
public:
    /**
     * @brief Try to acquire mutex
     * @param mutex Handle to semaphore/mutex
     * @param timeout Ticks to wait (default: 200ms - fail faster to help diagnostic)
     */
    explicit ScopeLock(SemaphoreHandle_t mutex, TickType_t timeout = TIMEOUT::MUTEX_STANDARD_TICKS)
        : _mutex(mutex) 
    {
        if (_mutex) {
            const uint32_t startMs = millis();
            _locked = xSemaphoreTake(_mutex, timeout) == pdTRUE;
            const TickType_t waitedTicks = pdMS_TO_TICKS(millis() - startMs);
            LOCK_DIAGNOSTICS::recordAcquire(false, waitedTicks, timeout, _locked);
        }
    }

    /**
     * @brief Release mutex if held
     */
    ~ScopeLock() {
        unlock();
    }

    /**
     * @brief Check if mutex was successfully acquired
     */
    bool isLocked() const {
        return _locked;
    }

    /**
     * @brief Manually release mutex early
     */
    void unlock() {
        if (_locked && _mutex) {
            xSemaphoreGive(_mutex);
            _locked = false;
        }
    }
    
    // Disable copy
    ScopeLock(const ScopeLock&) = delete;
    ScopeLock& operator=(const ScopeLock&) = delete;

    // Allow move (optional, but good practice)
    ScopeLock(ScopeLock&& other) noexcept : _mutex(other._mutex), _locked(other._locked) {
        other._mutex = nullptr;
        other._locked = false;
    }
    
    ScopeLock& operator=(ScopeLock&& other) noexcept {
        if (this != &other) {
            unlock();
            _mutex = other._mutex;
            _locked = other._locked;
            other._mutex = nullptr;
            other._locked = false;
        }
        return *this;
    }

private:
    SemaphoreHandle_t _mutex = nullptr;
    bool _locked = false;
};

/**
 * @brief RAII wrapper for FreeRTOS Recursive Mutex
 */
class RecursiveScopeLock {
public:
    /**
     * @brief Try to acquire recursive mutex
     * @param mutex Handle to recursive mutex
     * @param timeout Ticks to wait (default: 100ms)
     */
    explicit RecursiveScopeLock(SemaphoreHandle_t mutex, TickType_t timeout = TIMEOUT::MUTEX_STANDARD_TICKS)
        : _mutex(mutex) 
    {
        if (_mutex) {
            const uint32_t startMs = millis();
            _locked = xSemaphoreTakeRecursive(_mutex, timeout) == pdTRUE;
            const TickType_t waitedTicks = pdMS_TO_TICKS(millis() - startMs);
            LOCK_DIAGNOSTICS::recordAcquire(true, waitedTicks, timeout, _locked);
        }
    }

    /**
     * @brief Release mutex if held
     */
    ~RecursiveScopeLock() {
        unlock();
    }

    /**
     * @brief Check if mutex was successfully acquired
     */
    bool isLocked() const {
        return _locked;
    }

    /**
     * @brief Manually release mutex early
     */
    void unlock() {
        if (_locked && _mutex) {
            xSemaphoreGiveRecursive(_mutex);
            _locked = false;
        }
    }
    
    // Disable copy
    RecursiveScopeLock(const RecursiveScopeLock&) = delete;
    RecursiveScopeLock& operator=(const RecursiveScopeLock&) = delete;

    // Allow move
    RecursiveScopeLock(RecursiveScopeLock&& other) noexcept : _mutex(other._mutex), _locked(other._locked) {
        other._mutex = nullptr;
        other._locked = false;
    }
    
    RecursiveScopeLock& operator=(RecursiveScopeLock&& other) noexcept {
        if (this != &other) {
            unlock();
            _mutex = other._mutex;
            _locked = other._locked;
            other._mutex = nullptr;
            other._locked = false;
        }
        return *this;
    }

private:
    SemaphoreHandle_t _mutex = nullptr;
    bool _locked = false;
};

} // namespace SYSTEM
