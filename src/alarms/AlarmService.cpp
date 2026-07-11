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
    if (!_coordinator.isReady()) {
        LOGE("Failed to start service: alarm runtime buffers unavailable");
        return false;
    }

    // Initialize manager from the live PSRAM rule store and the retained RTC
    // runtime summary already restored during boot.
    bool success = _manager.begin();
    
    if (success) {
        _coordinator.refreshLatchedStateFromRuntime();
        LOGI("Service started. Rules: %u", _manager.getCount());
    } else {
        LOGE("Failed to start service");
    }
    
    return success;
}

bool AlarmService::updateRules(const AlarmRule* rules, uint8_t count) {
    if (!_coordinator.updateRules(rules, count)) {
        LOGE("Failed to apply updated alarm rules");
        return false;
    }
    return true;
}

void AlarmService::setShellyActionExecutor(ShellyActionExecutor executor) {
    _coordinator.setShellyActionExecutor(std::move(executor));
    if (_manager.isInitialized()) {
        _coordinator.reconcileAllShellyDevices();
    }
}

void AlarmService::reconcileShellyOutputs() {
    if (_manager.isInitialized()) {
        _coordinator.reconcileAllShellyDevices();
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
        _csiEdgeLatch.submit(inputData.wifiCsiMotion > 0.5f);
        hasUpdates = true;
    }
    if (!std::isnan(inputData.imuTamper)) {
        _lastImuTamper = inputData.imuTamper;
        _imuEdgeLatch.submit(inputData.imuTamper > 0.5f);
        hasUpdates = true;
    }

    if (hasUpdates) {
        _lastSnapshot.timestamp_ms = millis();
        _pendingEvaluation = true;
        _inputGeneration++;
    }

    portEXIT_CRITICAL(&_snapshotLock);

    return hasUpdates;
}

bool AlarmService::submitGpioInput(const char* gpioId, bool logicalValue) {
    // GpioService calls this from its polling task after debounce. The mailbox
    // is fixed-size and the critical section performs no allocation, logging,
    // rule lookup or external I/O.
    portENTER_CRITICAL(&_snapshotLock);
    const bool accepted = _gpioEdgeMailbox.submit(gpioId, logicalValue);
    if (accepted) {
        _lastSnapshot.timestamp_ms = millis();
        _pendingEvaluation = true;
        ++_inputGeneration;
    }
    portEXIT_CRITICAL(&_snapshotLock);

    return accepted;
}

float AlarmService::getLastImuTamperValue() const {
    portENTER_CRITICAL(&_snapshotLock);
    const float value = _lastImuTamper;
    portEXIT_CRITICAL(&_snapshotLock);
    return value;
}

bool AlarmService::isSourceTriggered(AlarmSource source) {
    // Boot-time detector restore must not turn a transient manager-lock delay
    // into an authoritative false that clears retained CSI/IMU state. Callers
    // use this only during service wiring/tests, so wait for the definitive
    // snapshot instead of applying the hot-path evaluation timeout.
    SYSTEM::ScopeLock lock(_manager.getMutex(), portMAX_DELAY);
    if (!lock.isLocked()) {
        return false;
    }

    const AlarmRule* rules = _manager.getRules();
    AlarmRuntimeState* states = _manager.getStates();
    const uint8_t count = _manager.getCount();
    for (uint8_t i = 0; i < count; ++i) {
        if (rules[i].enabled &&
            rules[i].source == source &&
            states[i].previouslyTriggered) {
            return true;
        }
    }
    return false;
}

uint8_t AlarmService::processPending() {
    // Shelly admission failures have their own fixed outbox, so they make
    // progress even when no new sensor/CSI/GPIO sample arrives.
    _coordinator.retryPendingShellyActions();

    AggregatedAlarmInput input;
    GpioAlarmEdgeMailbox::PassSnapshot gpioEdges;
    bool hasPending = false;
    uint32_t generation = 0;
    BooleanAlarmEdgeLatch::PendingDecision csiDecision =
        BooleanAlarmEdgeLatch::PendingDecision::None;
    BooleanAlarmEdgeLatch::PendingDecision imuDecision =
        BooleanAlarmEdgeLatch::PendingDecision::None;

    // Peek one coherent snapshot. It is acknowledged only after the coordinator
    // confirms that evaluation actually ran, so a transient lock timeout cannot
    // consume the alarm pass.
    portENTER_CRITICAL(&_snapshotLock);
    if (_pendingEvaluation || _csiEdgeLatch.hasPending() ||
        _imuEdgeLatch.hasPending() || _gpioEdgeMailbox.hasPending()) {
        input.sensors = _lastSnapshot;
        input.wifiVariance = _lastWifiVariance;
        input.wifiCsiMotion = _lastWifiCsiMotion;
        input.imuTamper = _lastImuTamper;
        csiDecision = _csiEdgeLatch.next();
        if (csiDecision == BooleanAlarmEdgeLatch::PendingDecision::Rising) {
            input.wifiCsiMotion = 1.0f;
        } else if (csiDecision == BooleanAlarmEdgeLatch::PendingDecision::Clear) {
            input.wifiCsiMotion = 0.0f;
        }
        imuDecision = _imuEdgeLatch.next();
        if (imuDecision == BooleanAlarmEdgeLatch::PendingDecision::Rising) {
            input.imuTamper = 1.0f;
        } else if (imuDecision == BooleanAlarmEdgeLatch::PendingDecision::Clear) {
            input.imuTamper = 0.0f;
        }
        _gpioEdgeMailbox.peek(gpioEdges);
        generation = _inputGeneration;
        hasPending = true;
    }
    portEXIT_CRITICAL(&_snapshotLock);

    if (!hasPending) {
        return 0;
    }

    bool evaluationCompleted = false;
    const uint8_t notifications = _coordinator.process(
        input.sensors,
        input.wifiVariance,
        input.wifiCsiMotion,
        input.imuTamper,
        &gpioEdges,
        &evaluationCompleted);
    if (!evaluationCompleted) {
        return notifications;
    }

    portENTER_CRITICAL(&_snapshotLock);
    _csiEdgeLatch.complete(csiDecision);
    _imuEdgeLatch.complete(imuDecision);
    _gpioEdgeMailbox.complete(gpioEdges);
    if (_inputGeneration == generation) {
        _pendingEvaluation = false;
    }
    if (_csiEdgeLatch.hasPending() || _imuEdgeLatch.hasPending() ||
        _gpioEdgeMailbox.hasPending()) {
        _pendingEvaluation = true;
    }
    portEXIT_CRITICAL(&_snapshotLock);

    return notifications;
}

} // namespace ALARMS
