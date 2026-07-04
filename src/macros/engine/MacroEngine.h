#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <atomic>
#include <functional>
#include <macros/model/MacroDefinitions.h>
#include <macros/parsing/MacroParser.h> // [Restored]
namespace KEYBOARD { class KeyboardService; }
namespace AIRMOUSE { class AirMouseService; }
namespace MATRIX_MANAGER { class MatrixManagerService; }

namespace MACROS {

class MacroEngine {
public:
    MacroEngine();
    ~MacroEngine();

    // Lifecycle
    void start(); // Start the background task
    void stop();  // Stop the background task

    // Script Control
    bool startScript(const char* filename);
    void stopScript();
    void clearState(bool notify = true);

    // State Inspection
    MacroState getState();
    
    // Observer
    using StateCallback = std::function<void(const MacroState&)>;
    void setStateCallback(StateCallback cb);

    // Dependency Injection
    void setKeyboardService(KEYBOARD::KeyboardService* ks) { _keyboardService = ks; }
    void setAirMouseService(AIRMOUSE::AirMouseService* am) { _airMouseService = am; }
    void setMatrixManager(MATRIX_MANAGER::MatrixManagerService* mgr) { _matrixManager = mgr; }
    void setFsMutex(SemaphoreHandle_t mutex) { _fsMutex = mutex; }

private:
    static void taskFunction(void* parameter);
    void executeLoop();
    void performDelay(uint32_t ms);
    void processCommand(const MacroCommand& cmd, MacroCommand& lastCmd, uint32_t& defaultDelay);
    bool executionLimitExceeded(uint32_t scriptStartMs, uint32_t executedCommands, PsramString& error) const;
    void notifyState();
    bool isTaskAlive(TaskHandle_t handle) const;
    void cleanupTaskResources();

    TaskHandle_t _taskHandle = nullptr;
    SemaphoreHandle_t _mutex;
    std::atomic<bool> _stopSignal; 
    
    MacroState _state;
    StateCallback _stateCallback;
    
    KEYBOARD::KeyboardService* _keyboardService = nullptr;
    AIRMOUSE::AirMouseService* _airMouseService = nullptr;
    MATRIX_MANAGER::MatrixManagerService* _matrixManager = nullptr;
    SemaphoreHandle_t _fsMutex = nullptr;
    
    StackType_t* _taskStack = nullptr;
    StaticTask_t* _taskTcb = nullptr;
};

}
