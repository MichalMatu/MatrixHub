/**
 * @file AlarmRuleManager.cpp
 * @brief Alarm rule manager - backed by PSRAM rules and RTC runtime state
 */

#include "AlarmRuleManager.h"
#include "AlarmRuleIdentity.h"
#include "AlarmRuleValidation.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../system/logging/Logging.h"
#include "../../system/memory/SystemAllocator.h"
#include "../../system/utils/ScopeLock.h"
#include <algorithm>
#include <cmath>
#include <cstring>

#undef LOG_TAG
#define LOG_TAG "AlarmMgr"

namespace ALARMS {

namespace {

int findRuleIndexById(const AlarmSnapshot& snapshot, const char* id) {
    for (uint8_t i = 0; i < snapshot.ruleCount; ++i) {
        if (strncmp(snapshot.rules[i].id, id, kMaxIdLen) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool hasShellyDevice(const AlarmRule& rule, const char* deviceId) {
    if (!deviceId || deviceId[0] == '\0') {
        return false;
    }
    for (uint8_t i = 0; i < rule.shellyDeviceCount && i < kMaxShellyPerRule; ++i) {
        if (strncmp(rule.shellyDeviceIds[i], deviceId, kShellyIdLen) == 0) {
            return true;
        }
    }
    return false;
}

bool appendAffectedShellyDevice(AlarmRuleUpdateEffects& effects,
                                const char* deviceId) {
    if (!deviceId || deviceId[0] == '\0') {
        return true;
    }
    for (uint8_t i = 0; i < effects.shellyDeviceCount; ++i) {
        if (strncmp(effects.shellyDeviceIds[i], deviceId, kShellyIdLen) == 0) {
            return true;
        }
    }
    if (effects.shellyDeviceCount >= kMaxRuleUpdateShellyDevices) {
        return false;
    }
    strlcpy(effects.shellyDeviceIds[effects.shellyDeviceCount],
            deviceId,
            kShellyIdLen);
    effects.shellyDeviceCount++;
    return true;
}

bool collectShellyBindingChanges(const AlarmSnapshot& previousState,
                                 const AlarmSnapshot& nextState,
                                 AlarmRuleUpdateEffects& effects) {
    for (uint8_t oldIndex = 0; oldIndex < previousState.ruleCount; ++oldIndex) {
        const AlarmRule& oldRule = previousState.rules[oldIndex];

        const int nextIndex = findRuleIndexById(nextState, oldRule.id);
        const bool compatible =
            nextIndex >= 0 &&
            hasSameAlarmRuntimeIdentity(oldRule, nextState.rules[nextIndex]);
        if (!compatible) {
            // Reconcile every old binding, even when retained runtime says the
            // rule was inactive. Global OR protects shared active ownership,
            // while an explicit false repairs a physically stale relay left by
            // an earlier admission/transport failure.
            for (uint8_t i = 0;
                 i < oldRule.shellyDeviceCount && i < kMaxShellyPerRule;
                 ++i) {
                if (!appendAffectedShellyDevice(
                        effects, oldRule.shellyDeviceIds[i])) {
                    return false;
                }
            }
            continue;
        }

        const AlarmRule& nextRule = nextState.rules[nextIndex];
        for (uint8_t i = 0;
             i < oldRule.shellyDeviceCount && i < kMaxShellyPerRule;
             ++i) {
            if (!hasShellyDevice(nextRule, oldRule.shellyDeviceIds[i]) &&
                !appendAffectedShellyDevice(effects, oldRule.shellyDeviceIds[i])) {
                return false;
            }
        }
        for (uint8_t i = 0;
             i < nextRule.shellyDeviceCount && i < kMaxShellyPerRule;
             ++i) {
            if (!hasShellyDevice(oldRule, nextRule.shellyDeviceIds[i]) &&
                !appendAffectedShellyDevice(effects, nextRule.shellyDeviceIds[i])) {
                return false;
            }
        }
    }

    // A new rule, or a semantically replaced rule, starts inactive. Still
    // publish an explicit global-OR false for every current binding so a relay
    // does not inherit an old physical ON state until the first future edge.
    for (uint8_t nextIndex = 0; nextIndex < nextState.ruleCount; ++nextIndex) {
        const AlarmRule& nextRule = nextState.rules[nextIndex];
        const int oldIndex = findRuleIndexById(previousState, nextRule.id);
        const bool compatible =
            oldIndex >= 0 &&
            hasSameAlarmRuntimeIdentity(previousState.rules[oldIndex], nextRule);
        if (compatible) {
            continue;
        }

        for (uint8_t i = 0;
             i < nextRule.shellyDeviceCount && i < kMaxShellyPerRule;
             ++i) {
            if (!appendAffectedShellyDevice(
                    effects, nextRule.shellyDeviceIds[i])) {
                return false;
            }
        }
    }
    return true;
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

    SYSTEM::ScopeLock scopeLock(_mutex, pdMS_TO_TICKS(kAlarmMutexTimeoutMs));
    if (!scopeLock.isLocked()) {
        LOGE("Mutex timeout in begin");
        return false;
    }

    // Restore stable trigger state, but never carry millis()-based cooldown
    // timestamps into a new boot clock domain. lastTriggeredMs == 0 together
    // with previouslyTriggered == true means "retained active, cooldown epoch
    // not anchored yet". AlarmEvaluator anchors it on the first valid sample
    // without notifying; a real false->true edge still notifies immediately.
    if (!syncFromStoresLocked()) {
        LOGE("Failed to read complete alarm boot snapshots");
        _initialized.store(false, std::memory_order_release);
        return false;
    }
    for (uint8_t i = 0; i < _state.ruleCount; ++i) {
        _state.runtimeStates[i].lastTriggeredMs = 0;
        // Both fields use the current millis() clock domain. Reset them on
        // every boot instead of presenting retained values as cross-boot
        // continuity evidence.
        _state.runtimeStates[i].resetTransitionObservation();
    }
    if (!commitLocked()) {
        LOGE("Failed to persist alarm boot epoch reset");
        _initialized.store(false, std::memory_order_release);
        return false;
    }
    _initialized.store(true, std::memory_order_release);

    LOGI("Rules active: %u", static_cast<unsigned>(_state.ruleCount));
    return true;
}

bool AlarmRuleManager::syncFromStoresLocked() {
    AlarmRuntimeSummary runtime{};
    bool loadedRuntime = false;
    RTC::withConfigSection(
        &RTC::ConfigStore::alarms,
        [&](const AlarmRuntimeSummary& retained) {
            runtime = retained;
            loadedRuntime = true;
        });
    if (!loadedRuntime) {
        LOGW("Failed to snapshot retained alarm runtime while syncing manager state");
        return false;
    }

    memset(&_state, 0, sizeof(_state));
    bool loadedRules = false;
    RULES_CONFIG::withRules([&](const AlarmRulesSnapshot& rules) {
        loadedRules = true;
        _state.ruleCount = std::min<uint8_t>(rules.ruleCount, kMaxRules);

        for (uint8_t i = 0; i < _state.ruleCount; i++) {
            _state.rules[i] = rules.rules[i];
            if (!canRetainAlarmRuntime(_state.rules[i])) {
                continue;
            }
            const uint64_t runtimeIdentityHash =
                stableAlarmRuntimeIdentityHash(_state.rules[i]);
            for (uint8_t runtimeIndex = 0;
                 runtimeIndex < runtime.ruleCount && runtimeIndex < kMaxRules;
                 ++runtimeIndex) {
                if (runtime.ruleRuntimeIdentityHashes[runtimeIndex] ==
                    runtimeIdentityHash) {
                    _state.runtimeStates[i] = runtime.runtimeStates[runtimeIndex];
                    break;
                }
            }
        }
    });

    if (!loadedRules) {
        LOGW("Failed to snapshot alarm rules while syncing manager state");
        memset(&_state, 0, sizeof(_state));
        return false;
    }
    return true;
}

bool AlarmRuleManager::commitLocked() {
    AlarmRuntimeSummary next{};
    next.ruleCount = _state.ruleCount;

    for (uint8_t i = 0; i < _state.ruleCount && i < kMaxRules; i++) {
        next.runtimeStates[i] = _state.runtimeStates[i];
        next.ruleRuntimeIdentityHashes[i] =
            stableAlarmRuntimeIdentityHash(_state.rules[i]);
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

bool AlarmRuleManager::updateRules(const AlarmRule* newRules,
                                   uint8_t count,
                                   AlarmRuleUpdateEffects& effects) {
    memset(&effects, 0, sizeof(effects));

    if (count > kMaxRules || (count > 0 && !newRules)) {
        LOGE("Invalid alarm rule update size: %u", static_cast<unsigned>(count));
        return false;
    }

    for (uint8_t i = 0; i < count; ++i) {
        AlarmRule normalizedRule = newRules[i];
        normalizedRule.normalizeBooleanSemantics();
        if (!isValidAlarmRuleDefinition(normalizedRule)) {
            LOGE("Invalid alarm rule at index %u", static_cast<unsigned>(i));
            return false;
        }
        for (uint8_t previous = 0; previous < i; ++previous) {
            if (strncmp(newRules[previous].id,
                        normalizedRule.id,
                        kMaxIdLen) == 0) {
                LOGE("Duplicate alarm rule id at index %u",
                     static_cast<unsigned>(i));
                return false;
            }
            if (alarmRuleNamesEqual(newRules[previous].name,
                                    normalizedRule.name)) {
                LOGE("Duplicate alarm rule name at index %u",
                     static_cast<unsigned>(i));
                return false;
            }
        }
    }

    SYSTEM::ScopeLock scopeLock(_mutex, pdMS_TO_TICKS(kAlarmMutexTimeoutMs));
    if (!scopeLock.isLocked()) {
        LOGE("Mutex timeout in updateRules");
        return false;
    }

    auto previousState = SYSTEM::MEMORY::makeUniqueInPsram<AlarmSnapshot>();
    auto nextState = SYSTEM::MEMORY::makeUniqueInPsram<AlarmSnapshot>();
    if (!previousState || !nextState) {
        LOGE("Failed to allocate transactional alarm snapshots");
        return false;
    }
    *previousState = _state;
    *nextState = AlarmSnapshot{};

    // Migrate runtime by stable rule id before replacing/reordering the rule
    // array. Only an enabled rule with the same trigger semantics can inherit
    // cooldown and active state. Presentation, delivery-channel and cooldown
    // edits do not manufacture a false edge for an already active CSI alarm.
    for (uint8_t i = 0; i < count; i++) {
        AlarmRule normalizedRule = newRules[i];
        normalizedRule.normalizeBooleanSemantics();

        const uint8_t nextIndex = nextState->ruleCount;
        nextState->rules[nextIndex] = normalizedRule;
        const int previousIndex =
            findRuleIndexById(*previousState, normalizedRule.id);
        if (previousIndex >= 0 &&
            hasSameAlarmRuntimeIdentity(previousState->rules[previousIndex],
                                        normalizedRule)) {
            nextState->runtimeStates[nextIndex] =
                previousState->runtimeStates[previousIndex];
        } else {
            nextState->runtimeStates[nextIndex].reset();
        }
        nextState->ruleCount++;
    }

    if (!collectShellyBindingChanges(*previousState, *nextState, effects)) {
        LOGE("Too many Shelly binding changes in alarm update");
        return false;
    }

    _state = *nextState;
    if (!commitLocked()) {
        _state = *previousState;
        memset(&effects, 0, sizeof(effects));
        return false;
    }
    _initialized.store(true, std::memory_order_release);
    effects.applied = true;
    
    LOGI("Updated alarm rules: %u", static_cast<unsigned>(_state.ruleCount));
    return true;
}

} // namespace ALARMS
