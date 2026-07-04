#include "ShellyConfigStore.h"

#include <cstdlib>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../system/logging/Logging.h"
#include <new>
#include <esp_heap_caps.h>
#include "../system/memory/SystemAllocator.h"
#include "../system/rtc/RtcConfig.h"
#include "../system/utils/ScopeLock.h"

#undef LOG_TAG
#define LOG_TAG "ShellyCfg"

namespace SHELLY {
namespace CONFIG_STORE {
namespace {

RTC::ShellyData* s_store = nullptr;
bool s_loggedFallback = false;

TickType_t configLockTimeoutTicks() {
    return xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED
               ? 0
               : pdMS_TO_TICKS(100);
}

RTC::ShellyData& requireStore() {
    if (!s_store) {
        RTC::ShellyData* psramStore = SYSTEM::MEMORY::allocInPsram<RTC::ShellyData>();
        if (psramStore) {
            *psramStore = RTC::ShellyData{};
            s_store = psramStore;
        } else {
            // Use internal heap only as a real fallback after PSRAM failure.
            // Keeping a static DRAM fallback store here would permanently pin a
            // fairly large Shelly config structure in internal RAM.
            void* mem = heap_caps_malloc(sizeof(RTC::ShellyData), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (mem) {
                s_store = new(mem) RTC::ShellyData();
                if (!s_loggedFallback) {
                    LOGW("Shelly config store PSRAM allocation failed; using internal heap fallback");
                    s_loggedFallback = true;
                }
            } else {
                LOGE("Shelly config store allocation failed entirely (PSRAM and Internal)");
                std::abort();
            }
        }
    }
    return *s_store;
}

}  // namespace

RTC::ShellyData copy() {
    RTC::ShellyData snapshot{};
    withConfig([&](const RTC::ShellyData& cfg) {
        snapshot = cfg;
    });
    return snapshot;
}

void withConfig(const std::function<void(const RTC::ShellyData&)>& reader) {
    RTC::ShellyData& cfg = requireStore();
    SemaphoreHandle_t lock = RTC::getLock();
    if (!lock) {
        LOGW("withConfig: RTC lock not initialized, returning unlocked Shelly config");
        reader(cfg);
        return;
    }

    SYSTEM::ScopeLock guard(lock, configLockTimeoutTicks());
    if (!guard.isLocked()) {
        LOGW("withConfig: Shelly config lock timeout");
        return;
    }

    reader(cfg);
}

bool update(const std::function<void(RTC::ShellyData&)>& updater) {
    RTC::ShellyData& cfg = requireStore();
    SemaphoreHandle_t lock = RTC::getLock();
    if (!lock) {
        LOGE("update: RTC lock not initialized, skipping Shelly config update");
        return false;
    }

    SYSTEM::ScopeLock guard(lock, configLockTimeoutTicks());
    if (!guard.isLocked()) {
        LOGW("update: Shelly config lock timeout");
        return false;
    }

    updater(cfg);
    return true;
}

}  // namespace CONFIG_STORE
}  // namespace SHELLY
