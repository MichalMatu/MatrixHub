#pragma once

#include <Arduino.h>
#include <functional>
#include <vector>
#include "model/MacroDefinitions.h"

#include <macros/engine/MacroEngine.h>

namespace MATRIX_MANAGER { class MatrixManagerService; }

namespace KEYBOARD { class KeyboardService; }
namespace AIRMOUSE { class AirMouseService; }

namespace MACROS {

class MacroService {
public:
    // Dependency Injection via Constructor
    MacroService(KEYBOARD::KeyboardService* ks, AIRMOUSE::AirMouseService* am, MATRIX_MANAGER::MatrixManagerService* matrixManager, SemaphoreHandle_t fsMutex);
    ~MacroService();
    
    // Lifecycle (Controls Engine)
    bool begin();
    void start();
    void stop();

    // Script Management
    bool startScript(const char* filename);
    void stopScript();
    
    // Status
    MACROS::MacroState getStatus();
    using StateCallback = std::function<void(const MACROS::MacroState&)>;
    void setUpdateCallback(StateCallback cb);

    // File Operations (Delegated to Repository)
    MACROS::PsramVector<MACROS::PsramString> listScripts();
    bool deleteScript(const char* filename);
    bool saveScript(const char* filename, const char* content);
    MACROS::PsramString getScriptContent(const char* filename);
    bool scriptExists(const char* filename);

    // Settings (Direct RTC Access)
    void applySettings();

private:
    // Helper task
    static void bootTaskFunction(void* parameter);
    
    // Internal helpers
    bool _begin();
    void onEngineStateChange(const MACROS::MacroState& state);

private:
    MacroEngine _engine;
    StateCallback _externalCallback;
    
    KEYBOARD::KeyboardService* _keyboardService;
    AIRMOUSE::AirMouseService* _airMouseService;
    MATRIX_MANAGER::MatrixManagerService* _matrixManager;
    
    SemaphoreHandle_t _fileMutex; // Protects file operations
};


} // namespace MACROS
