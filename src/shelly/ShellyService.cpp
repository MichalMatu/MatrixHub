#include "ShellyService.h"
#include "../system/logging/Logging.h"
#include "../system/utils/ScopeLock.h"
#include "../system/rtc/RtcConfig.h"
#include "ShellyPeerRevision.h"

#include <utility>

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

bool ShellyService::ensureStarted() {
    SYSTEM::RecursiveScopeLock lock(_lifecycleMutex, pdMS_TO_TICKS(15000));
    if (lock.isLocked()) {
        if (!_running.load(std::memory_order_acquire) || !_worker.isRunning()) {
            return begin(); // Safe recursive call
        }
        _startRetry.markStarted();
        return true;
    } else {
        LOGW("ensureStarted: mutex timeout");
        _startRetry.schedule(millis());
        return false;
    }
}

bool ShellyService::begin() {
    SYSTEM::RecursiveScopeLock lock(_lifecycleMutex, pdMS_TO_TICKS(15000));
    if (!lock.isLocked()) {
        LOGW("begin: mutex timeout");
        _startRetry.schedule(millis());
        return false;
    }

    if (_running.load(std::memory_order_acquire) && _worker.isRunning()) {
        _startRetry.markStarted();
        return true;
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
        _startRetry.schedule(millis());
        return false;
    }

    _startRetry.markStarted();
    LOGI("Started (devices=%d)", _deviceManager.getDeviceCount());
    return true;
}

void ShellyService::stop() {
    // Explicit stop/remove-last wins over a previously scheduled allocation
    // retry. A later command will schedule or start the runtime again.
    _startRetry.cancel();
    SYSTEM::RecursiveScopeLock lock(_lifecycleMutex, pdMS_TO_TICKS(15000));
    if (!lock.isLocked()) {
         // If destructing and mutex invalid, just skip
        if (!_lifecycleMutex) return;
        LOGW("stop: mutex timeout");
        return;
    }

    if (!_running.load() && !_worker.hasTask()) {
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
    SYSTEM::RecursiveScopeLock lock(_lifecycleMutex, pdMS_TO_TICKS(15000));
    if (!lock.isLocked()) {
        LOGW("upsertDevice: lifecycle mutex timeout");
        return false;
    }

    if (!_deviceManager.upsertDevice(device)) {
        return false;
    }

    if (!device.enabled) {
        // Disabling a peer is reversible. Park the latest logical state so a
        // later re-enable can rebind and converge without a new alarm edge.
        _worker.parkDevice(device.id, shellyPeerRevision(device));
    } else {
        // Always rebind after the atomic manager upsert. This is a no-op for a
        // new device, a same-peer edit, or a device without retained intent,
        // and closes the get-before-upsert race where a timed-out snapshot
        // could otherwise miss a real peer change.
        _worker.rebindDevice(device.id, shellyPeerRevision(device));
    }

    // Keep start logic next to persistence so every caller gets the same lazy
    // boot behavior. This was moved out of ShellyApiService to make "device was
    // saved but worker never started" easier to reason about in logs/debugging.
    (void)ensureStarted();
    OnConfigChangeCallback configChange = _onConfigChange;
    lock.unlock();
    if (configChange) {
        configChange();
    }
    return true;
}

bool ShellyService::removeDevice(const char* id) {
    SYSTEM::RecursiveScopeLock lock(_lifecycleMutex, pdMS_TO_TICKS(15000));
    if (!lock.isLocked()) {
        LOGW("removeDevice: lifecycle mutex timeout");
        return false;
    }

    if (!_deviceManager.removeDevice(id)) {
        return false;
    }

    _worker.forgetDevice(id);

    // If this was the last configured Shelly, shut the whole runtime down here
    // instead of in the API layer. That guarantees task/resource cleanup even
    // for non-HTTP call paths and gives one place to inspect during debugging.
    if (_deviceManager.getDeviceCount() == 0) {
        stop();
    }

    OnConfigChangeCallback configChange = _onConfigChange;
    lock.unlock();
    if (configChange) {
        configChange();
    }
    return true;
}

void ShellyService::setOnConfigChangeCallback(OnConfigChangeCallback cb) {
    SYSTEM::RecursiveScopeLock lock(_lifecycleMutex, pdMS_TO_TICKS(15000));
    if (!lock.isLocked()) {
        LOGW("setOnConfigChangeCallback: lifecycle mutex timeout");
        return;
    }
    _onConfigChange = std::move(cb);
}

bool ShellyService::setRelayState(const char* id, bool turnOn) {
    return admitRelayState(id, turnOn, pdMS_TO_TICKS(15000), false) ==
           ShellyRelayAdmissionResult::Accepted;
}

ShellyRelayAdmissionResult ShellyService::trySetAlarmRelayState(
    const char* id,
    bool turnOn) {
    constexpr TickType_t kAlarmAdmissionTimeout = pdMS_TO_TICKS(100);
    return admitRelayState(id, turnOn, kAlarmAdmissionTimeout, true);
}

ShellyRelayAdmissionResult ShellyService::admitRelayState(
    const char* id,
    bool turnOn,
    TickType_t mutexTimeout,
    bool parkDisabled) {
    SYSTEM::RecursiveScopeLock lock(_lifecycleMutex, mutexTimeout);
    if (!lock.isLocked()) {
        LOGW("setRelayState: lifecycle mutex timeout");
        return ShellyRelayAdmissionResult::Retry;
    }

    ShellyDevice device;
    const ShellyDeviceLookupResult lookup =
        _deviceManager.lookupDevice(id, device, mutexTimeout);
    if (lookup == ShellyDeviceLookupResult::Busy) {
        LOGW("setRelayState: device manager busy for %s", id ? id : "<null>");
        return ShellyRelayAdmissionResult::Retry;
    }
    if (lookup != ShellyDeviceLookupResult::Found) {
        LOGW("Device not found: %s", id);
        return ShellyRelayAdmissionResult::Terminal;
    }

    if (!device.enabled) {
        if (!parkDisabled) {
            LOGW("Device disabled: %s", id);
            return ShellyRelayAdmissionResult::Terminal;
        }
        // A disabled configured peer is terminal for immediate transport, but
        // its latest alarm intent is accepted in a parked ledger slot. Enabling
        // the peer rebinds this value and makes it pending automatically.
        if (!_worker.parkCommand(
                id,
                turnOn,
                shellyPeerRevision(device),
                mutexTimeout)) {
            return ShellyRelayAdmissionResult::Retry;
        }
        LOGD("Parked desired state for disabled device: %s", id);
        return ShellyRelayAdmissionResult::Accepted;
    }

    // Store intent before starting the runtime. The fixed ledger survives a
    // worker stop/restart and is processed even if its wake hint was coalesced.
    if (!_worker.queueCommand(
            id,
            turnOn,
            shellyPeerRevision(device),
            mutexTimeout)) {
        return ShellyRelayAdmissionResult::Retry;
    }
    if (!ensureStarted()) {
        // The command is already durable for this runtime session in the
        // fixed ledger. Return success to the alarm outbox, while the main-loop
        // retry schedule recreates the worker without requiring a new edge.
        LOGW("Desired state retained while Shelly worker start is pending");
    }
    return ShellyRelayAdmissionResult::Accepted;
}

void ShellyService::reconcileRuntimeIfDue(uint32_t nowMs) {
    if (!_startRetry.claimIfDue(nowMs)) {
        return;
    }

    LOGI("Retrying Shelly worker start with retained desired state");
    (void)ensureStarted();
}

} // namespace SHELLY
