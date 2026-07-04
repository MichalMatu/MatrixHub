#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <functional>

#include "../config/AirMouseConfigAdapter.h"
#include "../../sensors/imu/ImuService.h"
#include "../../sensors/imu/ImuManager.h"

// Internal components
#include "../processors/TapDetector.h"
#include "../processors/AirMouseProcessor.h"
#include "../features/JigglerController.h"
#include "../managers/MouseActuator.h"
#include "../managers/KeyboardActuator.h"
#include "../managers/SystemTimeProvider.h"

namespace AIRMOUSE {

class ClickHandler;

struct AirMouseDebugSnapshot {
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float offsetZ = 0.0f;
    float gx = 0.0f;
    float gy = 0.0f;
    float gz = 0.0f;
    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    float deltaG = 0.0f;
    uint32_t timestampMs = 0;
};

// Callback for raw IMU data (optional debug)
using ImuCallback = std::function<void(float lgx, float lgy, float lgz, float lax, float lay, float laz, float dG)>;

/**
 * @brief Core Air Mouse logic.
 *
 * Responsibilities:
 * - Convert IMU samples into mouse movement (AirMouseProcessor).
 * - Detect taps for click actions (TapDetector).
 * - Maintain Mouse Jiggler behavior (JigglerController).
 * - Apply runtime config changes from AirMouseConfigAdapter.
 *
 * Threading notes:
 * - processLoopStep() is called from AirMouseTask only.
 * - Calibration requests are queued and executed in the task thread.
 * - HID access is protected by the shared _hidMutex.
 */
class AirMouseController {
public:
    AirMouseController(AirMouseConfigAdapter* configAdapter, 
                       ImuService* imuService, 
                       IMU::ImuManager* imuManager, 
                       MouseActuator* actuator, 
                       KeyboardActuator* keyActuator, 
                       SystemTimeProvider* timeProvider, 
                       SemaphoreHandle_t hidMutex);
    ~AirMouseController();

    bool begin();

    // The main processing step, returns true if should continue running
    void processLoopStep(uint32_t now);

    void setImuCallback(ImuCallback cb);
    
    // Commands called from Task or Facade
    void calibrate();
    void resetProcessorFilters();
    void resetDetector();
    
    // Status accessors
    bool isCalibrating() const { return _calibrationInProgress.load(std::memory_order_acquire); }
    void getDebugSnapshot(AirMouseDebugSnapshot& snapshot) const;
    void getGyroOffsets(float& x, float& y, float& z);
    float getLastDeltaG();
    void getLastImuData(float& gx, float& gy, float& gz, float& ax, float& ay, float& az);
    
    // Jiggler manual trigger
    void notifyUserActivity();
    void setClickHandler(ClickHandler* handler) { _clickHandler = handler; }

private:
                         
    void doCalibration();
    bool updateComponentConfigs();
    void updateJiggler(const AirMouseConfigAdapter::RuntimeCfg& cfgState);
    void updateDebugSnapshot(const AirMouseDebugSnapshot& snapshot);
    void updateDebugOffsets(float x, float y, float z);

    // Dependencies (not owned)
    AirMouseConfigAdapter* _configAdapter;
    ImuService* _imuService;
    IMU::ImuManager* _imuManager;
    MouseActuator* _actuator;
    KeyboardActuator* _keyActuator;
    SystemTimeProvider* _timeProvider;

    // Owned components
    AirMouseProcessor* _processor = nullptr;
    TapDetector* _detector = nullptr;
    JigglerController* _jiggler = nullptr;
    ClickHandler* _clickHandler = nullptr;

    // Mutexes
    SemaphoreHandle_t _hidMutex = nullptr;
    SemaphoreHandle_t _callbackMutex = nullptr;

    // Optional debug callback for raw IMU data
    ImuCallback _imuCallback = nullptr;
    std::atomic<bool> _calibrationInProgress{false};
    std::atomic<bool> _calibrationRequested{false};

    // Cached runtime config — updated only when dirty flag is set
    AirMouseConfigAdapter::RuntimeCfg _cachedRuntimeCfg;
    bool _lastMovementEnabled = true;
    mutable portMUX_TYPE _debugSnapshotLock = portMUX_INITIALIZER_UNLOCKED;
    AirMouseDebugSnapshot _debugSnapshot;
};

} // namespace AIRMOUSE
