#pragma once

#include "../types/AlarmRule.h"
#include "../types/AlarmRuntimeState.h"
#include "../types/AlarmConstants.h"
#include "../AlarmRulesStore.h"
#include "../../system/rtc/types/RtcAlarmTypes.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <atomic>
namespace ALARMS {

// PSRAM outbox capacity: one rule update can affect every old and new binding
// (64), while a concurrent runtime/config reconciliation may additionally need
// every current binding (32). Coordinator admission prevents a second update
// commit while any prior entry is still pending.
constexpr uint8_t kMaxRuleUpdateShellyDevices =
    kMaxRules * kMaxShellyPerRule * 3;

struct AlarmRuleUpdateEffects {
    char shellyDeviceIds[kMaxRuleUpdateShellyDevices][kShellyIdLen]{};
    uint8_t shellyDeviceCount = 0;
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
    
    // Accessors (Thread-safety handled by caller using getLock/unlock or specialized methods)
    // NOTE: For iteration efficiency, we expose raw arrays but require locking.
    
    // Lock the manager (must be done before accessing rules/states)
    SemaphoreHandle_t getMutex() const { return _mutex; }
    
    // Data access (Must be locked!)
    const AlarmRule* getRules() const { return _state.rules; }
    AlarmRuntimeState* getStates() { return _state.runtimeStates; }
    uint8_t getCount() const { return _state.ruleCount; }
    
    // State management
    bool persistRuntimeState();
    bool persistRuntimeStateLocked();
    
    /**
     * @brief Update rules with new set.
     * Returns side-effect intents for the imperative shell to execute.
     */
    bool updateRules(const AlarmRule* newRules,
                     uint8_t count,
                     AlarmRuleUpdateEffects& effects);

    bool isInitialized() const { return _initialized.load(std::memory_order_acquire); }

private:
    bool syncFromStoresLocked();
    bool commitLocked();

    AlarmSnapshot _state{};
    std::atomic<bool> _initialized{false};
    
    // Thread safety
    SemaphoreHandle_t _mutex = nullptr;
    StaticSemaphore_t _mutexStorage;
};

} // namespace ALARMS
