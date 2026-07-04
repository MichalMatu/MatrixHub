#pragma once

#include "AlarmRuleManager.h"
#include "controller/AlarmMatrixController.h"
#include "../notifier/AlarmNotifier.h"
#include "../../sensors/model/SensorTypes.h" // For SensorSnapshot
#include "../types/AlarmConstants.h"
#include <cmath>
#include <functional>
#include <utility>

namespace MATRIX_MANAGER { class MatrixManagerService; } // Forward declaration

namespace BLE { class BleService; }
namespace GPIO { class GpioService; }

namespace ALARMS {

// Callback for alarm state changes (for WebSocket broadcast)
struct AlarmStateChange {
    char id[kMaxIdLen];     // Stable rule identifier
    bool triggered;         // Current triggered state
    float currentValue;     // Current sensor value
    AlarmSeverity severity; // Alarm severity
};

using AlarmStateCallback = std::function<void(const AlarmStateChange&)>;
using ShellyActionExecutor = std::function<uint8_t(const AlarmRule&, bool)>;

/**
 * @brief Coordinates the alarm evaluation process.
 * 
 * Takes sensor data, iterates over rules (via Manager), calls Evaluator,
 * and dispatches notifications via Notifier.
 * 
 * Thread-safe: Can be called from multiple tasks simultaneously.
 * Uses PSRAM-allocated buffer internally for efficiency.
 */
class AlarmCoordinator {
public:
    AlarmCoordinator(AlarmRuleManager& manager);
    ~AlarmCoordinator();
    
    /**
     * @brief Main evaluation loop.
     * @return Number of notifications sent/pending
     */
    uint8_t process(const SensorSnapshot& sensors,
                    float wifiVariance,
                    float wifiCsiMotion = NAN,
                    float imuTamper = NAN);
    
    // LED Latching logic helpers (exposed if needed by Service to clear LED on reload)
    void clearLatchedLed();
    void reapplyLatchedState();
    bool isAlarmLatched() const;
    // Set callback for state changes (WebSocket broadcast)
    void setStateChangeCallback(AlarmStateCallback cb) { _stateChangeCb = cb; }
    void setShellyActionExecutor(ShellyActionExecutor executor) { _shellyActionExecutor = std::move(executor); }
    void setNotificationBackend(AlarmNotificationBackend backend) { _notifier.setBackend(std::move(backend)); }

    // Inject Dependencies
    void setBleService(BLE::BleService* service) { _bleService = service; }
    void setGpioService(GPIO::GpioService* service) { _gpioService = service; }
    void setMatrixManager(MATRIX_MANAGER::MatrixManagerService* mgr) { _matrixController.setMatrixManager(mgr); }

private:
    struct EvaluationPassResult {
        AlarmAggregateState ledState;
        uint8_t pendingCount = 0;
        bool ready = false;
        bool hasRules = false;
    };

    struct PendingEvent {
        AlarmRule ruleSnapshot;
        EvaluationResult evalResult;
        AlarmAction action;
        bool shouldBroadcastState = false;
        AlarmStateChange stateChangeMsg;
    };

    AlarmRuleManager& _manager;
    AlarmNotifier _notifier;
    AlarmStateCallback _stateChangeCb = nullptr;
    ShellyActionExecutor _shellyActionExecutor = nullptr;
    BLE::BleService* _bleService = nullptr;
    GPIO::GpioService* _gpioService = nullptr;
    
    // Delegated controller for Matrix LED logic
    AlarmMatrixController _matrixController;

    // Buffer for evaluation results to avoid frequent heap allocation.
    // Allocated in PSRAM if available.
    PendingEvent* _pendingEventsBuffer = nullptr;
    SemaphoreHandle_t _processMutex = nullptr;
    StaticSemaphore_t _processMutexStorage;

    AlarmInputData buildInputData(const SensorSnapshot& sensors,
                                  float wifiVariance,
                                  float wifiCsiMotion,
                                  float imuTamper) const;
    EvaluationPassResult collectPendingEvents(const AlarmInputData& input, uint32_t now);
    uint8_t executePendingEvents(uint8_t pendingCount, const AlarmAggregateState& ledState);
    uint8_t executeShellyAction(const AlarmRule& rule, bool turnOn) const;
    static float getValueForLogging(const AlarmRule& rule, const AlarmInputData& input);
};

} // namespace ALARMS
