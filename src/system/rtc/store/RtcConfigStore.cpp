/**
 * @file RtcConfigStore.cpp
 * @brief RTC config store bootstrap, access, and locking
 */

#include "../RtcConfigInternal.h"

#include "../../../config/App.h"
#include "../../logging/Logging.h"
#include "../../memory/SystemAllocator.h"
#include "../../utils/ScopeLock.h"

#include <esp_attr.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <cstdlib>
#include <cstring>
#include <type_traits>

#undef LOG_TAG
#define LOG_TAG "RtcConfig"

namespace RTC {

ConfigStore* store = nullptr;

RTC_DATA_ATTR RtcSensorState sensorState;
RTC_DATA_ATTR RtcHeapHistory heapHistory;
RTC_DATA_ATTR RtcRuntimeStats runtimeStats;
RTC_DATA_ATTR RtcNetworkState networkState;

namespace detail {
namespace {

SemaphoreHandle_t& configLockHandle() {
    static SemaphoreHandle_t lock = nullptr;
    return lock;
}

}  // namespace

TickType_t configLockTimeoutTicks() {
    return xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED
               ? 0
               : pdMS_TO_TICKS(100);
}

TickType_t sleepSnapshotLockTimeoutTicks() {
    return xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED
               ? 0
               : pdMS_TO_TICKS(SHUTDOWN::RTC_BACKUP_LOCK_TIMEOUT_MS);
}

ConfigStore& requireStore() {
    if (!store) {
        init();
    }
    if (!store) {
        LOGE("RTC ConfigStore is unavailable (PSRAM allocation failed)");
        std::abort();
    }
    return *store;
}

}  // namespace detail

void init() {
    if (store) {
        return;
    }

    static_assert(std::is_trivially_copyable<ConfigStore>::value,
                  "ConfigStore must remain trivially copyable for RTC shadow memcpy");

    store = SYSTEM::MEMORY::allocInPsram<ConfigStore>();
    if (!store) {
        LOGE("Failed to allocate RTC ConfigStore in PSRAM");
        return;
    }

    if (!detail::restoreConfigFromBackup(*store)) {
        memset(store, 0, sizeof(ConfigStore));
    }
}

const ConfigStore& getConfig() {
    return detail::requireStore();
}

ConfigStore& getMutableConfig() {
    return detail::requireStore();
}

ConfigStore getConfigSafeCopy() {
    ConfigStore& cfg = detail::requireStore();
    SemaphoreHandle_t lockHandle = detail::configLockHandle();
    if (!lockHandle) {
        LOGE("getConfigSafeCopy: config lock not initialized, returning unlocked copy");
        return cfg;
    }

    const uint32_t waitStartMs = millis();
    SYSTEM::ScopeLock lock(lockHandle, detail::configLockTimeoutTicks());
    if (!lock.isLocked()) {
        const uint32_t waitMs = millis() - waitStartMs;
        LOGW("getConfigSafeCopy: lock timeout after %lu ms, returning unlocked copy",
             static_cast<unsigned long>(waitMs));
        return cfg;
    }
    return cfg;
}

SemaphoreHandle_t getLock() {
    return detail::configLockHandle();
}

void withConfig(const std::function<void(const ConfigStore&)>& reader) {
    ConfigStore& cfg = detail::requireStore();
    SemaphoreHandle_t lockHandle = detail::configLockHandle();
    if (!lockHandle) {
        LOGE("withConfig: config lock not initialized, skipping reader callback");
        return;
    }

    const uint32_t waitStartMs = millis();
    SYSTEM::ScopeLock lock(lockHandle, detail::configLockTimeoutTicks());
    const uint32_t waitMs = millis() - waitStartMs;
    static uint32_t lastSlowReadLockLogMs = 0;

    if (!lock.isLocked()) {
        LOGW("withConfig: lock timeout after %lu ms, skipping reader callback",
             static_cast<unsigned long>(waitMs));
        return;
    }

    if (waitMs >= TASK_MONITOR::THRESHOLD_RTC_CONFIG_READ_LOCK_MS &&
        (lastSlowReadLockLogMs == 0 ||
         millis() - lastSlowReadLockLogMs >= TASK_MONITOR::SLOW_LOG_THROTTLE_MS)) {
        lastSlowReadLockLogMs = millis();
        LOGW("withConfig: lock wait slow: %lu ms", static_cast<unsigned long>(waitMs));
    }

    reader(cfg);
}

void createLock() {
    SemaphoreHandle_t& lockHandle = detail::configLockHandle();
    if (!lockHandle) {
        lockHandle = xSemaphoreCreateMutex();
        if (!lockHandle) {
            LOGE("createLock: failed to create config mutex");
            std::abort();
        }
    }
}

bool updateConfig(const std::function<void(ConfigStore&)>& updater) {
    ConfigStore& cfg = detail::requireStore();
    SemaphoreHandle_t lockHandle = detail::configLockHandle();
    if (!lockHandle) {
        return false;
    }

    SYSTEM::ScopeLock lock(lockHandle, detail::configLockTimeoutTicks());
    if (!lock.isLocked()) {
        return false;
    }

    updater(cfg);
    detail::refreshConfigIntegrity(cfg);
    return true;
}

void updateConfigLocked(const std::function<void(ConfigStore&)>& updater) {
    ConfigStore& cfg = detail::requireStore();
    updater(cfg);
    detail::refreshConfigIntegrity(cfg);
}

}  // namespace RTC
