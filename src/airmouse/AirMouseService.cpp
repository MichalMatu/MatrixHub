#include "AirMouseService.h"

#include "../sensors/imu/ImuService.h"
#include "../sensors/imu/ImuManager.h"
#include "../system/logging/Logging.h"
#include "../config/System.h"
#include "../system/utils/ScopeLock.h"
#include "../system/watchdog/TaskWatchdog.h"
#include "../macros/MacroService.h"

#undef LOG_TAG
#define LOG_TAG "AirMouse"

namespace AIRMOUSE {

namespace {

bool shouldRunRuntimeTask(AirMouseConfigAdapter* configAdapter,
                          AirMouseTask* task,
                          bool& outRuntimeCfgValid,
                          AirMouseConfigAdapter::RuntimeCfg& outRuntimeCfg) {
    outRuntimeCfgValid = false;
    if (!configAdapter) {
        return false;
    }

    if (!configAdapter->getRuntimeConfig(outRuntimeCfg)) {
        return task && task->isRunning();
    }

    outRuntimeCfgValid = true;
    const bool movementEnabled = configAdapter->isMovementEnabled();
    const bool sensorClickEnabled =
        configAdapter->isClickEnabled() &&
        outRuntimeCfg.clickSource == RTC::ClickSource::SENSOR;
    const bool jigglerEnabled =
        outRuntimeCfg.jigglerMode != RTC::MouseJigglerMode::JIGGLER_OFF;

    return movementEnabled || sensorClickEnabled || jigglerEnabled;
}

} // namespace

AirMouseService::AirMouseService(ImuService* imuService) : _imuService(imuService) {
    // Always register the mouse descriptor, even if the service is disabled.
    // AirMouse runtime is USB-HID-only today, so we keep the descriptor stable
    // and do not expose a fake transport selector back through API/UI anymore.
    // This also prevents Serial/CDC port churn in the OS.
    _mouse.begin();
    
    // Adapter bridges RTC config into thread-safe runtime settings.
    _configAdapter = new (std::nothrow) AirMouseConfigAdapter();
}

AirMouseService::~AirMouseService() {
    stop();

    if (_clickHandler) { delete _clickHandler; _clickHandler = nullptr; }
    if (_task) { delete _task; _task = nullptr; }
    if (_controller) { delete _controller; _controller = nullptr; }
    
    if (_actuator) { delete _actuator; _actuator = nullptr; }
    if (_keyActuator) { delete _keyActuator; _keyActuator = nullptr; }
    if (_timeProvider) { delete _timeProvider; _timeProvider = nullptr; }

    if (_configAdapter) { delete _configAdapter; _configAdapter = nullptr; }

    if (_hidMutex)    { vSemaphoreDelete(_hidMutex);    _hidMutex    = nullptr; }
}

bool AirMouseService::begin(KEYBOARD::KeyboardService* keyboardService, SYSTEM::TaskWatchdog* watchdog) {
    bool runtimeCfgValid = false;
    AirMouseConfigAdapter::RuntimeCfg runtimeCfg{};
    bool shouldRunTask = false;

    if (isRunning()) {
        LOGW("Already running");
        return true;
    }

    // Store dependencies provided by the system layer.
    _watchdog = watchdog;

    // Pre-allocation dependency checks — nothing to clean up yet.
    if (!keyboardService) {
        LOGE("KeyboardService dependency missing!");
        return false;
    }
    _keyboardService = keyboardService;

    if (!_imuService) {
        LOGE("IMU dependency missing!");
        return false;
    }

    if (!_configAdapter || !_configAdapter->begin()) {
        LOGE("Failed to begin config adapter");
        return false;
    }
    _configAdapter->applySettings();

    // --- From this point, any failure must go through cleanup ---

    if (!_hidMutex) {
        _hidMutex = xSemaphoreCreateMutex();
        if (!_hidMutex) {
            LOGE("Failed to create HID mutex");
            goto cleanup;
        }
    }

    // Actuators encapsulate HID access (mouse/keyboard) and time/random for jiggler.
    if (!_actuator) _actuator = new (std::nothrow) MouseActuator(_mouse);
    if (!_keyActuator) _keyActuator = new (std::nothrow) KeyboardActuator(*_keyboardService);
    if (!_timeProvider) _timeProvider = new (std::nothrow) SystemTimeProvider();
    
    if (!_actuator || !_keyActuator || !_timeProvider) {
        LOGE("Failed to allocate actuators");
        goto cleanup;
    }

    // Controller handles IMU->movement, tap detection, and jiggler updates.
    if (!_controller) {
        _controller = new (std::nothrow) AirMouseController(_configAdapter, _imuService, _imuManager, _actuator, _keyActuator, _timeProvider, _hidMutex);
        if (!_controller || !_controller->begin()) {
            LOGE("Failed to create or begin controller");
            goto cleanup;
        }
    }

    // Task owns the main loop and timing.
    if (!_task) {
        _task = new (std::nothrow) AirMouseTask(_controller, _configAdapter);
        if (!_task) {
            LOGE("Failed to allocate task");
            goto cleanup;
        }
    }

    if (!_clickHandler) {
        _clickHandler = new (std::nothrow) ClickHandler(_actuator, _macroService, _hidMutex, _configAdapter);
        if (!_clickHandler) {
            LOGE("Failed to allocate click handler");
            goto cleanup;
        }
    }
    if (_controller) {
        _controller->setClickHandler(_clickHandler);
    }

    // Apply config after all dependencies are wired.
    // This lazily starts the runtime task only when movement, sensor-click,
    // or jiggler actually needs it.
    applySettings();

    shouldRunTask =
        shouldRunRuntimeTask(_configAdapter, _task, runtimeCfgValid, runtimeCfg);
    if (shouldRunTask && (!_task || !_task->isRunning())) {
        LOGE("Failed to start AirMouse Task");
        goto cleanup;
    }

    return true;

cleanup:
    // Free any partially-allocated resources (mirrors destructor logic)
    if (_clickHandler) { delete _clickHandler; _clickHandler = nullptr; }
    if (_task) { delete _task; _task = nullptr; }
    if (_controller) { delete _controller; _controller = nullptr; }
    if (_actuator) { delete _actuator; _actuator = nullptr; }
    if (_keyActuator) { delete _keyActuator; _keyActuator = nullptr; }
    if (_timeProvider) { delete _timeProvider; _timeProvider = nullptr; }
    if (_hidMutex) { vSemaphoreDelete(_hidMutex); _hidMutex = nullptr; }
    LOGE("begin() failed — all resources cleaned up");
    return false;
}

void AirMouseService::stop() {
    if (_task) {
        _task->stop();
    }
    
    // Inform IMU manager that this consumer is inactive (movement/tap).
    if (_imuManager) {
        _imuManager->setConsumerActive(IMU::Consumer::AirMouseMovement, false);
        _imuManager->setConsumerActive(IMU::Consumer::AirMouseClick, false);
    }
}

void AirMouseService::setEnabled(bool enabled) {
    LOGI("AirMouse %s", enabled ? "active" : "paused");
    // Handled intrinsically by Config/Task right now, although legacy just logged.
    // Ensure IMU manager gets the right state when paused externally.
}

void AirMouseService::setMacroService(MACROS::MacroService* macroService) {
    _macroService = macroService;
    if (_clickHandler) {
        _clickHandler->setMacroService(macroService);
    }
}

void AirMouseService::setPhysicalButtonDoubleClickWindowSink(std::function<void(uint32_t)> sink) {
    if (_configAdapter) {
        // ConfigAdapter remains the place where RTC config changes fan out to
        // runtime consumers; this setter only exposes the physical-button hook.
        _configAdapter->setPhysicalButtonDoubleClickWindowSink(std::move(sink));
    }
}

bool AirMouseService::isEnabled() const {
    if (!_configAdapter) return false;
    return _configAdapter->isMovementEnabled() || _configAdapter->isClickEnabled();
}

bool AirMouseService::isRunning() const {
    return _task && _task->isRunning();
}

bool AirMouseService::isCalibrating() const {
    return _controller && _controller->isCalibrating();
}

void AirMouseService::getDebugSnapshot(AirMouseDebugSnapshot& snapshot) const {
    if (_controller) {
        _controller->getDebugSnapshot(snapshot);
    } else {
        snapshot = AirMouseDebugSnapshot{};
    }
}

void AirMouseService::calibrate() {
    if (_controller) _controller->calibrate();
}

void AirMouseService::setImuCallback(ImuCallback cb) {
    if (_controller) _controller->setImuCallback(std::move(cb));
}

void AirMouseService::applySettings() {
    if (_configAdapter) {
        _configAdapter->applySettings();

        AirMouseConfigAdapter::RuntimeCfg runtimeCfg{};
        bool runtimeCfgValid = false;
        const bool shouldRunTask =
            shouldRunRuntimeTask(_configAdapter, _task, runtimeCfgValid, runtimeCfg);

        if (_task) {
            if (shouldRunTask && !_task->isRunning()) {
                if (!_task->begin(_watchdog)) {
                    LOGE("Failed to start AirMouse task after config update");
                }
            } else if (!shouldRunTask && _task->isRunning()) {
                _task->stop();
            }
        }

        // Pass consumer active state up to IMU Manager here
        // to decouple the controller from the IMU Manager directly.
        if (_imuManager) {
            const bool taskRunning = _task && _task->isRunning();
            const bool mov = taskRunning && _configAdapter->isMovementEnabled();
            _imuManager->setConsumerActive(IMU::Consumer::AirMouseMovement, mov);

            if (!runtimeCfgValid && !_configAdapter->getRuntimeConfig(runtimeCfg)) {
                LOGW("Runtime config snapshot timed out - forcing AirMouse IMU click consumer off");
                _imuManager->setConsumerActive(IMU::Consumer::AirMouseClick, false);
                return;
            }

            // IMU is needed only if click source is SENSOR (tap).
            const bool clk = taskRunning &&
                             _configAdapter->isClickEnabled() &&
                             (runtimeCfg.clickSource == RTC::ClickSource::SENSOR);
            _imuManager->setConsumerActive(IMU::Consumer::AirMouseClick, clk);
        }
    }
}

void AirMouseService::getGyroOffsets(float& x, float& y, float& z) {
    if (_controller) _controller->getGyroOffsets(x, y, z);
    else { x = y = z = 0; }
}

float AirMouseService::getLastDeltaG() {
    if (_controller) return _controller->getLastDeltaG();
    return 0.0f;
}

void AirMouseService::getLastImuData(float& gx, float& gy, float& gz, float& ax, float& ay, float& az) {
    if (_controller) _controller->getLastImuData(gx, gy, gz, ax, ay, az);
    else { gx = gy = gz = ax = ay = az = 0; }
}

void AirMouseService::move(int x, int y) {
    if (_actuator && _hidMutex) {
        SYSTEM::ScopeLock lock(_hidMutex, pdMS_TO_TICKS(CONFIG::AIR_MOUSE::HID::LOCK_TIMEOUT_MS));
        if (lock.isLocked()) {
            _actuator->move(x, y);
        }
    }
}

void AirMouseService::click(uint8_t button) {
    if (_actuator && _hidMutex) {
        SYSTEM::ScopeLock lock(_hidMutex, pdMS_TO_TICKS(CONFIG::AIR_MOUSE::HID::LOCK_TIMEOUT_MS));
        if (lock.isLocked()) {
            _actuator->click(button);
        }
    }
}

void AirMouseService::handleButtonClick(uint8_t clickCount) {
    // Physical button -> configured action (mouse click or macro).
    if (_controller && clickCount > 0) {
        _controller->notifyUserActivity();
    }

    if (_clickHandler) {
        _clickHandler->handleButtonClick(clickCount);
    }
}

} // namespace AIRMOUSE
