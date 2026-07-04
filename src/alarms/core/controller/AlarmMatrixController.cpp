#include "AlarmMatrixController.h"
#include "../../../system/matrix_manager/MatrixManagerService.h"
#include "../../types/AlarmConstants.h" // For LED_LATCH_REFRESH_MS

namespace ALARMS {

AlarmMatrixController::AlarmMatrixController() {
}

void AlarmMatrixController::updateDisplay(bool active, AlarmSeverity severity, const char* alarmName) {
    if (!_matrixManager) {
        return;
    }

    RTC::MatrixAlarmMode mode = RTC::MatrixAlarmMode::SCROLL_TEXT;
    RTC::withConfig([&](const RTC::ConfigStore& cfg) {
        mode = cfg.matrix.alarmMode;
    });

    AlarmDisplaySnapshot snapshot;
    snapshot.active = active;
    snapshot.severity = severity;
    snapshot.alarmName = alarmName;
    const AlarmDisplayResult result = _displayEngine.build(mode, snapshot);

    if (result.clearLayer) {
        _matrixManager->clearLayer(MATRIX_MANAGER::Layer::ALARM);
        return;
    }

    _matrixManager->setLayer(MATRIX_MANAGER::Layer::ALARM, result.content);
}

bool AlarmMatrixController::update(const AlarmAggregateState& ledState) {
    bool doSetLatched = false;
    bool doClearLatched = false;
    AlarmSeverity severityToSet = AlarmSeverity::Info;
    char nameToSet[kMaxAlarmNameLen] = {0};
    uint32_t now = millis();

    portENTER_CRITICAL(&_mutex);
    if (ledState.active) {
        bool shouldRefresh = false;
        
        // Check for state change
        if (!_active || ledState.maxSeverity != _severity) {
            shouldRefresh = true;
        } 
        // Check for periodic refresh (to enforce display over other temporary content)
        else if (_lastRefreshMs == 0 || (now - _lastRefreshMs) >= ALARM::LED_LATCH_REFRESH_MS) {
            shouldRefresh = true;
        }
        
        if (shouldRefresh) {
            _active = true;
            _severity = ledState.maxSeverity;
            _lastRefreshMs = now;
            safeCopyAlarmName(_alarmName, ledState.alarmName);
            
            doSetLatched = true;
            severityToSet = _severity;
            safeCopyAlarmName(nameToSet, _alarmName);
        }
    } else {
        if (_active) {
            _active = false;
            _lastRefreshMs = 0;
            doClearLatched = true;
            severityToSet = _severity;
        }
    }
    portEXIT_CRITICAL(&_mutex);

    if (doSetLatched) {
        updateDisplay(true, severityToSet, nameToSet);
        return true;
    } else if (doClearLatched) {
        updateDisplay(false, severityToSet);
        return true;
    }
    
    return false;
}

void AlarmMatrixController::reapplyLatchedState() {
    bool active = false;
    AlarmSeverity severity = AlarmSeverity::Info;
    char name[kMaxAlarmNameLen] = {0};

    portENTER_CRITICAL(&_mutex);
    if (_active) {
        active = true;
        severity = _severity;
        safeCopyAlarmName(name, _alarmName);
        _lastRefreshMs = millis(); // Reset timer
    }
    portEXIT_CRITICAL(&_mutex);

    if (active) {
        updateDisplay(true, severity, name);
    }
}

void AlarmMatrixController::clearLatchedState() {
    bool wasActive = false;
    
    portENTER_CRITICAL(&_mutex);
    if (_active) {
        _active = false;
        _lastRefreshMs = 0;
        wasActive = true;
    }
    portEXIT_CRITICAL(&_mutex);

    if (wasActive) {
        updateDisplay(false, AlarmSeverity::Info);
    }
}

bool AlarmMatrixController::isLatched() const {
    bool active;
    portENTER_CRITICAL(&_mutex);
    active = _active;
    portEXIT_CRITICAL(&_mutex);
    return active;
}

} // namespace ALARMS
