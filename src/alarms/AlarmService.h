#pragma once

#include <Arduino.h>
#include "core/AlarmRuleManager.h"
#include "core/AlarmCoordinator.h"
#include "core/CsiAlarmEdgeLatch.h"
#include "core/GpioAlarmEdgeMailbox.h"
#include "types/AlarmInputData.h"
#include <utility>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace BLE { class BleService; }
namespace MATRIX_MANAGER { class MatrixManagerService; }
namespace GPIO { class GpioService; }

namespace ALARMS {

/**
 * @brief Central alarm service (Facade).
 * 
 * Coordinates Rule Management and Evaluation Logic.
 * Managed by ServiceRegistry (Dependency Injection).
 */
class AlarmService {
public:
    // static AlarmService& instance(); // REMOVED: Use Dependency Injection
    
    /**
     * @brief Initialize the service
     */
    bool begin();
    
    /**
     * @brief Re-apply current latched LED/Matrix state.
     * Useful when display settings (mode, brightness) change.
     */
    void reapplyLatchedState();
    
    /**
     * @brief Check if an alarm LED indication is currently active.
     */
    bool isAlarmLatched() const;
    
    /**
     * @brief Update rules with new set and persist them.
     * Applies a new in-memory ruleset and executes required external side effects.
     * Returns false when the manager could not apply the update.
     */
    bool updateRules(const AlarmRule* rules, uint8_t count);
    
    /**
     * @brief Merge new input into the latest alarm snapshot.
     * Producers call this on their hot path; it does not execute rules inline.
     * The intent is to keep sensor/WiFi collection deterministic and let one
     * central caller decide when the alarm pipeline should actually run.
     */
    bool submitInput(const AlarmInputData& input);
    bool submitGpioInput(const char* gpioId, bool logicalValue);
    float getLastImuTamperValue() const;
    bool isSourceTriggered(AlarmSource source);

    /**
     * @brief Execute one pending alarm evaluation pass, if new input arrived.
     * Consumes the latest merged snapshot only once; older intermediate updates
     * are intentionally coalesced instead of forming a backlog.
     * @return Number of notifications/actions triggered
     */
    uint8_t processPending();

    /**
     * @brief Set callback for alarm state changes (WebSocket broadcast).
     */
    void setStateChangeCallback(AlarmStateCallback cb) { _coordinator.setStateChangeCallback(cb); }
    void setShellyActionExecutor(ShellyActionExecutor executor);
    void reconcileShellyOutputs();
    void setNotificationBackend(AlarmNotificationBackend backend) { _coordinator.setNotificationBackend(std::move(backend)); }
    void setGpioService(GPIO::GpioService* service) { _coordinator.setGpioService(service); }
    
    // Direct access to manager if needed (e.g. for locking)
    AlarmRuleManager& getManager() { return _manager; }

#ifdef UNIT_TEST
    bool enqueueShellyReconcileForTest(const char* deviceId) {
        return _coordinator.enqueueShellyReconcileForTest(deviceId);
    }
    uint8_t pendingShellyReconcileCountForTest() {
        return _coordinator.pendingShellyReconcileCountForTest();
    }
#endif

    // Inject Dependencies
    AlarmService(MATRIX_MANAGER::MatrixManagerService* matrixManager, BLE::BleService* ble);

private:
    struct AggregatedAlarmInput {
        SensorSnapshot sensors;
        float wifiVariance = NAN;
        float wifiCsiMotion = NAN;
        float imuTamper = NAN;
    };

    AlarmRuleManager _manager;
    AlarmCoordinator _coordinator;
    // Cache the last merged view from all producers. SensorTaskLoop and
    // WifiSensingTaskRunner update different fields, so we keep one combined
    // snapshot and evaluate alarms against that coherent state later.
    SensorSnapshot _lastSnapshot;
    float _lastWifiVariance = NAN;
    float _lastWifiCsiMotion = NAN;
    float _lastImuTamper = NAN;
    // Dirty flag for the coalesced "latest state available" mailbox model.
    bool _pendingEvaluation = false;
    uint32_t _inputGeneration = 0;
    BooleanAlarmEdgeLatch _csiEdgeLatch;
    BooleanAlarmEdgeLatch _imuEdgeLatch;
    GpioAlarmEdgeMailbox _gpioEdgeMailbox;

    // Cross-task producers only touch this small in-memory snapshot. The
    // expensive alarm pipeline runs outside the lock in processPending().
    mutable portMUX_TYPE _snapshotLock;
};

} // namespace ALARMS
