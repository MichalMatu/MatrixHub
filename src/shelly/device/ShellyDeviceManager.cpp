/**
 * @file ShellyDeviceManager.cpp
 * @brief Ultra-thin facade implementation (Phase 3.3)
 * 
 * All operations delegate to:
 *  - ShellyDeviceStore: config store + persistence
 *  - ShellyDeviceValidator: Device validation
 */

#include "ShellyDeviceManager.h"
#include "../../system/logging/Logging.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../config/App.h"
#include "../../system/utils/ScopeLock.h"
#include "../ShellyPeerRevision.h"

#undef LOG_TAG
#define LOG_TAG "ShellyDev"

namespace SHELLY {

namespace {
bool isValidGeneration(uint8_t generation) {
    return generation == 1 || generation == 2;
}
}

ShellyDeviceManager::ShellyDeviceManager(FS& fs)
    : _store(fs), _mutex(nullptr) {
    
    _mutex = xSemaphoreCreateMutex();
    if (!_mutex) {
        LOGE("Failed to create mutex");
    }
}

ShellyDeviceManager::~ShellyDeviceManager() {
    if (_mutex) {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }
}

bool ShellyDeviceManager::loadFromStorage() {
    // Delegate to store
    uint8_t count = _store.load();
    LOGI("Loaded %d devices", count);

    // Migration/Sanitization: Ensure generation is valid (default to 2)
    // This handles struct expansion where new field might be 0
    if (count > 0) {
        SYSTEM::ScopeLock lock(_mutex, TIMEOUT::MUTEX_FS_TICKS);
        if (lock.isLocked()) {
            bool changed = false;
            RTC::ShellyData before = _store.snapshot();
            for (uint8_t i = 0; i < count; i++) {
                 ShellyDevice& dev = _store.getDeviceAt(i);
                 if (!isValidGeneration(dev.generation)) {
                     LOGW("Migrating device %s: Gen %u -> 2", dev.id, dev.generation);
                     dev.generation = 2;
                     changed = true;
                 }
            }
            
            if (changed) {
                if (!_store.commit()) {
                    LOGE("Failed to commit Shelly generation migration to RTC");
                    return false;
                }
            }

            if (changed && !saveLockedWithRollback(before)) {
                LOGE("Failed to persist Shelly generation migration");
                return false;
            }
        }
    }

    return true;
}

void ShellyDeviceManager::setOnStateChangeCallback(OnStateChangeCallback cb) {
    SYSTEM::ScopeLock lock(_mutex, kMutexTimeout);
    if (!lock.isLocked()) {
        LOGW("setOnStateChangeCallback: mutex timeout");
        return;
    }

    _onStateChange = std::move(cb);
}

ShellyDeviceLookupResult ShellyDeviceManager::lookupDevice(
    const char* id,
    ShellyDevice& deviceOut,
    TickType_t timeout) {
    SYSTEM::ScopeLock lock(_mutex, timeout);
    if (!lock.isLocked()) {
        LOGW("lookupDevice: mutex timeout");
        return ShellyDeviceLookupResult::Busy;
    }

    ShellyDevice* dev = _store.findDevice(id);
    if (!dev) {
        return ShellyDeviceLookupResult::NotFound;
    }

    deviceOut = *dev;
    return ShellyDeviceLookupResult::Found;
}

bool ShellyDeviceManager::getDevice(const char* id, ShellyDevice& deviceOut) {
    return lookupDevice(id, deviceOut) == ShellyDeviceLookupResult::Found;
}

bool ShellyDeviceManager::upsertDevice(const ShellyDevice& device) {
    // Validate before acquiring mutex
    if (!ShellyDeviceValidator::isValid(device)) {
        LOGW("Invalid device: missing id or ip");
        return false;
    }
    
    if (!ShellyDeviceValidator::isValidIp(device.ip)) {
        LOGW("Invalid IP: %s", device.ip);
        return false;
    }

    bool result = false;
    SYSTEM::ScopeLock lock(_mutex, TIMEOUT::MUTEX_FS_TICKS);
    if (lock.isLocked()) {
        RTC::ShellyData before = _store.snapshot();
        ShellyDevice* existing = _store.findDevice(device.id);
        
        if (existing) {
            // Update existing device
            result = _store.updateDevice(device.id, device);
        } else {
            // Add new device
            result = _store.addDevice(device);
        }
        
        if (result && !saveLockedWithRollback(before)) {
            LOGE("upsertDevice: failed to persist Shelly config");
            result = false;
        }
    } else {
        LOGW("upsertDevice: mutex timeout");
    }
    
    return result;
}

bool ShellyDeviceManager::removeDevice(const char* id) {
    bool result = false;
    SYSTEM::ScopeLock lock(_mutex, TIMEOUT::MUTEX_FS_TICKS);
    if (lock.isLocked()) {
        RTC::ShellyData before = _store.snapshot();
        result = _store.removeDevice(id);
        
        if (result && !saveLockedWithRollback(before)) {
            LOGE("removeDevice: failed to persist Shelly config");
            result = false;
        }
    } else {
        LOGW("removeDevice: mutex timeout");
    }
    return result;
}

size_t ShellyDeviceManager::getDeviceCount() {
    // Lock-free read of the local Shelly snapshot. Mutations happen under _mutex.
    return _store.getCount();
}

bool ShellyDeviceManager::getDeviceByIndex(size_t index, ShellyDevice& deviceOut) {
    bool found = false;
    SYSTEM::ScopeLock lock(_mutex, TIMEOUT::MUTEX_FS_TICKS);
    if (lock.isLocked()) {
        if (index < _store.getCount()) {
            deviceOut = _store.getDeviceAt(index);
            found = true;
        }
    } else {
        LOGW("getDeviceByIndex: mutex timeout");
    }
    return found;
}

void ShellyDeviceManager::applyPollHealth(ShellyDevice& dev, bool isOnline, bool& changed) {
    if (isOnline) {
        const bool wasOnline = dev.isOnline;
        dev.failedPolls = 0;
        dev.pollBackoff = 1;
        dev.isOnline = true;
        if (!wasOnline) {
            changed = true;
        }
        return;
    }

    if (dev.failedPolls < 255) {
        dev.failedPolls++;
    }
    if (dev.failedPolls >= 3) {
        if (dev.isOnline) {
            dev.isOnline = false;
            changed = true;
        }
        if (dev.pollBackoff < 30) {
            dev.pollBackoff = static_cast<uint8_t>(dev.pollBackoff * 2);
            if (dev.pollBackoff > 30) {
                dev.pollBackoff = 30;
            }
        }
    }
}

void ShellyDeviceManager::applyPowerDebounce(ShellyDevice& dev,
                                             ShellyStatus& status) {
    if (!status.hasPower) {
        return;
    }

    // Some Shelly firmwares intermittently publish 0 W while the relay is ON.
    // Keep the debounce counter and the accepted sample in the same peer-fenced
    // transaction as the rest of the poll commit. A slow response from an old
    // IP/relay must not mutate the replacement peer's runtime fields.
    constexpr uint8_t kZeroPowerThreshold = 3;
    if (status.isOn && status.power == 0.0f && dev.power > 0.0f) {
        const uint8_t count = static_cast<uint8_t>(dev.zeroPowerCount + 1);
        if (count < kZeroPowerThreshold) {
            LOGW("Shelly %s: 0W while ON (%u/%u), keeping %.1fW",
                 dev.id, count, kZeroPowerThreshold, dev.power);
            status.power = dev.power;
            dev.zeroPowerCount = count;
        } else {
            LOGI("Shelly %s: 0W confirmed after %u readings, accepting",
                 dev.id, count);
            dev.zeroPowerCount = 0;
        }
        return;
    }

    if (status.power > 0.0f) {
        dev.zeroPowerCount = 0;
    }
}

bool ShellyDeviceManager::updateCommandState(const char* id,
                                             bool isOn,
                                             bool isOnline,
                                             const ShellyDevice* expectedPeer) {
    bool changed = false;
    bool shouldNotify = false;
    ShellyDevice snapshot = {};
    OnStateChangeCallback callbackCopy = nullptr;

    SYSTEM::ScopeLock lock(_mutex, TIMEOUT::MUTEX_FS_TICKS);
    if (!lock.isLocked()) {
        LOGW("updateCommandState: mutex timeout");
        return false;
    }

    ShellyDevice* dev = _store.findDevice(id);
    if (!dev) {
        return false;
    }
    if (expectedPeer && !shellyPeerMatches(*dev, *expectedPeer)) {
        LOGW("Ignoring stale command ACK for reconfigured peer %s", id);
        return false;
    }

    if (isOnline) {
        changed = (dev->isOn != isOn) || !dev->isOnline;
        dev->isOn = isOn;
        dev->isOnline = true;
        dev->failedPolls = 0;
        dev->pollBackoff = 1;
        dev->lastUpdate = millis();
        if (!_store.commit()) {
            LOGW("updateCommandState: failed to commit Shelly state to RTC");
            return false;
        }
    }

    if (changed && _onStateChange) {
        snapshot = *dev;
        callbackCopy = _onStateChange;
        shouldNotify = true;
    }

    lock.unlock();
    if (shouldNotify && callbackCopy) {
        callbackCopy(snapshot);
    }
    return changed;
}

bool ShellyDeviceManager::updateGenerationIfPeerMatches(
    const ShellyDevice& expectedPeer,
    uint8_t generation) {
    if (!isValidGeneration(generation)) {
        return false;
    }

    SYSTEM::ScopeLock lock(_mutex, TIMEOUT::MUTEX_FS_TICKS);
    if (!lock.isLocked()) {
        LOGW("updateGenerationIfPeerMatches: mutex timeout");
        return false;
    }

    ShellyDevice* current = _store.findDevice(expectedPeer.id);
    if (!current || !shellyPeerMatches(*current, expectedPeer)) {
        LOGW("Skipping stale generation update for %s", expectedPeer.id);
        return false;
    }
    if (current->generation == generation) {
        return true;
    }

    const RTC::ShellyData before = _store.snapshot();
    ShellyDevice corrected = *current;
    corrected.generation = generation;
    if (!_store.updateDevice(current->id, corrected)) {
        return false;
    }
    if (!saveLockedWithRollback(before)) {
        LOGE("Failed to persist corrected Shelly generation for %s", expectedPeer.id);
        return false;
    }
    return true;
}

bool ShellyDeviceManager::updatePollState(const ShellyDevice& expectedPeer,
                                          const ShellyStatus& status,
                                          bool isOnline) {
    bool changed = false;
    bool shouldNotify = false;
    ShellyDevice snapshot = {};
    OnStateChangeCallback callbackCopy = nullptr;
    SYSTEM::ScopeLock lock(_mutex, TIMEOUT::MUTEX_FS_TICKS);
    if (lock.isLocked()) {
        ShellyDevice* dev = _store.findDevice(expectedPeer.id);
        if (dev) {
            if (!shellyPeerMatches(*dev, expectedPeer)) {
                LOGW("Ignoring stale poll result for reconfigured peer %s",
                     expectedPeer.id);
                return false;
            }

            applyPollHealth(*dev, isOnline, changed);

            // 2. Update data ONLY if this specific poll was successful (isOnline input)
            // If poll failed, we keep the old data (even if we mark it offline or keep it online)
            if (isOnline) {
                ShellyStatus acceptedStatus = status;
                applyPowerDebounce(*dev, acceptedStatus);
                bool dataChanged = (dev->isOn != acceptedStatus.isOn);
                
                // Check all metering fields when hasPower is set
                if (acceptedStatus.hasPower) {
                    dataChanged = dataChanged ||
                                  (dev->power != acceptedStatus.power) ||
                                  (dev->energy != acceptedStatus.energy) ||
                                  (dev->voltage != acceptedStatus.voltage) ||
                                  (dev->current != acceptedStatus.current) ||
                                  (dev->temperature != acceptedStatus.temperature) ||
                                  (dev->rssi != acceptedStatus.rssi);
                }

                if (dataChanged) {
                    dev->isOn = acceptedStatus.isOn;
                    
                    if (acceptedStatus.hasPower) {
                        dev->power = acceptedStatus.power;
                        dev->energy = acceptedStatus.energy;
                        dev->voltage = acceptedStatus.voltage;
                        dev->current = acceptedStatus.current;
                        dev->temperature = acceptedStatus.temperature;
                        dev->rssi = acceptedStatus.rssi;
                    }
                    changed = true;
                }
                dev->lastUpdate = millis();
            }
            if (!_store.commit()) {
                LOGW("updatePollState: failed to commit Shelly state to RTC");
                return false;
            }
            if (changed && _onStateChange) {
                snapshot = *dev;
                callbackCopy = _onStateChange;
                shouldNotify = true;
            }
        }
    } else {
        LOGW("updatePollState: mutex timeout");
    }
    lock.unlock();
    if (shouldNotify && callbackCopy) {
        callbackCopy(snapshot);
    }
    return changed;
}

bool ShellyDeviceManager::saveLockedWithRollback(const RTC::ShellyData& snapshot) {
    if (_store.save()) {
        return true;
    }

    if (!_store.restore(snapshot)) {
        LOGE("Failed to restore Shelly RTC snapshot after save error");
    }
    return false;
}

} // namespace SHELLY
