/**
 * @file ThermalMonitor.cpp
 * @brief Thermal throttling implementation
 * 
 * Background task monitors temperature and adjusts CPU frequency.
 * Uses internal DRAM for the task stack because this task must remain
 * cache-disable safe.
 */

#include "ThermalMonitor.h"
#include "../../config/Hardware.h"
#include "../../config/System.h"
#include "../logging/Logging.h"
#include <MatrixService.h>
#include "../../system/power/PowerManager.h"
#ifndef NATIVE_BUILD
#include <NimBLEDevice.h>
#include "../../ble/BleService.h"
#endif
#include "../../system/rtc/RtcConfig.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#undef LOG_TAG
#define LOG_TAG "Thermal"

namespace {

bool waitForTaskSuspended(TaskHandle_t taskHandle, SemaphoreHandle_t cleanupSem, TickType_t waitTicks) {
    if (taskHandle == nullptr) {
        return true;
    }

    if (eTaskGetState(taskHandle) == eSuspended) {
        return true;
    }

    if (cleanupSem != nullptr) {
        if (xSemaphoreTake(cleanupSem, waitTicks) != pdTRUE &&
            eTaskGetState(taskHandle) != eSuspended) {
            return false;
        }
    } else if (waitTicks > 0) {
        return false;
    }

    const TickType_t pollStep = pdMS_TO_TICKS(10) > 0 ? pdMS_TO_TICKS(10) : 1;
    const TickType_t settleWait = pdMS_TO_TICKS(50);
    TickType_t waited = 0;

    while (eTaskGetState(taskHandle) != eSuspended && waited < settleWait) {
        vTaskDelay(pollStep);
        waited += pollStep;
    }

    return eTaskGetState(taskHandle) == eSuspended;
}

} // namespace

