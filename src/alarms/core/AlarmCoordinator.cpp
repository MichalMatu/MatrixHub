#include "AlarmCoordinator.h"
#include "../engine/AlarmEvaluator.h"
#include "../utils/BleDataProvider.h"
#include "AlarmLogic.h"
#include "../../gpio/GpioService.h"
#include "../../system/logging/Logging.h"
#include "../../system/memory/SystemAllocator.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../system/utils/ScopeLock.h"
#include <esp_heap_caps.h>
#include <cmath>

#undef LOG_TAG
#define LOG_TAG "AlarmCoord"

namespace ALARMS {

namespace {

bool shouldPersistRuntimeState(const AlarmRuntimeState& before, const AlarmRuntimeState& after) {
    return before.lastTriggeredMs != after.lastTriggeredMs ||
           before.previouslyTriggered != after.previouslyTriggered ||
           before.initialized != after.initialized;
}

bool ruleHasShellyDevice(const AlarmRule& rule, const char* deviceId) {
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

bool appendUniqueShellyDevice(AlarmRuleUpdateEffects& devices,
                              const char* deviceId) {
    if (!deviceId || deviceId[0] == '\0') {
        return true;
    }
    for (uint8_t i = 0; i < devices.shellyDeviceCount; ++i) {
        if (strncmp(devices.shellyDeviceIds[i], deviceId, kShellyIdLen) == 0) {
            return true;
        }
    }
    if (devices.shellyDeviceCount >= kMaxRuleUpdateShellyDevices) {
        return false;
    }
    strlcpy(devices.shellyDeviceIds[devices.shellyDeviceCount],
            deviceId,
            kShellyIdLen);
    ++devices.shellyDeviceCount;
    return true;
}

#ifdef UNIT_TEST
bool g_forcePendingEventsAllocationFailure = false;
bool g_forceBootShellyAllocationFailure = false;
#endif

void* allocatePendingEventsStorage(size_t bytes) {
#ifdef UNIT_TEST
    if (g_forcePendingEventsAllocationFailure) {
        return nullptr;
    }
#endif
    return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void* allocateBootShellyStorage(size_t bytes) {
#ifdef UNIT_TEST
    if (g_forceBootShellyAllocationFailure) {
        return nullptr;
    }
#endif
    return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

}  // namespace

AlarmCoordinator::AlarmCoordinator(AlarmRuleManager& manager) 
    : _manager(manager) {
    _processMutex = xSemaphoreCreateMutexStatic(&_processMutexStorage);
    
    // Allocate buffer in PSRAM to avoid stack overflow and reduce heap fragmentation
    _pendingEventsBuffer = static_cast<PendingEvent*>(
        allocatePendingEventsStorage(kMaxRules * sizeof(PendingEvent)));
    _pendingShellyReconcileEffects = static_cast<AlarmRuleUpdateEffects*>(
        allocateBootShellyStorage(sizeof(AlarmRuleUpdateEffects)));
        
    if (!_pendingEventsBuffer) {
        LOGE("Failed to allocate _pendingEventsBuffer in PSRAM");
    }
    if (!_pendingShellyReconcileEffects) {
        LOGE("Failed to allocate Shelly reconcile outbox in PSRAM");
    } else {
        *_pendingShellyReconcileEffects = AlarmRuleUpdateEffects{};
    }
}

AlarmCoordinator::~AlarmCoordinator() {
    if (_pendingEventsBuffer) {
        heap_caps_free(_pendingEventsBuffer);
        _pendingEventsBuffer = nullptr;
    }
    if (_pendingShellyReconcileEffects) {
        heap_caps_free(_pendingShellyReconcileEffects);
        _pendingShellyReconcileEffects = nullptr;
    }
}

#ifdef UNIT_TEST
namespace TEST_HOOKS {

void setAlarmPendingEventsAllocationFailure(bool fail) {
    g_forcePendingEventsAllocationFailure = fail;
}

void setAlarmBootShellyAllocationFailure(bool fail) {
    g_forceBootShellyAllocationFailure = fail;
}

}  // namespace TEST_HOOKS
#endif

void AlarmCoordinator::reapplyLatchedState() {
    _matrixController.reapplyLatchedState();
}

void AlarmCoordinator::refreshLatchedStateFromRuntime() {
    // Match the normal evaluation lock order (process -> manager). This makes a
    // rule update's final display rebuild run after any evaluation that already
    // captured the previous ruleset, so an old aggregate cannot overwrite the
    // migrated state after this method returns.
    SYSTEM::ScopeLock processLock(_processMutex, portMAX_DELAY);
    if (!processLock.isLocked()) {
        LOGW("Process mutex unavailable while refreshing alarm display");
        return;
    }

    refreshLatchedStateFromRuntimeLocked();
}

void AlarmCoordinator::refreshLatchedStateFromRuntimeLocked() {
    AlarmAggregateState aggregate;
    aggregate.reset();
    {
        SYSTEM::ScopeLock managerLock(
            _manager.getMutex(), pdMS_TO_TICKS(kAlarmMutexTimeoutMs));
        if (!managerLock.isLocked()) {
            LOGW("Manager mutex timeout while refreshing alarm display");
            return;
        }

        const AlarmRule* rules = _manager.getRules();
        AlarmRuntimeState* states = _manager.getStates();
        const uint8_t count = _manager.getCount();
        for (uint8_t i = 0; i < count; ++i) {
            if (rules[i].isValid() && rules[i].enabled) {
                AlarmLogic::updateAggregate(rules[i], states[i], aggregate);
            }
        }
    }

    _matrixController.update(aggregate);
}

void AlarmCoordinator::reconcileAllShellyDevices() {
    SYSTEM::ScopeLock processLock(_processMutex, portMAX_DELAY);
    if (!processLock.isLocked()) {
        LOGW("Process mutex unavailable while reconciling Shelly outputs");
        return;
    }
    if (!_pendingShellyReconcileEffects) {
        LOGE("Shelly reconciliation outbox unavailable");
        return;
    }

    {
        SYSTEM::ScopeLock managerLock(
            _manager.getMutex(), pdMS_TO_TICKS(kAlarmMutexTimeoutMs));
        if (!managerLock.isLocked()) {
            LOGW("Manager mutex timeout while collecting Shelly outputs");
            return;
        }

        const AlarmRule* rules = _manager.getRules();
        const uint8_t count = _manager.getCount();
        for (uint8_t ruleIndex = 0; ruleIndex < count; ++ruleIndex) {
            if (!rules[ruleIndex].isValid()) {
                continue;
            }
            for (uint8_t deviceIndex = 0;
                 deviceIndex < rules[ruleIndex].shellyDeviceCount &&
                 deviceIndex < kMaxShellyPerRule;
                 ++deviceIndex) {
                if (!enqueueShellyReconcileLocked(
                        rules[ruleIndex].shellyDeviceIds[deviceIndex])) {
                    LOGE("Too many Shelly devices during boot reconciliation");
                    return;
                }
            }
        }
    }

    // Publish one latest-wins intent per device after boot/executor wiring.
    // This restores retained active alarms and explicitly clears current
    // bindings whose retained global OR is false, without waiting for an edge.
    retryPendingShellyActionsLocked();
}

void AlarmCoordinator::retryPendingShellyActions() {
    if (!_pendingShellyReconcileEffects) {
        return;
    }

    SYSTEM::ScopeLock processLock(
        _processMutex, pdMS_TO_TICKS(kAlarmEvalMutexTimeoutMs));
    if (!processLock.isLocked()) {
        return;
    }
    retryPendingShellyActionsLocked();
}

#ifdef UNIT_TEST
bool AlarmCoordinator::enqueueShellyReconcileForTest(const char* deviceId) {
    SYSTEM::ScopeLock processLock(_processMutex, portMAX_DELAY);
    return processLock.isLocked() && enqueueShellyReconcileLocked(deviceId);
}

uint8_t AlarmCoordinator::pendingShellyReconcileCountForTest() {
    SYSTEM::ScopeLock processLock(_processMutex, portMAX_DELAY);
    if (!processLock.isLocked() || !_pendingShellyReconcileEffects) {
        return 0;
    }
    return _pendingShellyReconcileEffects->shellyDeviceCount;
}
#endif

bool AlarmCoordinator::updateRules(const AlarmRule* rules, uint8_t count) {
    SYSTEM::ScopeLock processLock(_processMutex, portMAX_DELAY);
    if (!processLock.isLocked()) {
        LOGE("Process mutex unavailable during alarm rule update");
        return false;
    }

    // A committed rule update may need 64 unique old/new binding effects. Keep
    // another 32 slots reserved for every current binding that a runtime edge
    // or Shelly config callback can reconcile before transport recovers. Never
    // commit a second update while the first update's imperative side effects
    // are still pending; returning false lets AlarmSettingsService roll back
    // its filesystem transaction instead of acknowledging a dropped intent.
    retryPendingShellyActionsLocked();
    if (_pendingShellyReconcileEffects &&
        _pendingShellyReconcileEffects->shellyDeviceCount > 0) {
        LOGW("Deferring alarm rule update until Shelly reconciliation is admitted");
        return false;
    }

    auto effects = SYSTEM::MEMORY::makeUniqueInPsram<AlarmRuleUpdateEffects>();
    if (!effects) {
        LOGE("Failed to allocate alarm rule update effects");
        return false;
    }
    if (!_manager.updateRules(rules, count, *effects)) {
        return false;
    }

    // No evaluation based on the previous ruleset can execute after this point:
    // processLock covers the manager commit, global Shelly reconciliation and
    // final Matrix aggregate rebuild as one control-plane transaction.
    reconcileAffectedShellyDevices(*effects);
    refreshLatchedStateFromRuntimeLocked();
    return true;
}

void AlarmCoordinator::clearLatchedLed() {
    _matrixController.clearLatchedState();
}

bool AlarmCoordinator::isAlarmLatched() const {
    return _matrixController.isLatched();
}

AlarmInputData AlarmCoordinator::buildInputData(const ::SensorSnapshot& sensors,
                                                float wifiVariance,
                                                float wifiCsiMotion,
                                                float imuTamper) const {
    AlarmInputData input;
    input.co2 = static_cast<float>(sensors.co2);
    input.temperature = sensors.temp;
    input.humidity = sensors.humid;
    input.wifiVariance = wifiVariance;
    input.wifiCsiMotion = wifiCsiMotion;
    input.imuTamper = imuTamper;
    return input;
}

float AlarmCoordinator::getValueForLogging(const AlarmRule& rule, const AlarmInputData& input) {
    if (rule.source == AlarmSource::CO2) return input.co2;
    if (rule.source == AlarmSource::Temperature) return input.temperature;
    if (rule.source == AlarmSource::Humidity) return input.humidity;
    if (rule.source == AlarmSource::WifiMotion) return input.wifiVariance;
    if (rule.source == AlarmSource::WifiCsiMotion) return input.wifiCsiMotion;
    if (rule.source == AlarmSource::ImuTamper) return input.imuTamper;
    if (rule.source == AlarmSource::GpioDigital) return input.gpioDigital;
    if (rule.source == AlarmSource::BleTemperature) return input.bleTemp;
    if (rule.source == AlarmSource::BleHumidity) return input.bleHumid;
    if (rule.source == AlarmSource::BleBattery) return input.bleBattery;
    if (rule.source == AlarmSource::BleRssi) return input.bleRssi;
    return NAN;
}

AlarmCoordinator::EvaluationPassResult AlarmCoordinator::collectPendingEvents(
    const AlarmInputData& input,
    uint32_t now,
    const GpioAlarmEdgeMailbox::PassSnapshot* gpioEdges) {
    EvaluationPassResult passResult;
    passResult.ledState.reset();

    // Hot-path evaluation must not stall sensor or WiFi sensing loops for seconds.
    // If rules are being updated concurrently, skip this pass and let the next
    // sensor sample re-run evaluation instead of blocking the producer task.
    SYSTEM::ScopeLock managerLock(_manager.getMutex(), pdMS_TO_TICKS(kAlarmEvalMutexTimeoutMs));
    if (!managerLock.isLocked()) {
        LOGW("Manager mutex timeout in process");
        return passResult;
    }

    passResult.ready = true;

    AlarmInputData currentInput = input;
    const uint8_t count = _manager.getCount();
    const AlarmRule* rules = _manager.getRules();
    AlarmRuntimeState* states = _manager.getStates();
    AlarmRuntimeState runtimeBefore[kMaxRules]{};

    // Runtime transitions are acknowledged only after their retained summary
    // commits successfully. Keep a small fixed before-image on the caller
    // stack so a transient RTC lock failure can roll the whole evaluation pass
    // back without allocating on this hot path.
    for (uint8_t i = 0; i < count; ++i) {
        runtimeBefore[i] = states[i];
    }

    if (count == 0) {
        return passResult;
    }

    passResult.hasRules = true;
    bool runtimeStateDirty = false;

    for (uint8_t i = 0; i < count; i++) {
        const AlarmRule& rule = rules[i];
        AlarmRuntimeState& state = states[i];
        const AlarmRuntimeState beforeEval = state;

        if (!rule.isValid() || !rule.enabled) {
            continue;
        }

        populateBleValues(
            rule.isBleSource(),
            rule.source,
            rule.bleDeviceMac,
            now,
            _bleService,
            currentInput.bleTemp,
            currentInput.bleHumid,
            currentInput.bleBattery,
            currentInput.bleRssi
        );

        if (rule.isGpioSource()) {
            bool logicalValue = false;
            if (gpioEdges && gpioEdges->valueFor(rule.gpioId, logicalValue)) {
                // The retained edge wins over the already-converged live GPIO
                // level. This is what preserves a complete pulse between alarm
                // passes while still keeping selectors isolated by gpioId.
                currentInput.gpioDigital = logicalValue ? 1.0f : 0.0f;
            } else {
                currentInput.gpioDigital =
                    _gpioService &&
                            _gpioService->getLogicalValue(rule.gpioId, logicalValue)
                        ? (logicalValue ? 1.0f : 0.0f)
                        : NAN;
            }
        } else {
            currentInput.gpioDigital = NAN;
        }

        EvaluationResult result = AlarmEvaluator::evaluate(rule, currentInput, state, now);
        const float valueForLogging = getValueForLogging(rule, currentInput);

        char safeName[kMaxAlarmNameLen] = {0};
        safeCopyAlarmName(safeName, rule.name);

        if (result.stateChanged || result.shouldNotify) {
            LOGD("Rule '%s' state: Src=%d Val=%.2f Thresh=%.2f -> Trig=%d Notify=%d Ch=0x%02X",
                 safeName, static_cast<int>(rule.source),
                 valueForLogging, rule.threshold, result.triggered, result.shouldNotify, rule.notifyChannels);
        }

        const bool shouldBroadcast =
            (_stateChangeCb != nullptr) &&
            result.stateChanged &&
            !std::isnan(result.currentValue);
        AlarmStateChange changeMsg{};
        if (shouldBroadcast) {
            strlcpy(changeMsg.id, rule.id, sizeof(changeMsg.id));
            changeMsg.triggered = result.triggered;
            changeMsg.currentValue = result.currentValue;
            changeMsg.severity = rule.severity;
        }

        AlarmLogic::updateAggregate(rule, state, passResult.ledState);

        const bool wasUninitialized = !state.initialized;
        state.initialized = true;
        runtimeStateDirty = runtimeStateDirty || shouldPersistRuntimeState(beforeEval, state);

        if (passResult.pendingCount >= kMaxRules) {
            continue;
        }

        AlarmAction action = AlarmLogic::determineAction(rule, result, wasUninitialized);
        if (!action.hasAction() && !shouldBroadcast) {
            continue;
        }

        PendingEvent& event = _pendingEventsBuffer[passResult.pendingCount];
        event.ruleSnapshot = rule;
        event.evalResult = result;
        event.action = action;
        event.shouldBroadcastState = shouldBroadcast;
        if (shouldBroadcast) {
            event.stateChangeMsg = changeMsg;
        }
        event.evalResult.rule = &event.ruleSnapshot;
        passResult.pendingCount++;
    }

    if (runtimeStateDirty && !_manager.persistRuntimeStateLocked()) {
        for (uint8_t i = 0; i < count; ++i) {
            states[i] = runtimeBefore[i];
        }

        // Returning a non-ready pass prevents Matrix, Shelly, notification and
        // WebSocket side effects. AlarmService then leaves its mailbox/CSI edge
        // pending and retries the same transition after RTC storage recovers.
        passResult.ready = false;
        passResult.pendingCount = 0;
        passResult.ledState.reset();
        LOGW("Failed to persist retained alarm runtime state; evaluation rolled back");
    }

    return passResult;
}

uint8_t AlarmCoordinator::executePendingEvents(uint8_t pendingCount, const AlarmAggregateState& ledState) {
    _matrixController.update(ledState);

    uint8_t notifiedCount = 0;
    for (uint8_t i = 0; i < pendingCount; i++) {
        PendingEvent& event = _pendingEventsBuffer[i];

        if (event.action.triggerShelly) {
            for (uint8_t deviceIndex = 0;
                 deviceIndex < event.ruleSnapshot.shellyDeviceCount &&
                 deviceIndex < kMaxShellyPerRule;
                 ++deviceIndex) {
                const char* deviceId = event.ruleSnapshot.shellyDeviceIds[deviceIndex];
                if (!deviceId || deviceId[0] == '\0') {
                    continue;
                }

                bool alreadyHandled = false;
                for (uint8_t previousEvent = 0;
                     previousEvent <= i && !alreadyHandled;
                     ++previousEvent) {
                    const PendingEvent& candidate = _pendingEventsBuffer[previousEvent];
                    if (!candidate.action.triggerShelly) {
                        continue;
                    }
                    const uint8_t deviceLimit =
                        previousEvent == i
                            ? deviceIndex
                            : std::min<uint8_t>(candidate.ruleSnapshot.shellyDeviceCount,
                                                kMaxShellyPerRule);
                    for (uint8_t previousDevice = 0;
                         previousDevice < deviceLimit;
                         ++previousDevice) {
                        if (strncmp(candidate.ruleSnapshot.shellyDeviceIds[previousDevice],
                                    deviceId,
                                    kShellyIdLen) == 0) {
                            alreadyHandled = true;
                            break;
                        }
                    }
                }
                if (alreadyHandled) {
                    continue;
                }

                if (!enqueueShellyReconcileLocked(deviceId)) {
                    LOGE("Shelly reconcile outbox full for %s", deviceId);
                }
            }
        }

        if (event.action.sendNotify) {
            NotifyResult notifyResult = _notifier.notify(event.evalResult);
            if (notifyResult.anySuccess()) {
                notifiedCount++;
                LOGI("Alarm '%s' triggered: %.1f",
                     event.ruleSnapshot.name,
                     event.evalResult.currentValue);
            }
        } else if (event.action.sendClear) {
            NotifyResult notifyResult = _notifier.notifyCleared(event.evalResult);
            if (notifyResult.anySuccess()) {
                LOGI("Alarm '%s' cleared", event.ruleSnapshot.name);
            }
        }

        if (event.shouldBroadcastState && _stateChangeCb) {
            _stateChangeCb(event.stateChangeMsg);
        }
    }

    // Admission failure is independent of the sensor mailbox: keep retrying
    // from the fixed outbox even after this evaluation pass is acknowledged.
    retryPendingShellyActionsLocked();
    return notifiedCount;
}

ShellyActionResult AlarmCoordinator::executeShellyAction(
    const AlarmRule& rule,
    bool turnOn) const {
    if (!_shellyActionExecutor) {
        LOGW("Shelly executor not configured");
        return ShellyActionResult::Retry;
    }

    return _shellyActionExecutor(rule, turnOn);
}

ShellyActionResult AlarmCoordinator::executeShellyDeviceAction(
    const char* deviceId,
    bool turnOn) const {
    if (!deviceId || deviceId[0] == '\0') {
        return ShellyActionResult::Terminal;
    }

    AlarmRule deviceRule;
    if (!deviceRule.addShellyDevice(deviceId)) {
        return ShellyActionResult::Terminal;
    }
    return executeShellyAction(deviceRule, turnOn);
}

bool AlarmCoordinator::getShellyDeviceDesiredState(const char* deviceId,
                                                   bool& desiredOn) const {
    desiredOn = false;
    SYSTEM::ScopeLock managerLock(
        _manager.getMutex(), pdMS_TO_TICKS(kAlarmMutexTimeoutMs));
    if (!managerLock.isLocked()) {
        LOGW("Manager mutex timeout while aggregating Shelly state");
        return false;
    }

    const AlarmRule* rules = _manager.getRules();
    AlarmRuntimeState* states = _manager.getStates();
    const uint8_t count = _manager.getCount();
    for (uint8_t i = 0; i < count; ++i) {
        if (rules[i].isValid() && rules[i].enabled &&
            states[i].previouslyTriggered &&
            ruleHasShellyDevice(rules[i], deviceId)) {
            desiredOn = true;
            break;
        }
    }
    return true;
}

bool AlarmCoordinator::enqueueShellyReconcileLocked(const char* deviceId) {
    if (!_pendingShellyReconcileEffects) {
        return false;
    }
    return appendUniqueShellyDevice(
        *_pendingShellyReconcileEffects, deviceId);
}

void AlarmCoordinator::retryPendingShellyActionsLocked() {
    if (!_pendingShellyReconcileEffects ||
        _pendingShellyReconcileEffects->shellyDeviceCount == 0 ||
        !_shellyActionExecutor) {
        return;
    }

    uint8_t index = 0;
    while (index < _pendingShellyReconcileEffects->shellyDeviceCount) {
        const char* deviceId =
            _pendingShellyReconcileEffects->shellyDeviceIds[index];
        bool desiredOn = false;
        if (!getShellyDeviceDesiredState(deviceId, desiredOn)) {
            ++index;
            continue;
        }

        const ShellyActionResult result =
            executeShellyDeviceAction(deviceId, desiredOn);
        if (result == ShellyActionResult::Retry) {
            ++index;
            continue;
        }

        // Accepted means the worker ledger owns the intent. Terminal means the
        // configured target is definitively absent/invalid, so retaining it
        // would only fill the fixed outbox forever. Both close this entry.
        const uint8_t last =
            --_pendingShellyReconcileEffects->shellyDeviceCount;
        if (index != last) {
            strlcpy(_pendingShellyReconcileEffects->shellyDeviceIds[index],
                    _pendingShellyReconcileEffects->shellyDeviceIds[last],
                    kShellyIdLen);
        }
        _pendingShellyReconcileEffects->shellyDeviceIds[last][0] = '\0';
    }
}

void AlarmCoordinator::reconcileAffectedShellyDevices(
    const AlarmRuleUpdateEffects& effects) {
    if (effects.shellyDeviceCount == 0) {
        return;
    }

    for (uint8_t i = 0; i < effects.shellyDeviceCount; ++i) {
        if (!enqueueShellyReconcileLocked(effects.shellyDeviceIds[i])) {
            LOGE("Shelly reconcile outbox full for %s",
                 effects.shellyDeviceIds[i]);
        }
    }
    retryPendingShellyActionsLocked();
}

uint8_t AlarmCoordinator::process(const ::SensorSnapshot& sensors,
                                  float wifiVariance,
                                  float wifiCsiMotion,
                                  float imuTamper,
                                  const GpioAlarmEdgeMailbox::PassSnapshot* gpioEdges,
                                  bool* evaluationCompleted) {
    if (evaluationCompleted) {
        *evaluationCompleted = false;
    }
    if (!_manager.isInitialized() || !isReady()) return 0;

    // Protect the shared pending-events buffer, but keep the timeout short:
    // sensor and WiFi sensing are producer hot paths, so dropping one pass is
    // cheaper than letting alarm bookkeeping back-pressure the main loop.
    SYSTEM::ScopeLock processLock(_processMutex, pdMS_TO_TICKS(kAlarmEvalMutexTimeoutMs));
    if (!processLock.isLocked()) {
        LOGW("Process mutex timeout - skipping evaluation");
        return 0;
    }

    const AlarmInputData input = buildInputData(sensors, wifiVariance, wifiCsiMotion, imuTamper);
    const EvaluationPassResult passResult =
        collectPendingEvents(input, millis(), gpioEdges);

    if (!passResult.ready) {
        return 0;
    }

    if (evaluationCompleted) {
        *evaluationCompleted = true;
    }

    if (!passResult.hasRules) {
        clearLatchedLed();
        return 0;
    }

    return executePendingEvents(passResult.pendingCount, passResult.ledState);
}

} // namespace ALARMS
