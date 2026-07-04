#include "HeartbeatConfigStore.h"

#include <cstdlib>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../../logging/Logging.h"
#include <new>
#include <esp_heap_caps.h>
#include "../../memory/SystemAllocator.h"
#include "../../rtc/RtcConfig.h"
#include "../../utils/ScopeLock.h"

#undef LOG_TAG
#define LOG_TAG "HeartCfg"

namespace SYSTEM {
namespace HEARTBEAT_CONFIG {
namespace {

RTC::HeartbeatData* s_store = nullptr;
bool s_loggedFallback = false;

TickType_t configLockTimeoutTicks() {
    return xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED
               ? 0
               : pdMS_TO_TICKS(100);
}

RTC::HeartbeatData& requireStore() {
    if (!s_store) {
        RTC::HeartbeatData* psramStore = SYSTEM::MEMORY::allocInPsram<RTC::HeartbeatData>();
        if (psramStore) {
            *psramStore = RTC::HeartbeatData{};
            s_store = psramStore;
        } else {
            // Heartbeat config is intentionally PSRAM-first. The internal heap
            // path is lazy fallback only; do not reintroduce a static .bss copy
            // because the slot URLs make this structure a noticeable DRAM cost.
            void* mem = heap_caps_malloc(sizeof(RTC::HeartbeatData), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (mem) {
                s_store = new(mem) RTC::HeartbeatData();
                if (!s_loggedFallback) {
                    LOGW("Heartbeat config store PSRAM allocation failed; using internal heap fallback");
                    s_loggedFallback = true;
                }
            } else {
                LOGE("Heartbeat config store allocation failed entirely (PSRAM and Internal)");
                std::abort();
            }
        }
    }
    return *s_store;
}

}  // namespace

RTC::HeartbeatData copy() {
    RTC::HeartbeatData snapshot{};
    withConfig([&](const RTC::HeartbeatData& cfg) {
        snapshot = cfg;
    });
    return snapshot;
}

void withConfig(const std::function<void(const RTC::HeartbeatData&)>& reader) {
    RTC::HeartbeatData& cfg = requireStore();
    SemaphoreHandle_t lock = RTC::getLock();
    if (!lock) {
        LOGW("withConfig: RTC lock not initialized, returning unlocked heartbeat config");
        reader(cfg);
        return;
    }

    SYSTEM::ScopeLock guard(lock, configLockTimeoutTicks());
    if (!guard.isLocked()) {
        LOGW("withConfig: heartbeat config lock timeout");
        return;
    }

    reader(cfg);
}

bool update(const std::function<void(RTC::HeartbeatData&)>& updater) {
    RTC::HeartbeatData& cfg = requireStore();
    SemaphoreHandle_t lock = RTC::getLock();
    if (!lock) {
        LOGE("update: RTC lock not initialized, skipping heartbeat config update");
        return false;
    }

    SYSTEM::ScopeLock guard(lock, configLockTimeoutTicks());
    if (!guard.isLocked()) {
        LOGW("update: heartbeat config lock timeout");
        return false;
    }

    updater(cfg);
    return true;
}

}  // namespace HEARTBEAT_CONFIG
}  // namespace SYSTEM
