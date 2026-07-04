#include "GpioConfigStore.h"

#include <cstdlib>
#include <new>

#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../system/logging/Logging.h"
#include "../system/memory/SystemAllocator.h"
#include "../system/rtc/RtcConfig.h"
#include "../system/utils/ScopeLock.h"
#include "GpioSafePins.h"

#undef LOG_TAG
#define LOG_TAG "GpioCfg"

namespace GPIO {
namespace CONFIG_STORE {
namespace {

GpioData* s_store = nullptr;
bool s_loggedFallback = false;

TickType_t configLockTimeoutTicks() {
    return xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED
               ? 0
               : pdMS_TO_TICKS(100);
}

GpioData& requireStore() {
    if (!s_store) {
        GpioData* psramStore = SYSTEM::MEMORY::allocInPsram<GpioData>();
        if (psramStore) {
            applyDefaultConfig(*psramStore);
            s_store = psramStore;
        } else {
            void* mem = heap_caps_malloc(sizeof(GpioData), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (mem) {
                s_store = new(mem) GpioData();
                applyDefaultConfig(*s_store);
                if (!s_loggedFallback) {
                    LOGW("GPIO config store PSRAM allocation failed; using internal heap fallback");
                    s_loggedFallback = true;
                }
            } else {
                LOGE("GPIO config store allocation failed entirely");
                std::abort();
            }
        }
    }
    return *s_store;
}

}  // namespace

GpioData copy() {
    GpioData snapshot{};
    withConfig([&](const GpioData& cfg) {
        snapshot = cfg;
    });
    return snapshot;
}

void withConfig(const std::function<void(const GpioData&)>& reader) {
    GpioData& cfg = requireStore();
    SemaphoreHandle_t lock = RTC::getLock();
    if (!lock) {
        LOGW("withConfig: RTC lock not initialized, returning unlocked GPIO config");
        reader(cfg);
        return;
    }

    SYSTEM::ScopeLock guard(lock, configLockTimeoutTicks());
    if (!guard.isLocked()) {
        LOGW("withConfig: GPIO config lock timeout");
        return;
    }

    reader(cfg);
}

bool update(const std::function<void(GpioData&)>& updater) {
    GpioData& cfg = requireStore();
    SemaphoreHandle_t lock = RTC::getLock();
    if (!lock) {
        LOGE("update: RTC lock not initialized, skipping GPIO config update");
        return false;
    }

    SYSTEM::ScopeLock guard(lock, configLockTimeoutTicks());
    if (!guard.isLocked()) {
        LOGW("update: GPIO config lock timeout");
        return false;
    }

    updater(cfg);
    normalizeConfig(cfg);
    return true;
}

bool resetToDefaults() {
    return update([](GpioData& cfg) {
        applyDefaultConfig(cfg);
    });
}

}  // namespace CONFIG_STORE
}  // namespace GPIO

