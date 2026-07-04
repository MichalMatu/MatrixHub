#pragma once

#include "../types/AlarmRule.h"
#include "../types/AlarmRuntimeState.h"
#include "../types/AlarmConstants.h"
#include "../AlarmRulesStore.h"
#include "../../system/rtc/types/RtcAlarmTypes.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
namespace ALARMS {

struct AlarmRuleUpdateEffects {
    AlarmRule shellyOffRules[kMaxRules];
    uint8_t shellyOffCount = 0;
    bool applied = false;
};

/**
 * @brief Manages lifecycle, storage, and thread-safety of alarm rules.
 * Rules live in PSRAM config, runtime states stay in RTC.
 */
class AlarmRuleManager {
public:
    AlarmRuleManager();
    
    // Lifecycle
    bool begin();
    bool reloadRules();
    
    // Accessors (Thread-safety handled by caller using getLock/unlock or specialized methods)
    // NOTE: For iteration efficiency, we expose raw arrays but require locking.
    
    // Lock the manager (must be done before accessing rules/states)
    SemaphoreHandle_t getMutex() const { return _mutex; }
    
    // Data access (Must be locked!)
    const AlarmRule* getRules() const { return _state.rules; }
    AlarmRuntimeState* getStates() { return _state.runtimeStates; }
    uint8_t getCount() const { return _state.ruleCount; }
    
    // State management
    bool resetRuntimeStateLocked();
    bool persistRuntimeState();
    bool persistRuntimeStateLocked();
    
    /**
     * @brief Update rules with new set.
     * Returns side-effect intents for the imperative shell to execute.
     */
    AlarmRuleUpdateEffects updateRules(const AlarmRule* newRules, uint8_t count);

    bool isInitialized() const { return _initialized; }

private:
    void syncFromStoresLocked();
    bool commitLocked();

    AlarmSnapshot _state{};
    bool _initialized = false;
    
    // Thread safety
    SemaphoreHandle_t _mutex = nullptr;
    StaticSemaphore_t _mutexStorage;
};

} // namespace ALARMS
