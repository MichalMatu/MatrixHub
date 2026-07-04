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

bool ShellyDeviceManager::getDevice(const char* id, ShellyDevice& deviceOut) {
    bool found = false;
    SYSTEM::ScopeLock lock(_mutex, TIMEOUT::MUTEX_FS_TICKS);
    if (lock.isLocked()) {
        ShellyDevice* dev = _store.findDevice(id);
        if (dev) {
            deviceOut = *dev;
            found = true;
        }
    } else {
        LOGW("getDevice: mutex timeout");
    }
    return found;
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

bool ShellyDeviceManager::updateCommandState(const char* id, bool isOn, bool isOnline) {
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

bool ShellyDeviceManager::updatePollState(const char* id, const ShellyStatus& status, bool isOnline) {
    bool changed = false;
    bool shouldNotify = false;
    ShellyDevice snapshot = {};
    OnStateChangeCallback callbackCopy = nullptr;
    SYSTEM::ScopeLock lock(_mutex, TIMEOUT::MUTEX_FS_TICKS);
    if (lock.isLocked()) {
        ShellyDevice* dev = _store.findDevice(id);
        if (dev) {
            applyPollHealth(*dev, isOnline, changed);

            // 2. Update data ONLY if this specific poll was successful (isOnline input)
            // If poll failed, we keep the old data (even if we mark it offline or keep it online)
            if (isOnline) {
                bool dataChanged = (dev->isOn != status.isOn);
                
                // Check all metering fields when hasPower is set
                if (status.hasPower) {
                    dataChanged = dataChanged ||
                                  (dev->power != status.power) ||
                                  (dev->energy != status.energy) ||
                                  (dev->voltage != status.voltage) ||
                                  (dev->current != status.current) ||
                                  (dev->temperature != status.temperature) ||
                                  (dev->rssi != status.rssi);
                }

                if (dataChanged) {
                    dev->isOn = status.isOn;
                    
                    if (status.hasPower) {
                        dev->power = status.power;
                        dev->energy = status.energy;
                        dev->voltage = status.voltage;
                        dev->current = status.current;
                        dev->temperature = status.temperature;
                        dev->rssi = status.rssi;
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
