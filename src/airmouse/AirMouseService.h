#pragma once

#include <Arduino.h>
#include <USBHIDMouse.h>
#include <USBHIDKeyboard.h>
#include <functional>
#include <atomic>

#include "config/AirMouseConfigAdapter.h"
#include "core/AirMouseController.h"
#include "core/AirMouseTask.h"
#include "features/ClickHandler.h"

#include "managers/MouseActuator.h"
#include "managers/KeyboardActuator.h"
#include "managers/SystemTimeProvider.h"

// External forward declarations
class ImuService;
namespace KEYBOARD { class KeyboardService; }
namespace SYSTEM { class TaskWatchdog; }
namespace MACROS { class MacroService; }
namespace IMU { class ImuManager; }

namespace AIRMOUSE {

/**
 * @brief Air Mouse service facade and lifetime owner.
 *
 * Responsibilities:
 * - Creates and owns controller/task/actuators/config adapter.
 * - Bridges external services (IMU, Keyboard, Macro, Watchdog).
 * - Exposes simple control/status API for the rest of the system.
 *
 * Key dependencies:
 * - ImuService: provides cached IMU samples (gyro/acc).
 * - IMU::ImuManager: global IMU consumer flags (movement/click).
 * - KEYBOARD::KeyboardService: USB HID keyboard injection for jiggler/shortcuts.
 * - MACROS::MacroService: optional script execution on button clicks.
 * - SYSTEM::TaskWatchdog: task registration/unregistration.
 *
 * Threading notes:
 * - AirMouseTask runs the main loop on a dedicated FreeRTOS task.
 * - USB HID access is protected by _hidMutex (shared by mouse and click handler).
 */
class AirMouseService {
public:
    AirMouseService(ImuService* imuService);
    ~AirMouseService();

    bool begin(KEYBOARD::KeyboardService* keyboardService, SYSTEM::TaskWatchdog* watchdog);
    void stop();

    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    // Dependency Injection for Macro execution
    void setMacroService(MACROS::MacroService* macroService);
    // Physical button timing stays outside AirMouse ownership. This setter only
    // forwards a config sink so button click-window updates can leave the
    // service without introducing a ButtonHandler dependency back here.
    void setPhysicalButtonDoubleClickWindowSink(std::function<void(uint32_t)> sink);
    
    // Status
    bool isRunning() const;
    bool isCalibrating() const;
    void getDebugSnapshot(AirMouseDebugSnapshot& snapshot) const;
    float getLastDeltaG();
    void getLastImuData(float& gx, float& gy, float& gz, float& ax, float& ay, float& az);

    void calibrate();

    void setImuCallback(ImuCallback cb);
    void setImuManager(IMU::ImuManager* manager) { _imuManager = manager; }

    void getGyroOffsets(float& x, float& y, float& z);

    // Consolidated method to apply settings from RTC/Config
    void applySettings();

    // Control Methods
    void move(int x, int y);
    void click(uint8_t button);

    /**
     * @brief Handle a physical button click gesture
     * @param clickCount Number of accumulated clicks (1=single, 2=double, 3=triple)
     */
    void handleButtonClick(uint8_t clickCount);

private:

    USBHIDMouse _mouse;
    KEYBOARD::KeyboardService* _keyboardService = nullptr;
    MACROS::MacroService* _macroService = nullptr;
    ImuService* _imuService = nullptr;
    IMU::ImuManager* _imuManager = nullptr;
    SYSTEM::TaskWatchdog* _watchdog = nullptr;
    
    // Protects USB HID device access across task/callbacks
    SemaphoreHandle_t _hidMutex = nullptr;

    MouseActuator* _actuator = nullptr;
    KeyboardActuator* _keyActuator = nullptr;
    SystemTimeProvider* _timeProvider = nullptr;

    AirMouseConfigAdapter* _configAdapter = nullptr;
    AirMouseController* _controller = nullptr;
    AirMouseTask* _task = nullptr;
    ClickHandler* _clickHandler = nullptr;
};

} // namespace AIRMOUSE
