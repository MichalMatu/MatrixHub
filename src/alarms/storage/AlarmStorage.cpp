/**
 * @file AlarmStorage.cpp
 * @brief PSRAM-backed persistence adapter for alarm rules
 */

#include "AlarmStorage.h"
#include "../AlarmRulesStore.h"
#include "core/config/ConfigManager.h"
#include "../../system/logging/Logging.h"
#include "../../system/memory/SystemAllocator.h"

#include <cstring>

#undef LOG_TAG
#define LOG_TAG "AlarmStore"

namespace ALARMS {

AlarmStorage::AlarmStorage(FS& fs)
    : _fs(fs) {}

void AlarmStorage::buildRulesSnapshot(const AlarmRule* rules, uint8_t count, AlarmRulesSnapshot& out) {
    // Before: this function returned AlarmRulesData by value, which forced a
    // large temporary object on the current stack. Now the caller provides the
    // destination buffer, so the snapshot can live in PSRAM instead.
    memset(&out, 0, sizeof(out));
    if (!rules) {
        return;
    }

    for (uint8_t i = 0; i < count; i++) {
        if (out.ruleCount >= RTC::kMaxAlarmRules) break;
        if (!rules[i].isValid()) {
            continue;
        }
        out.rules[out.ruleCount] = rules[i];
        out.ruleCount++;
    }
}

bool AlarmStorage::persistRules(const AlarmRule* rules, uint8_t count) {
    // This snapshot is plain CPU-owned config data, so a temporary PSRAM
    // buffer is safe here and avoids putting ~2 KB on the current task stack.
    // Before: persistRules(buildRulesSnapshot(...)) created that snapshot as a
    // local return-by-value object on the stack.
    auto snapshot = SYSTEM::MEMORY::makeUniqueInPsram<AlarmRulesSnapshot>();
    if (!snapshot) {
        LOGE("Failed to allocate PSRAM snapshot for staged alarm rules");
        return false;
    }

    buildRulesSnapshot(rules, count, *snapshot);
    return persistRules(*snapshot);
}

bool AlarmStorage::persistRules(const AlarmRulesSnapshot& rulesData) {
    if (CONFIG::saveWithAlarmRules(_fs, rulesData)) {
        LOGI("Saved %u staged rules to FS", static_cast<unsigned>(rulesData.ruleCount));
        return true;
    }

    LOGE("Failed to persist staged alarm rules to FS");
    return false;
}

}  // namespace ALARMS
