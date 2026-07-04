/**
 * @file ThermalMonitor.h
 * @brief Dynamic thermal throttling for ESP32
 * 
 * Monitors core temperature and adjusts CPU frequency to prevent overheating.
 * Uses hysteresis to prevent oscillation between states.
 */

#pragma once

#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace BLE { class BleService; }
namespace POWER { class PowerManager; }
class MatrixService;

namespace SYSTEM {

/**
 * @brief Thermal state levels
 */
enum class ThermalState : uint8_t {
    NORMAL = 0,      // Full performance (240 MHz)
    SOFT_THROTTLE,   // Reduced (160 MHz)
    HARD_THROTTLE,   // Minimum (160 MHz - WebServer stability)
    CRITICAL         // Emergency Shutdown (>85°C)
};

/**
 * @brief Thermal monitoring singleton
 * 
 * Runs a background task that periodically checks temperature
 * and adjusts CPU frequency accordingly.
 *
 * This one is intentionally left as a singleton for now: the device only has
 * one thermal governor, and the class already accepts its collaborators via
 * setter injection. Converting it to a registry-owned service would bring less
 * value than refactoring normal backend workers such as UdpPusher.
 *
 * Leave it that way unless the firmware grows multiple thermal domains or a
 * stronger need for alternate test/runtime instances. Right now "one device,
 * one governor" is the natural ownership model.
 */
class ThermalMonitor {
public:
    static ThermalMonitor& instance();
    
    // Dependency Injection
    void setBleService(BLE::BleService* service) { _bleService = service; }
    void setPowerManager(POWER::PowerManager* pm) { _powerManager = pm; }
    void setMatrixService(MatrixService* matrix) { _matrixService = matrix; }

    /**
     * @brief Start the thermal monitoring task
     * @return true if started successfully
     */
    bool begin();

    /**
     * @brief Stop the thermal monitoring task
     */
    void stop();

    /**
     * @brief Get current thermal state
     */
    ThermalState getState() const { return _state; }

    /**
     * @brief Get last measured temperature (°C)
     */
    float getTemperature() const { return _lastTemp; }

    /**
     * @brief Get current CPU frequency (MHz)
     */
    uint32_t getCpuFrequency() const { return _currentFreq; }

    /**
     * @brief Check if currently throttled
     */
    bool isThrottled() const { return _state != ThermalState::NORMAL; }

protected:
    ThermalMonitor() = default;
    ~ThermalMonitor() = default;
    ThermalMonitor(const ThermalMonitor&) = delete;
    ThermalMonitor& operator=(const ThermalMonitor&) = delete;

    static void taskEntry(void* param);
    void taskLoop();
    void evaluateAndApply(float temp);
    void applyState(ThermalState newState);
    bool reapStoppedTask(TickType_t waitTicks);
    void freeTaskResources();
    void restoreNormalState();
    
    BLE::BleService* _bleService = nullptr;
    POWER::PowerManager* _powerManager = nullptr;
    MatrixService* _matrixService = nullptr;
    volatile ThermalState _state = ThermalState::NORMAL;
    volatile float _lastTemp = 0.0f;
    volatile uint32_t _currentFreq = 240;
    volatile bool _running = false;

    TaskHandle_t _taskHandle = nullptr;
    SemaphoreHandle_t _cleanupSem = nullptr;
    StaticTask_t* _tcb = nullptr;
    StackType_t* _stackBuffer = nullptr;
};

} // namespace SYSTEM
