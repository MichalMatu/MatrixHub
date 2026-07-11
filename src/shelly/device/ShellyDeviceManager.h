#pragma once

#include <vector>
#include <functional>
#include <utility>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <FS.h>
#include "../ShellyTypes.h"
#include "ShellyDeviceStore.h"
#include "ShellyDeviceValidator.h"
#include "../../system/utils/ScopeLock.h"

namespace SHELLY {

enum class ShellyDeviceLookupResult : uint8_t {
    Found,
    NotFound,
    Busy,
};

namespace {
    constexpr TickType_t kMutexTimeout = pdMS_TO_TICKS(5000);  // 5s timeout (was portMAX_DELAY)
}

/**
 * @brief Ultra-thin facade for Shelly device management (Phase 3.3).
 * 
 * Thread-safe CRUD operations delegating to:
 *  - ShellyDeviceStore: config store + persistence
 *  - ShellyDeviceValidator: Validation logic
 */
class ShellyDeviceManager {
public:
    explicit ShellyDeviceManager(FS& fs);
    ~ShellyDeviceManager();

    // Lifecycle
    bool loadFromStorage();

    // CRUD operations (thread-safe)
    ShellyDeviceLookupResult lookupDevice(
        const char* id,
        ShellyDevice& deviceOut,
        TickType_t timeout = TIMEOUT::MUTEX_FS_TICKS);
    bool getDevice(const char* id, ShellyDevice& deviceOut);
    bool upsertDevice(const ShellyDevice& device);
    bool removeDevice(const char* id);
    size_t getDeviceCount();
    bool getDeviceByIndex(size_t index, ShellyDevice& deviceOut);

    // State updates
    bool updateCommandState(const char* id,
                            bool isOn,
                            bool isOnline,
                            const ShellyDevice* expectedPeer = nullptr);
    bool updatePollState(const ShellyDevice& expectedPeer,
                         const ShellyStatus& status,
                         bool isOnline);

    /**
     * Persist protocol auto-detection only if the command/poll snapshot still
     * identifies the current peer. Prevents a slow old request from overwriting
     * a concurrent IP/relay/configuration update.
     */
    bool updateGenerationIfPeerMatches(const ShellyDevice& expectedPeer,
                                       uint8_t generation);

    // Callbacks
    using OnStateChangeCallback = std::function<void(const ShellyDevice&)>;
    void setOnStateChangeCallback(OnStateChangeCallback cb);

    // Template iteration methods (lock-free callback execution)

    template<typename Func>
    bool withDevice(const char* id, Func callback) {
        SYSTEM::ScopeLock lock(_mutex, kMutexTimeout);
        if (lock.isLocked()) {
            ShellyDevice* dev = _store.findDevice(id);
            bool result = false;
            if (dev) {
                result = callback(*dev);
            }
            return result;
        }
        return false;
    }

    template<typename Func>
    void forEachEnabled(Func callback) {
        SYSTEM::ScopeLock lock(_mutex, kMutexTimeout);
        if (lock.isLocked()) {
            for (uint8_t i = 0; i < _store.getCount(); i++) {
                if (_store.getDeviceAt(i).enabled) {
                    callback(_store.getDeviceAt(i));
                }
            }
        }
    }

    template<typename Func>
    void forAll(Func callback) {
        SYSTEM::ScopeLock lock(_mutex, kMutexTimeout);
        if (lock.isLocked()) {
            for (uint8_t i = 0; i < _store.getCount(); i++) {
                callback(_store.getDeviceAt(i));
            }
        }
    }

private:
    bool saveLockedWithRollback(const RTC::ShellyData& snapshot);
    static void applyPollHealth(ShellyDevice& dev, bool isOnline, bool& changed);
    static void applyPowerDebounce(ShellyDevice& dev, ShellyStatus& status);

    ShellyDeviceStore _store;
    SemaphoreHandle_t _mutex;
    OnStateChangeCallback _onStateChange = nullptr;
};

} // namespace SHELLY
