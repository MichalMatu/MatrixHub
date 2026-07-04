#include "ShellyService.h"
#include "../system/logging/Logging.h"
#include "../system/utils/ScopeLock.h"
#include "../system/rtc/RtcConfig.h"

#undef LOG_TAG
#define LOG_TAG "Shelly"

namespace SHELLY {

ShellyService::ShellyService(FS& fs, SemaphoreHandle_t networkMutex)
    : _running(false)
    , _configLoaded(false)
    , _deviceManager(fs)
    , _relayController(_deviceManager, _running, networkMutex)
    , _worker(_deviceManager, _relayController, _running) {
    
    _lifecycleMutex = xSemaphoreCreateRecursiveMutex();
    if (!_lifecycleMutex) {
        LOGE("Failed to create lifecycle mutex");
    }
}

ShellyService::~ShellyService() {
    stop();
    if (_lifecycleMutex) {
        vSemaphoreDelete(_lifecycleMutex);
        _lifecycleMutex = nullptr;
    }
}

void ShellyService::loadConfig() {
    _deviceManager.loadFromStorage();
    _configLoaded = true;
}

void ShellyService::ensureStarted() {
    SYSTEM::RecursiveScopeLock lock(_lifecycleMutex, pdMS_TO_TICKS(15000));
    if (lock.isLocked()) {
        if (!isRunning()) {
            begin(); // Safe recursive call
        }
    } else {
        LOGW("ensureStarted: mutex timeout");
    }
}

void ShellyService::begin() {
    SYSTEM::RecursiveScopeLock lock(_lifecycleMutex, pdMS_TO_TICKS(15000));
    if (!lock.isLocked()) {
        LOGW("begin: mutex timeout");
        return;
    }

    if (isRunning()) {
        LOGW("Already running");
        return;
    }



    // Load configuration if not already loaded via loadConfig()
    if (!_configLoaded) {
        _deviceManager.loadFromStorage();
        _configLoaded = true;
    }

    // Start worker
    _running.store(true);
    if (!_worker.start()) {
        LOGE("Failed to start worker");
        _running.store(false);
        return;
    }

    LOGI("Started (devices=%d)", _deviceManager.getDeviceCount());
}

void ShellyService::stop() {
    SYSTEM::RecursiveScopeLock lock(_lifecycleMutex, pdMS_TO_TICKS(15000));
    if (!lock.isLocked()) {
         // If destructing and mutex invalid, just skip
        if (!_lifecycleMutex) return;
        LOGW("stop: mutex timeout");
        return;
    }

    if (!_running.load() && !_worker.isRunning()) {
        return;
    }

    LOGI("Stopping...");
    _running.store(false);
    if (!_worker.stop()) {
        LOGW("Shelly worker is still stopping asynchronously");
        return;
    }
    _relayController.releaseResources();
    LOGI("Stopped");
}

bool ShellyService::upsertDevice(const ShellyDevice& device) {
    if (!_deviceManager.upsertDevice(device)) {
        return false;
    }

    // Keep start logic next to persistence so every caller gets the same lazy
    // boot behavior. This was moved out of ShellyApiService to make "device was
    // saved but worker never started" easier to reason about in logs/debugging.
    ensureStarted();
    return true;
}

bool ShellyService::removeDevice(const char* id) {
    if (!_deviceManager.removeDevice(id)) {
        return false;
    }

    // If this was the last configured Shelly, shut the whole runtime down here
    // instead of in the API layer. That guarantees task/resource cleanup even
    // for non-HTTP call paths and gives one place to inspect during debugging.
    if (_deviceManager.getDeviceCount() == 0) {
        stop();
    }

    return true;
}

bool ShellyService::setRelayState(const char* id, bool turnOn) {
    // Check if device exists
    ShellyDevice device;
    if (!_deviceManager.getDevice(id, device)) {
        LOGW("Device not found: %s", id);
        return false;
    }

    // Queue command
    // Note: queueCommand copies the ID string by value into the queue, 
    // so using valid reference here is safe even if original string is destroyed.
    return _worker.queueCommand(id, turnOn);
}

} // namespace SHELLY
