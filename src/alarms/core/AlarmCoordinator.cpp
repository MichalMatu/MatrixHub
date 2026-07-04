#include "AlarmCoordinator.h"
#include "../engine/AlarmEvaluator.h"
#include "../utils/BleDataProvider.h"
#include "AlarmLogic.h"
#include "../../gpio/GpioService.h"
#include "../../system/logging/Logging.h"
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

}  // namespace

AlarmCoordinator::AlarmCoordinator(AlarmRuleManager& manager) 
    : _manager(manager) {
    _processMutex = xSemaphoreCreateMutexStatic(&_processMutexStorage);
    
    // Allocate buffer in PSRAM to avoid stack overflow and reduce heap fragmentation
    _pendingEventsBuffer = static_cast<PendingEvent*>(
        heap_caps_malloc(kMaxRules * sizeof(PendingEvent), MALLOC_CAP_SPIRAM));
        
    if (!_pendingEventsBuffer) {
        LOGE("Failed to allocate _pendingEventsBuffer in PSRAM");
    }
}

AlarmCoordinator::~AlarmCoordinator() {
    if (_pendingEventsBuffer) {
        heap_caps_free(_pendingEventsBuffer);
        _pendingEventsBuffer = nullptr;
    }
}

void AlarmCoordinator::reapplyLatchedState() {
    _matrixController.reapplyLatchedState();
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

AlarmCoordinator::EvaluationPassResult AlarmCoordinator::collectPendingEvents(const AlarmInputData& input, uint32_t now) {
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
            currentInput.gpioDigital =
                _gpioService && _gpioService->getLogicalValue(rule.gpioId, logicalValue)
                    ? (logicalValue ? 1.0f : 0.0f)
                    : NAN;
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
        LOGW("Failed to persist retained alarm runtime state");
    }

    return passResult;
}

uint8_t AlarmCoordinator::executePendingEvents(uint8_t pendingCount, const AlarmAggregateState& ledState) {
    _matrixController.update(ledState);

    uint8_t notifiedCount = 0;
    for (uint8_t i = 0; i < pendingCount; i++) {
        PendingEvent& event = _pendingEventsBuffer[i];

        if (event.action.triggerShelly) {
            executeShellyAction(event.ruleSnapshot, event.action.shellyState);
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

    return notifiedCount;
}

uint8_t AlarmCoordinator::executeShellyAction(const AlarmRule& rule, bool turnOn) const {
    if (!_shellyActionExecutor) {
        LOGW("Shelly executor not configured");
        return 0;
    }

    return _shellyActionExecutor(rule, turnOn);
}

uint8_t AlarmCoordinator::process(const ::SensorSnapshot& sensors,
                                  float wifiVariance,
                                  float wifiCsiMotion,
                                  float imuTamper) {
    if (!_manager.isInitialized() || !_pendingEventsBuffer) return 0;

    // Protect the shared pending-events buffer, but keep the timeout short:
    // sensor and WiFi sensing are producer hot paths, so dropping one pass is
    // cheaper than letting alarm bookkeeping back-pressure the main loop.
    SYSTEM::ScopeLock processLock(_processMutex, pdMS_TO_TICKS(kAlarmEvalMutexTimeoutMs));
    if (!processLock.isLocked()) {
        LOGW("Process mutex timeout - skipping evaluation");
        return 0;
    }

    const AlarmInputData input = buildInputData(sensors, wifiVariance, wifiCsiMotion, imuTamper);
    const EvaluationPassResult passResult = collectPendingEvents(input, millis());

    if (!passResult.ready) {
        return 0;
    }

    if (!passResult.hasRules) {
        clearLatchedLed();
        return 0;
    }

    return executePendingEvents(passResult.pendingCount, passResult.ledState);
}

} // namespace ALARMS
