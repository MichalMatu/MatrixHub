#include "MacroService.h"
#include "persistence/MacroRepository.h"

#include "../system/logging/Logging.h"
#include "../system/rtc/RtcConfig.h"
#include "../config/System.h" // For CONFIG::TASKS
#include "../system/utils/ScopeLock.h"

#undef LOG_TAG
#define LOG_TAG "MacroService"

namespace MACROS {

namespace {


RTC::MacroData readMacroData() {
    RTC::MacroData macros{};
    bool got = false;
    RTC::withConfig([&](const RTC::ConfigStore& c) {
        macros = c.macros;
        got = true;
    });
    if (!got) {
        macros = RTC::getConfig().macros;
    }
    return macros;
}
} // namespace

MacroService::MacroService(KEYBOARD::KeyboardService* ks, AIRMOUSE::AirMouseService* am, MATRIX_MANAGER::MatrixManagerService* matrixManager, SemaphoreHandle_t fsMutex) 
    : _keyboardService(ks), _airMouseService(am), _matrixManager(matrixManager) {
    
    _engine.setKeyboardService(ks);
    _engine.setAirMouseService(am);
    _engine.setMatrixManager(matrixManager);
    
    _engine.setStateCallback([this](const MacroState& s){
        this->onEngineStateChange(s);
    });

    _fileMutex = xSemaphoreCreateMutex();
    
    MacroRepository::setFsMutex(fsMutex);
    _engine.setFsMutex(fsMutex);
}

MacroService::~MacroService() {
    setUpdateCallback(nullptr);
    _engine.setStateCallback(nullptr);
    stop();
    if (_fileMutex) {
        vSemaphoreDelete(_fileMutex);
        _fileMutex = nullptr;
    }
}

bool MacroService::begin() { 
    return _begin(); 
}

bool MacroService::_begin() {
    // LittleFS mounted in Phase 1
    if (!LittleFS.exists("/scripts")) {
        LittleFS.mkdir("/scripts");
    }

    const auto macros = readMacroData();
    if (macros.enabled) {
        _engine.start(); // Start engine thread
        
        // Boot script logic
        if (macros.bootScript[0] != '\0') {
            // Dynamic one-shot task: FreeRTOS releases resources after vTaskDelete(NULL).
            BaseType_t ok = xTaskCreatePinnedToCore(
                bootTaskFunction,
                "MacroBoot",
                CONFIG::TASKS::STACK_MACRO_BOOT,
                this, // Pass 'this' as parameter
                CONFIG::TASKS::PRIO_MACRO,
                nullptr,
                CONFIG::TASKS::CORE_MACRO
            );
            if (ok != pdPASS) {
                LOGE("Failed to create MacroBoot task");
            }
        }
        
        LOGI("MacroService initialized (Paused)");
    }
    return true;
}

void MacroService::start() { 
    _engine.start(); 
    onEngineStateChange(_engine.getState()); // Notify
}

void MacroService::stop() { 
    _engine.stop();
    onEngineStateChange(_engine.getState());
}

bool MacroService::startScript(const char* filename) {
    const auto cfg = readMacroData();
    if (!cfg.enabled) {
        LOGW("MacroService: startScript rejected (macros disabled)");
        return false;
    }
    return _engine.startScript(filename);
}

void MacroService::stopScript() {
    _engine.stopScript();
}

MacroState MacroService::getStatus() {
    return _engine.getState();
}

void MacroService::setUpdateCallback(StateCallback cb) {
    _externalCallback = cb;
     // Optional: Notify current state immediately
     // if (cb) cb(_engine.getState());
}

void MacroService::onEngineStateChange(const MacroState& state) {
    if (_externalCallback) {
        _externalCallback(state);
    }
}

// Delegations
PsramVector<PsramString> MacroService::listScripts() { 
    SYSTEM::ScopeLock lock(_fileMutex);
    if (!lock.isLocked()) return {};
    return MacroRepository::listScripts(); 
}

bool MacroService::saveScript(const char* f, const char* c) { 
    SYSTEM::ScopeLock lock(_fileMutex);
    if (!lock.isLocked()) return false;
    
    // Safety: Stop engine if overwriting the currently running script
    bool wasRunning = false;
    MacroState s = _engine.getState();
    if (s.status == MacroStatus::RUNNING && s.currentScript == f) {
         LOGW("MacroService: Stopping engine before saving active script: %s", f);
         _engine.stop();
         wasRunning = true;
         vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    bool result = MacroRepository::saveScript(f, c);

    // Resume script after saving if it was running before overwrite
    if (result && wasRunning) {
        LOGI("MacroService: Restarting script after overwrite: %s", f);
        _engine.start();
        _engine.startScript(f);
    }

    return result; 
}

PsramString MacroService::getScriptContent(const char* f) { 
    SYSTEM::ScopeLock lock(_fileMutex);
    if (!lock.isLocked()) return "";
    return MacroRepository::getScriptContent(f); 
}

bool MacroService::scriptExists(const char* filename) {
    SYSTEM::ScopeLock lock(_fileMutex);
    if (!lock.isLocked()) return false;
    return MacroRepository::scriptExists(filename);
}

bool MacroService::deleteScript(const char* filename) {
    SYSTEM::ScopeLock lock(_fileMutex);
    if (!lock.isLocked()) return false;
    
    MacroState s = _engine.getState();
    if (s.status == MacroStatus::RUNNING && s.currentScript == filename) {
         LOGW("MacroService: Stopping engine before deleting active script: %s", filename);
         _engine.stop();
         vTaskDelay(pdMS_TO_TICKS(50));
    }
    return MacroRepository::deleteScript(filename);
}

void MacroService::applySettings() { 
    const auto cfg = readMacroData();
    
    if (cfg.enabled) {
        start();
    } else {
        stop();
        _engine.clearState();
    }
}

void MacroService::bootTaskFunction(void* parameter) {
    MacroService* service = static_cast<MacroService*>(parameter);
    const auto cfgBefore = readMacroData();
    const uint32_t delay = cfgBefore.bootDelay;
    vTaskDelay(pdMS_TO_TICKS(delay));
    
    // Re-read config after delay
    const auto cfg = readMacroData();
    if (cfg.enabled && cfg.bootScript[0] != '\0') {
        if (service) {
            service->startScript(cfg.bootScript);
        }
    }
    vTaskDelete(NULL);
}

} // namespace MACROS
