#pragma once

#include <Arduino.h>
#include <atomic>
#include <FS.h>

#include "ShellyTypes.h"
#include "ShellyConfig.h"
#include "device/ShellyDeviceManager.h"
#include "control/ShellyRelayController.h"
#include "worker/ShellyWorker.h"
#include "validation/IpValidator.h"

namespace SHELLY {

/**
 * Shelly Device Management Service - Main Facade.
 * 
 * Orchestrates all Shelly-related functionality:
 * - Device CRUD operations (via ShellyDeviceManager)
 * - Async relay control (via ShellyWorker)
 * - Periodic status polling (via ShellyRelayController)
 * 
 * This is a thin facade that delegates to specialized components.
 */
class ShellyService {
public:
    explicit ShellyService(FS& fs, SemaphoreHandle_t networkMutex = nullptr);
    ~ShellyService();

    // ========================================================================
    // Lifecycle
    // ========================================================================

    void begin();
    void stop();
    bool isRunning() const { return _running.load() || _worker.isRunning(); }

    // Load device config without starting worker (for lazy loading check at boot)
    void loadConfig();

    // Start worker if not already running (lazy start on first device add)
    void ensureStarted();

    // ========================================================================
    // Device Management (delegates to ShellyDeviceManager)
    // ========================================================================

    bool getDevice(const char* id, ShellyDevice& out) { return _deviceManager.getDevice(id, out); }
    // Lifecycle note: after the March/April 2026 cleanup this method is the
    // canonical place that persists a device and makes sure the background
    // worker exists. Keeping that coupling here avoids "saved but not started"
    // bugs when callers bypass the REST API and talk to ShellyService directly.
    bool upsertDevice(const ShellyDevice& device);
    // Lifecycle note: removing the last device must also stop the worker so its
    // task, queue, stack, and lazy HTTP resources are reclaimed immediately.
    // The service owns that rule centrally so debugging does not depend on
    // which entry point (REST/API/other internal caller) performed the delete.
    bool removeDevice(const char* id);
    size_t getDeviceCount() { return _deviceManager.getDeviceCount(); }
    
    // Callback pattern to avoid vector copy (for API serialization)
    template<typename Func>
    void forAll(Func callback) { _deviceManager.forAll(callback); }

    // ========================================================================
    // Device Control (delegates to ShellyWorker)
    // ========================================================================

    bool setRelayState(const char* id, bool turnOn);

    // ========================================================================
    // Callbacks
    // ========================================================================
    
    using OnStateChangeCallback = ShellyDeviceManager::OnStateChangeCallback;
    void setOnStateChangeCallback(OnStateChangeCallback cb) { _deviceManager.setOnStateChangeCallback(cb); }

    // ========================================================================
    // Static Utilities
    // ========================================================================

    static bool isValidPrivateIp(const char* ip) {
        return IpValidator::isValidPrivateIp(ip);
    }

private:
    std::atomic<bool> _running{false};
    bool _configLoaded{false};
    SemaphoreHandle_t _lifecycleMutex{nullptr};
    
    // Components (order matters for initialization)
    ShellyDeviceManager _deviceManager;
    ShellyRelayController _relayController;
    ShellyWorker _worker;
};

} // namespace SHELLY
