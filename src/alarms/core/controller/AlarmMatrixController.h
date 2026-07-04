#pragma once

#include "display/AlarmDisplayEngine.h"
#include "../../../system/rtc/RtcConfig.h" // For MatrixAlarmMode

namespace MATRIX_MANAGER { class MatrixManagerService; } // Forward declaration
#include "../../types/AlarmEnums.h" // For AlarmSeverity
// Cannot include AlarmLogic.h here due to possible circular deps if Logic needs Controller someday, 
// but currently Logic is pure. However, AggregateState is in Logic.
// Let's forward declare or include Logic if needed for structs.
#include "../AlarmLogic.h" 

namespace ALARMS {

/**
 * @brief Controller for Matrix LED display in context of Alarms
 * 
 * Handles:
 * - Latching logic (keeping highest severity alarm displayed)
 * - Interfacing with MatrixService
 * - Refreshing display locally
 */
class AlarmMatrixController {
public:
    AlarmMatrixController();
    void setMatrixManager(MATRIX_MANAGER::MatrixManagerService* mgr) { _matrixManager = mgr; }

    /**
     * @brief Update the matrix display based on desired state
     * 
     * @param active Whether an alarm is active
     * @param severity Severity to display
     * @param alarmName Name of the alarm (for SCROLL_TEXT mode)
     */
    void updateDisplay(bool active, AlarmSeverity severity, const char* alarmName = nullptr);

    /**
     * @brief Core loop method to maintain latched state (refresh if needed)
     * 
     * @param ledState Aggregated state from all rules
     * @return true if a display update occurred
     */
    bool update(const AlarmAggregateState& ledState);

    /**
     * @brief Force re-application of current latched state to Matrix
     * (Useful after MatrixService restart or deep sleep wake)
     */
    void reapplyLatchedState();

    /**
     * @brief Clear valid latched state
     */
    void clearLatchedState();

    /**
     * @brief Check if any alarm is currently latched on display
     */
    bool isLatched() const;

private:
    mutable portMUX_TYPE _mutex = portMUX_INITIALIZER_UNLOCKED;
    
    // Latched state
    bool _active = false;
    AlarmSeverity _severity = AlarmSeverity::Info;
    uint32_t _lastRefreshMs = 0;
    char _alarmName[kAlarmNameBufferLen] = {0};

    AlarmDisplayEngine _displayEngine;
    MATRIX_MANAGER::MatrixManagerService* _matrixManager = nullptr;
};

} // namespace ALARMS
