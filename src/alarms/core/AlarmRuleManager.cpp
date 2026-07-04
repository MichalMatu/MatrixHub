/**
 * @file AlarmRuleManager.cpp
 * @brief Alarm rule manager - backed by PSRAM rules and RTC runtime state
 */

#include "AlarmRuleManager.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../system/logging/Logging.h"
#include "../../system/utils/ScopeLock.h"
#include <algorithm>
#include <cstring>

#undef LOG_TAG
#define LOG_TAG "AlarmMgr"

namespace ALARMS {

namespace {

bool ruleRemainsEnabled(const AlarmRule& oldRule, const AlarmRule* newRules, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        if (strncmp(oldRule.id, newRules[i].id, kMaxIdLen) == 0) {
            return newRules[i].enabled;
        }
    }

    return false;
}

}  // namespace

AlarmRuleManager::AlarmRuleManager() {
    _mutex = xSemaphoreCreateMutexStatic(&_mutexStorage);
}

bool AlarmRuleManager::begin() {
    if (!_mutex) {
        LOGE("Mutex not created");
        return false;
    }
    
    // Warm up the manager from the live PSRAM rule store plus the retained RTC
    // runtime summary already restored during boot.
    return reloadRules();
}

void AlarmRuleManager::syncFromStoresLocked() {
    const AlarmRuntimeSummary runtime = RTC::copyConfigSection(&RTC::ConfigStore::alarms);

    memset(&_state, 0, sizeof(_state));
    bool loadedRules = false;
    RULES_CONFIG::withRules([&](const AlarmRulesSnapshot& rules) {
        loadedRules = true;
        _state.ruleCount = std::min<uint8_t>(rules.ruleCount, kMaxRules);

        for (uint8_t i = 0; i < _state.ruleCount; i++) {
            _state.rules[i] = rules.rules[i];
            if (i < runtime.ruleCount) {
                _state.runtimeStates[i] = runtime.runtimeStates[i];
            }
        }
    });

    if (!loadedRules) {
        LOGW("Failed to snapshot alarm rules while syncing manager state");
    }
}

bool AlarmRuleManager::commitLocked() {
    AlarmRuntimeSummary next{};
    next.ruleCount = _state.ruleCount;

    for (uint8_t i = 0; i < _state.ruleCount && i < kMaxRules; i++) {
        next.runtimeStates[i] = _state.runtimeStates[i];
        if (_state.rules[i].enabled) {
            next.enabledCount++;
        }
    }

    const bool ok = RTC::updateConfigSection(&RTC::ConfigStore::alarms, [&](AlarmRuntimeSummary& alarms) {
        alarms = next;
    });

    if (!ok) {
        LOGE("Failed to commit alarm snapshot to RTC");
    }
    return ok;
}

bool AlarmRuleManager::resetRuntimeStateLocked() {
    for (uint8_t i = 0; i < kMaxRules; i++) {
        _state.runtimeStates[i].reset();
    }
    return commitLocked();
}

bool AlarmRuleManager::persistRuntimeState() {
    SYSTEM::ScopeLock scopeLock(_mutex, pdMS_TO_TICKS(kAlarmMutexTimeoutMs));
    if (!scopeLock.isLocked()) {
        LOGW("Mutex timeout in persistRuntimeState");
        return false;
    }

    return persistRuntimeStateLocked();
}

bool AlarmRuleManager::persistRuntimeStateLocked() {
    return commitLocked();
}

bool AlarmRuleManager::reloadRules() {
    SYSTEM::ScopeLock scopeLock(_mutex, pdMS_TO_TICKS(kAlarmMutexTimeoutMs));
    if (!scopeLock.isLocked()) {
        LOGE("Mutex timeout in reloadRules");
        return false;
    }

    syncFromStoresLocked();
    _initialized = true;
    
    LOGI("Rules active: %u", static_cast<unsigned>(_state.ruleCount));
    return true;
}

AlarmRuleUpdateEffects AlarmRuleManager::updateRules(const AlarmRule* newRules, uint8_t count) {
    AlarmRuleUpdateEffects effects;

    SYSTEM::ScopeLock scopeLock(_mutex, pdMS_TO_TICKS(kAlarmMutexTimeoutMs));
    if (!scopeLock.isLocked()) {
        LOGE("Mutex timeout in updateRules");
        return effects;
    }

    for (uint8_t i = 0; i < _state.ruleCount; i++) {
        const AlarmRule& oldRule = _state.rules[i];

        if (_state.runtimeStates[i].previouslyTriggered &&
            oldRule.hasShellyDevices() &&
            !ruleRemainsEnabled(oldRule, newRules, count) &&
            effects.shellyOffCount < kMaxRules) {
            effects.shellyOffRules[effects.shellyOffCount++] = oldRule;
        }
    }

    // Update local rule snapshot
    _state.ruleCount = 0;
    for (uint8_t i = 0; i < count && i < kMaxRules; i++) {
        if (newRules[i].isValid()) {
            _state.rules[_state.ruleCount] = newRules[i];
            _state.ruleCount++;
        }
    }

    // Intentionally reset runtime state for the whole ruleset after any config
    // write. Alarm updates are applied as one transactional snapshot, not as a
    // per-rule patch, so carrying old cooldown/trigger state across a changed
    // snapshot would mix semantics from two different alarm definitions.
    //
    // This means that toggling or editing one rule can re-arm other enabled
    // rules that are already above/below threshold, causing them to notify
    // again on the next evaluation pass. That behavior is currently preferred
    // to stale-state preservation because it keeps config changes atomic and
    // easier to reason about during runtime recovery and rollback.
    //
    // Do not "optimize" this into a partial per-rule preserve unless the
    // design explicitly handles source/operator/threshold/channel changes and
    // documents how cooldowns migrate across those mutations.
    if (!resetRuntimeStateLocked()) {
        return effects;
    }
    _initialized = true;
    effects.applied = true;
    
    LOGI("Updated alarm rules: %u", static_cast<unsigned>(_state.ruleCount));
    return effects;
}

} // namespace ALARMS
