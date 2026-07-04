#include "AlarmService.h"
#include "../system/rtc/RtcConfig.h"
#include "../system/logging/Logging.h"
#include "../system/utils/ScopeLock.h"
#include <algorithm>
#include <cmath>
#include <utility>

#undef LOG_TAG
#define LOG_TAG "AlarmService"

namespace ALARMS {

AlarmService::AlarmService(MATRIX_MANAGER::MatrixManagerService* matrixManager, BLE::BleService* ble) 
    : _manager(),
      _coordinator(_manager),
      _lastWifiVariance(NAN),
      _lastWifiCsiMotion(NAN),
      _snapshotLock(portMUX_INITIALIZER_UNLOCKED) {
    _lastSnapshot.temp = NAN;
    _lastSnapshot.humid = NAN;
    _coordinator.setMatrixManager(matrixManager);
    _coordinator.setBleService(ble);
}

bool AlarmService::begin() {
    // Initialize manager from the live PSRAM rule store and the retained RTC
    // runtime summary already restored during boot.
    bool success = _manager.begin();
    
    if (success) {
        LOGI("Service started. Rules: %u", _manager.getCount());
    } else {
        LOGE("Failed to start service");
    }
    
    return success;
}

bool AlarmService::reloadRules() {
    bool result = _manager.reloadRules();
    
    if (result) {
        // Also clear any latched LED since rules changed
        _coordinator.clearLatchedLed();
    }
    
    return result;
}

bool AlarmService::updateRules(const AlarmRule* rules, uint8_t count) {
    AlarmRuleUpdateEffects effects = _manager.updateRules(rules, count);
    if (!effects.applied) {
        LOGE("Failed to apply updated alarm rules");
        return false;
    }

    if (_shellyActionExecutor) {
        for (uint8_t i = 0; i < effects.shellyOffCount; i++) {
            _shellyActionExecutor(effects.shellyOffRules[i], false);
        }
    } else if (effects.shellyOffCount > 0) {
        LOGW("Shelly executor not configured for disabled rule cleanup");
    }
    
    // Clear latched LED state
    _coordinator.clearLatchedLed();
    return true;
}

void AlarmService::setShellyActionExecutor(ShellyActionExecutor executor) {
    _shellyActionExecutor = std::move(executor);
    _coordinator.setShellyActionExecutor(_shellyActionExecutor);
}

void AlarmService::resetRuntimeState() {
    SYSTEM::ScopeLock lock(_manager.getMutex(), pdMS_TO_TICKS(kAlarmMutexTimeoutMs));
    if (lock.isLocked()) {
        if (_manager.resetRuntimeStateLocked()) {
            LOGI("Runtime state reset");
        } else {
            LOGW("Failed to persist runtime state reset");
        }
    } else {
        LOGW("Mutex timeout during reset");
    }
}

void AlarmService::reapplyLatchedState() {
    _coordinator.reapplyLatchedState();
}

bool AlarmService::isAlarmLatched() const {
    return _coordinator.isAlarmLatched();
}

bool AlarmService::submitInput(const AlarmInputData& inputData) {
    bool hasUpdates = false;

    // Producers only merge fresh values into the latest shared snapshot. This
    // section stays intentionally tiny so sensor/WiFi hot paths never iterate
    // rules, fire callbacks, or enqueue notification work inline.
    portENTER_CRITICAL(&_snapshotLock);

    if (!std::isnan(inputData.temperature)) {
        _lastSnapshot.temp = inputData.temperature;
        hasUpdates = true;
    }
    if (!std::isnan(inputData.humidity)) {
        _lastSnapshot.humid = inputData.humidity;
        hasUpdates = true;
    }
    if (!std::isnan(inputData.co2)) {
        const float clamped = std::max(0.0f, std::min(inputData.co2, 65535.0f));
        _lastSnapshot.co2 = static_cast<uint16_t>(clamped);
        hasUpdates = true;
    }
    if (!std::isnan(inputData.wifiVariance)) {
        _lastWifiVariance = inputData.wifiVariance;
        hasUpdates = true;
    }
    if (!std::isnan(inputData.wifiCsiMotion)) {
        _lastWifiCsiMotion = inputData.wifiCsiMotion;
        hasUpdates = true;
    }
    if (!std::isnan(inputData.imuTamper)) {
        _lastImuTamper = inputData.imuTamper;
        hasUpdates = true;
    }

    if (hasUpdates) {
        _lastSnapshot.timestamp_ms = millis();
        _pendingEvaluation = true;
    }

    portEXIT_CRITICAL(&_snapshotLock);

    return hasUpdates;
}

float AlarmService::getLastImuTamperValue() const {
    portENTER_CRITICAL(&_snapshotLock);
    const float value = _lastImuTamper;
    portEXIT_CRITICAL(&_snapshotLock);
    return value;
}

uint8_t AlarmService::processPending() {
    AggregatedAlarmInput input;
    bool hasPending = false;

    // Take one coherent snapshot and clear the dirty flag before running the
    // alarm pipeline. Any new producer update that arrives while processing is
    // simply marked pending for the next pass instead of blocking the caller.
    portENTER_CRITICAL(&_snapshotLock);
    if (_pendingEvaluation) {
        input.sensors = _lastSnapshot;
        input.wifiVariance = _lastWifiVariance;
        input.wifiCsiMotion = _lastWifiCsiMotion;
        input.imuTamper = _lastImuTamper;
        _pendingEvaluation = false;
        hasPending = true;
    }
    portEXIT_CRITICAL(&_snapshotLock);

    if (!hasPending) {
        return 0;
    }

    // All rule evaluation and side effects are centralized here on purpose.
    // This keeps producers lightweight and makes the execution model easier to
    // reason about than scattered synchronous alarm runs from multiple tasks.
    return _coordinator.process(input.sensors, input.wifiVariance, input.wifiCsiMotion, input.imuTamper);
}

} // namespace ALARMS