namespace SYSTEM {

ThermalMonitor& ThermalMonitor::instance() {
    static ThermalMonitor inst;
    return inst;
}

bool ThermalMonitor::begin() {
    if (_taskHandle) {
        if (!_running) {
            (void)reapStoppedTask(0);
        }
        if (_taskHandle) {
            LOGW("Thermal monitor task already running or still stopping");
            return _running;
        }
    }

    if (_running) {
        LOGW("Already running");
        return true;
    }

    // Allocate stack in internal DRAM (required for cache-disable safety)
    if (!_stackBuffer) {
        _stackBuffer = (StackType_t*)heap_caps_malloc(
            CONFIG::TASKS::STACK_THERMAL_MONITOR, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (!_tcb) {
        _tcb = (StaticTask_t*)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (!_cleanupSem) {
        _cleanupSem = xSemaphoreCreateBinary();
    }

    if (!_stackBuffer || !_tcb || !_cleanupSem) {
        LOGE("Failed to allocate thermal monitor task resources");
        freeTaskResources();
        return false;
    }

    (void)xSemaphoreTake(_cleanupSem, 0);
    _running = true;
    _state = ThermalState::NORMAL;
    _currentFreq = THERMAL::FREQ_NORMAL;

    _taskHandle = xTaskCreateStaticPinnedToCore(
        taskEntry,
        "thermal_mon",
        CONFIG::TASKS::STACK_THERMAL_MONITOR,
        this,
        CONFIG::TASKS::PRIO_THERMAL_MONITOR,
        _stackBuffer,
        _tcb,
        CONFIG::TASKS::CORE_THERMAL_MONITOR
    );

    if (!_taskHandle) {
        LOGE("Failed to create task");
        freeTaskResources();
        return false;
    }

    LOGD("Started (interval=%ums, thresholds=%.0f/%.0f/%.0f°C)",
         THERMAL::MONITOR_INTERVAL_MS,
         THERMAL::TEMP_SOFT_THROTTLE,
         THERMAL::TEMP_HARD_THROTTLE,
         THERMAL::TEMP_CRITICAL);

    return true;
}

void ThermalMonitor::stop() {
    if (!_taskHandle && !_running) {
        return;
    }

    if (_taskHandle && xTaskGetCurrentTaskHandle() == _taskHandle) {
        LOGW("ThermalMonitor::stop() called from worker context; requesting graceful exit only");
        _running = false;
        return;
    }

    const bool wasRunning = _running;
    _running = false;

    if (_taskHandle) {
        // Wake the task out of startup/interval delay so shutdown does not wait
        // for up to the full monitor interval.
        (void)xTaskAbortDelay(_taskHandle);
    }

    constexpr TickType_t kTimeout = pdMS_TO_TICKS(THERMAL::MONITOR_INTERVAL_MS * 2 + 500);
    const TickType_t waitTicks = wasRunning ? kTimeout : 0;
    if (!reapStoppedTask(waitTicks)) {
        LOGW("Thermal monitor did not suspend cleanly - keeping resources allocated");
        return;
    }

    restoreNormalState();

    LOGD("Stopped");
}

void ThermalMonitor::taskEntry(void* param) {
    auto* self = static_cast<ThermalMonitor*>(param);
    self->taskLoop();
}

void ThermalMonitor::taskLoop() {
    // Initial delay to let system stabilize
    vTaskDelay(pdMS_TO_TICKS(5000));

    while (_running) {
        float temp = temperatureRead();
        _lastTemp = temp;

        evaluateAndApply(temp);


        // Stack Monitoring
        LOG_STACK_PERIODIC(CONFIG::TASKS::STACK_THERMAL_MONITOR);

        vTaskDelay(pdMS_TO_TICKS(THERMAL::MONITOR_INTERVAL_MS));
    }

    if (_cleanupSem) {
        xSemaphoreGive(_cleanupSem);
    }
    vTaskSuspend(nullptr);
}

bool ThermalMonitor::reapStoppedTask(TickType_t waitTicks) {
    if (!_taskHandle) {
        freeTaskResources();
        return true;
    }

    if (_running) {
        return false;
    }

    if (!waitForTaskSuspended(_taskHandle, _cleanupSem, waitTicks)) {
        return false;
    }

    vTaskDelete(_taskHandle);
    freeTaskResources();
    return true;
}

void ThermalMonitor::freeTaskResources() {
    _taskHandle = nullptr;
    _running = false;

    if (_stackBuffer) {
        heap_caps_free(_stackBuffer);
        _stackBuffer = nullptr;
    }
    if (_tcb) {
        heap_caps_free(_tcb);
        _tcb = nullptr;
    }
    if (_cleanupSem) {
        vSemaphoreDelete(_cleanupSem);
        _cleanupSem = nullptr;
    }
}

void ThermalMonitor::restoreNormalState() {
    setCpuFrequencyMhz(THERMAL::FREQ_NORMAL);
    _state = ThermalState::NORMAL;
    _currentFreq = THERMAL::FREQ_NORMAL;
}

void ThermalMonitor::evaluateAndApply(float temp) {
    ThermalState newState = _state;

    // Evaluate based on current state (hysteresis logic)
    switch (_state) {
        case ThermalState::NORMAL:
            // Transition UP only
            if (temp >= THERMAL::TEMP_CRITICAL) {
                newState = ThermalState::CRITICAL;
            } else if (temp >= THERMAL::TEMP_HARD_THROTTLE) {
                newState = ThermalState::HARD_THROTTLE;
            } else if (temp >= THERMAL::TEMP_SOFT_THROTTLE) {
                newState = ThermalState::SOFT_THROTTLE;
            }
            break;

        case ThermalState::SOFT_THROTTLE:
            // Transition UP or DOWN with hysteresis
            if (temp >= THERMAL::TEMP_CRITICAL) {
                newState = ThermalState::CRITICAL;
            } else if (temp >= THERMAL::TEMP_HARD_THROTTLE) {
                newState = ThermalState::HARD_THROTTLE;
            } else if (temp < (THERMAL::TEMP_SOFT_THROTTLE - THERMAL::HYSTERESIS)) {
                newState = ThermalState::NORMAL;
            }
            break;

        case ThermalState::HARD_THROTTLE:
            // Transition DOWN only with hysteresis (or UP to CRITICAL)
            if (temp >= THERMAL::TEMP_CRITICAL) {
                newState = ThermalState::CRITICAL;
            } else if (temp < (THERMAL::TEMP_HARD_THROTTLE - THERMAL::HYSTERESIS)) {
                if (temp < (THERMAL::TEMP_SOFT_THROTTLE - THERMAL::HYSTERESIS)) {
                    newState = ThermalState::NORMAL;
                } else {
                    newState = ThermalState::SOFT_THROTTLE;
                }
            }
            break;

        case ThermalState::CRITICAL:
            // Device reboots in this state, no transitions out
            break;
    }

    if (newState != _state) {
        applyState(newState);
    }
}

void ThermalMonitor::applyState(ThermalState newState) {
    uint32_t newFreq = THERMAL::FREQ_NORMAL;
    wifi_power_t newWifiPower = WIFI_POWER_15dBm;
    const char* stateName = "NORMAL";

    switch (newState) {
        case ThermalState::NORMAL:
            newFreq = THERMAL::FREQ_NORMAL;
            newWifiPower = WIFI_POWER_15dBm;
            stateName = "NORMAL";
            break;
        case ThermalState::SOFT_THROTTLE:
            newFreq = THERMAL::FREQ_SOFT;
            newWifiPower = WIFI_POWER_11dBm;
            stateName = "SOFT_THROTTLE";
            break;
        case ThermalState::HARD_THROTTLE:
            newFreq = THERMAL::FREQ_HARD;
            newWifiPower = WIFI_POWER_8_5dBm;
            stateName = "HARD_THROTTLE";
            break;
        case ThermalState::CRITICAL:
            newFreq = THERMAL::FREQ_HARD; // Keep min freq before sleep
            newWifiPower = WIFI_POWER_8_5dBm; // Min power
            stateName = "CRITICAL (SHUTDOWN)";
            break;
    }

    // Apply CPU frequency change
    setCpuFrequencyMhz(newFreq);
    // Apply WiFi power change
    WiFi.setTxPower(newWifiPower);
    
    // Configure WiFi Power Save based on thermal state
    static wifi_ps_type_t lastPsType = (wifi_ps_type_t)-1;
    wifi_ps_type_t psType = WIFI_PS_NONE;
    const char* psStr = "NONE";
    
    if (newState == ThermalState::HARD_THROTTLE) {
        psType = WIFI_PS_MAX_MODEM;
        psStr = "MAX_MODEM";
    } else {
        // Keep the appliance UI/network stack fully responsive in NORMAL and
        // SOFT_THROTTLE. SOFT still reduces CPU frequency and WiFi TX power,
        // which buys thermal headroom without adding the wake-up latency of
        // modem sleep on the first inbound HTTPS request.
        psType = WIFI_PS_NONE;
        psStr = "NONE";
    }

    // Only apply if state changed and WiFi is initialized (avoids redundant esp_wifi_set_ps calls)
    if (psType != lastPsType && WiFi.getMode() != WIFI_MODE_NULL) {
        esp_wifi_set_ps(psType);
        lastPsType = psType;
    }

#ifndef NATIVE_BUILD
    // Apply BLE Thermal Throttling
    // Check if BLE is running to avoid initializing the stack unnecessarily
    if (_bleService && _bleService->isRunning()) {
        esp_power_level_t blePower = ESP_PWR_LVL_P9; // Default Max (+9dBm)
        uint32_t bleScanInterval = SENSOR::BLE::SCAN_INTERVAL_MS;
        uint32_t bleScanWindow = SENSOR::BLE::SCAN_WINDOW_MS;

        switch (newState) {
            case ThermalState::NORMAL:
                blePower = ESP_PWR_LVL_P9;
                
                // Boost for Discovery Mode (Manual Search) - 100% Duty Cycle
                if (_bleService->isDiscoveryActive()) {
                     bleScanInterval = SENSOR::BLE::DISCOVERY_SCAN_INTERVAL_MS;
                     bleScanWindow = SENSOR::BLE::DISCOVERY_SCAN_WINDOW_MS;
                     LOGI("Thermal: BOOSTING BLE for Discovery Mode (100% duty cycle)");
                } else {
                     bleScanInterval = SENSOR::BLE::SCAN_INTERVAL_MS;
                     bleScanWindow = SENSOR::BLE::SCAN_WINDOW_MS;
                }
                break;
            case ThermalState::SOFT_THROTTLE:
                blePower = ESP_PWR_LVL_P3; // Reduce to +3dBm
                bleScanInterval = 500;     // Keep 500ms interval
                bleScanWindow = 150;       // 150ms window (30% duty)
                break;
            case ThermalState::HARD_THROTTLE:
            case ThermalState::CRITICAL:
                blePower = ESP_PWR_LVL_N0; // Reduce to 0dBm (or lower)
                bleScanInterval = 1000;    // Relax scan to 1000ms
                bleScanWindow = 100;       // 100ms window (10% duty)
                break;
        }

        NimBLEDevice::setPower(blePower);
        
        // Update scan parameters for the NEXT scan cycle (BleScanner restarts every 2s)
        NimBLEScan* pScan = NimBLEDevice::getScan();
        if (pScan) {
            pScan->setInterval(bleScanInterval);
            pScan->setWindow(bleScanWindow);
        }
    }
#endif

    // Apply Matrix LED Brightness Limits
    uint8_t matrixLimit = 255;
    if (newState == ThermalState::CRITICAL) {
        matrixLimit = 0;  // Off
    } else if (newState == ThermalState::HARD_THROTTLE) {
        matrixLimit = 2;  // Extreme dimming for critical heat
    } else if (newState == ThermalState::SOFT_THROTTLE) {
        matrixLimit = 16; // User requested 16 max for soft throttle
    }
    if (_matrixService) {
        _matrixService->setThermalBrightnessLimit(matrixLimit);
    }

    _currentFreq = getCpuFrequencyMhz();  // Verify actual frequency

    // Log with clear indication of actions (before state assignment!)
    const char* wifiStr = "Max";
    if (newWifiPower == WIFI_POWER_15dBm) wifiStr = "Med (15dBm)";
    else if (newWifiPower == WIFI_POWER_8_5dBm) wifiStr = "Low (8.5dBm)";
    else if (newWifiPower == WIFI_POWER_11dBm) wifiStr = "Low (11dBm)";

    const char* oldStateName = "NORMAL";
    switch (_state) {
        case ThermalState::SOFT_THROTTLE: oldStateName = "SOFT"; break;
        case ThermalState::HARD_THROTTLE: oldStateName = "HARD"; break;
        case ThermalState::CRITICAL:      oldStateName = "CRITICAL"; break;
        default: break;
    }

    LOGI("State: %s -> %s | CPU: %uMHz | WiFi: TX=%s PS=%s | LED: %u | Temp: %.1f°C", 
         oldStateName, 
         stateName, 
         _currentFreq, 
         wifiStr, 
         psStr,
         matrixLimit,
         _lastTemp);

    _state = newState;

    // Handle CRITICAL Shutdown
    if (newState == ThermalState::CRITICAL) {
        LOGE("CRITICAL TEMP %.1f°C > %.1f°C! Initiating emergency cooling sleep (%ums)...", 
             _lastTemp, THERMAL::TEMP_CRITICAL, THERMAL::SHUTDOWN_COOLING_MS);

        // Give time for logs to flush
        vTaskDelay(pdMS_TO_TICKS(100));

        // Request sleep immediately
        if (_powerManager) {
            _powerManager->setWakeInterval(THERMAL::SHUTDOWN_COOLING_MS);
            RTC::markMaintenanceSleepPending(millis(), _lastTemp);
            _powerManager->requestSleep("thermal-critical");
        }
        
        // Pause here effectively until sleep happens
        vTaskDelay(portMAX_DELAY);
    }
}

} // namespace SYSTEM
