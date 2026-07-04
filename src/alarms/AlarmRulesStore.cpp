#include "AlarmRulesStore.h"

#include <cstdlib>
#include <cstring>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../system/logging/Logging.h"
#include <new>
#include <esp_heap_caps.h>
#include "../system/memory/SystemAllocator.h"
#include "../system/rtc/RtcConfig.h"
#include "../system/utils/ScopeLock.h"

#undef LOG_TAG
#define LOG_TAG "AlarmRules"

namespace ALARMS {
namespace RULES_CONFIG {
namespace {

AlarmRulesSnapshot* s_store = nullptr;
bool s_loggedFallback = false;

TickType_t configLockTimeoutTicks() {
    return xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED
               ? 0
               : pdMS_TO_TICKS(100);
}

AlarmRulesSnapshot& requireStore() {
    if (!s_store) {
        // Alarm rules are regular config data (not RTC-retained), so keeping
        // the live store in PSRAM is safe and saves scarce internal DRAM.
        AlarmRulesSnapshot* psramStore = SYSTEM::MEMORY::allocInPsram<AlarmRulesSnapshot>();
        if (psramStore) {
            *psramStore = AlarmRulesSnapshot{};
            s_store = psramStore;
        } else {
            // Use a lazy internal-heap fallback only after PSRAM allocation
            // actually fails. Do not keep a static .bss fallback here: these
            // stores were moved off DRAM on purpose to protect internal RAM.
            void* mem = heap_caps_malloc(sizeof(AlarmRulesSnapshot), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (mem) {
                s_store = new(mem) AlarmRulesSnapshot();
                if (!s_loggedFallback) {
                    LOGW("Alarm rules store PSRAM allocation failed; using internal heap fallback");
                    s_loggedFallback = true;
                }
            } else {
                LOGE("Alarm rules store allocation failed entirely (PSRAM and Internal)");
                std::abort();
            }
        }
    }
    return *s_store;
}

}  // namespace

bool copyTo(AlarmRulesSnapshot& out) {
    // Keep the copy target external so callers can place large snapshots in
    // PSRAM. This replaces the old copy()-by-value pattern.
    memset(&out, 0, sizeof(out));
    bool copied = false;
    withRules([&](const AlarmRulesSnapshot& cfg) {
        out = cfg;
        copied = true;
    });
    return copied;
}

void withRules(const std::function<void(const AlarmRulesSnapshot&)>& reader) {
    AlarmRulesSnapshot& cfg = requireStore();
    SemaphoreHandle_t lock = RTC::getLock();
    if (!lock) {
        LOGW("withRules: RTC lock not initialized, returning unlocked alarm rules");
        reader(cfg);
        return;
    }

    SYSTEM::ScopeLock guard(lock, configLockTimeoutTicks());
    if (!guard.isLocked()) {
        LOGW("withRules: alarm rules lock timeout");
        return;
    }

    reader(cfg);
}

bool update(const std::function<void(AlarmRulesSnapshot&)>& updater) {
    AlarmRulesSnapshot& cfg = requireStore();
    SemaphoreHandle_t lock = RTC::getLock();
    if (!lock) {
        LOGE("update: RTC lock not initialized, skipping alarm rules update");
        return false;
    }

    SYSTEM::ScopeLock guard(lock, configLockTimeoutTicks());
    if (!guard.isLocked()) {
        LOGW("update: alarm rules lock timeout");
        return false;
    }

    updater(cfg);
    return true;
}

}  // namespace RULES_CONFIG
}  // namespace ALARMS
