#include "BootTracker.h"
#include <esp_attr.h>
#include "../logging/Logging.h"
#include "../health/heap/HeapMonitor.h"
#include <esp_system.h>

#undef LOG_TAG
#define LOG_TAG "BootTrack"

namespace SYSTEM {

// RTC memory storage (survives deep sleep and many soft resets, but not power loss).
// NOINIT prevents the bootloader from zeroing it on some reset types.
RTC_NOINIT_ATTR BootState rtcBootState;

// Static member initialization
BootState BootTracker::_state = {};
BootState BootTracker::_lastSessionState = {};
bool BootTracker::_initialized = false;
bool BootTracker::_lastBootUnexpected = false;

namespace {

bool shouldCountUnexpectedRestart(esp_reset_reason_t resetReason) {
    switch (resetReason) {
        case ESP_RST_SW:
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
        case ESP_RST_BROWNOUT:
        case ESP_RST_PWR_GLITCH:
        case ESP_RST_CPU_LOCKUP:
            return true;
        default:
            return false;
    }
}

const char* resetReasonToString(esp_reset_reason_t resetReason) {
    switch (resetReason) {
        case ESP_RST_POWERON: return "power_on";
        case ESP_RST_EXT: return "ext";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "int_wdt";
        case ESP_RST_TASK_WDT: return "task_wdt";
        case ESP_RST_WDT: return "wdt";
        case ESP_RST_DEEPSLEEP: return "deep_sleep";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_SDIO: return "sdio";
        case ESP_RST_USB: return "usb";
        case ESP_RST_JTAG: return "jtag";
        case ESP_RST_EFUSE: return "efuse";
        case ESP_RST_PWR_GLITCH: return "power_glitch";
        case ESP_RST_CPU_LOCKUP: return "cpu_lockup";
        default: return "unknown";
    }
}

}  // namespace

void BootTracker::begin() {
    if (_initialized) {
        return;
    }
    
    loadFromRtc();
    esp_reset_reason_t resetReason = esp_reset_reason();
    
    // Check if RTC-backed state is valid for this boot epoch.
    if (_state.magic != kMagic) {
        // RTC memory was wiped by a cold/power boot or other low-level reset behavior.
        // BootTracker is intentionally RTC-only, so there is no NVS recovery path here;
        // we just start a new local boot-counting window from a clean state.
        LOGW("RTC state invalid (magic: 0x%08X), starting fresh RTC-only boot state", _state.magic);

        resetState();

        // Lost RTC context - do not report a synthetic crash on a brand-new epoch.
        _lastBootUnexpected = false;
        _lastSessionState = {};
    } else {
        _lastSessionState = _state;
        const bool hasShutdownMarker = (_state.lastShutdownReason != 0);
        _lastBootUnexpected = !hasShutdownMarker && shouldCountUnexpectedRestart(resetReason);

        if (_lastBootUnexpected) {
            if (_state.unexpectedRestarts < UINT16_MAX) {
                _state.unexpectedRestarts++;
            }
            LOGW("Unexpected restart detected! (count: %u)", _state.unexpectedRestarts);
        }
    }
    
    // Increment boot count
    _state.bootCount++;
    
    // Record reset reason for this boot
    _state.lastResetReason = static_cast<uint8_t>(esp_reset_reason());
    
    // Clear shutdown info for this session (will be set on clean shutdown)
    _state.lastShutdownReason = 0;
    _state.lastShutdownMs = 0;
    _state.lastUptimeMs = 0;
    _state.freeHeapAtShutdown = 0;
    
    // Save updated state
    saveToRtc();
    
    _initialized = true;
    
    resetReason = esp_reset_reason();

    // Log boot info
    const char* resetReasonStr = resetReasonToString(resetReason);
    
    LOGI("Boot #%u, reset: %s (%d), unexpected restarts: %u", 
         _state.bootCount, resetReasonStr, static_cast<int>(resetReason), _state.unexpectedRestarts);
}

void BootTracker::recordShutdown(ShutdownReason reason) {
    if (!_initialized) {
        begin();
    }
    
    _state.lastShutdownReason = static_cast<uint8_t>(reason);
    _state.lastShutdownMs = millis();
    _state.lastUptimeMs = millis();  // Current uptime
    _state.freeHeapAtShutdown = HeapMonitor::instance().getFreeHeap();
    _lastSessionState = _state;
    
    saveToRtc();
    
    const char* reasonStr = "unknown";
    switch (reason) {
        case ShutdownReason::CLEAN_SLEEP: reasonStr = "clean_sleep"; break;
        case ShutdownReason::RESTART_COMMAND: reasonStr = "restart_cmd"; break;
        case ShutdownReason::OTA_UPDATE: reasonStr = "ota"; break;
        case ShutdownReason::FACTORY_RESET: reasonStr = "factory_reset"; break;
        case ShutdownReason::WATCHDOG_RESTART: reasonStr = "watchdog"; break;
        case ShutdownReason::LOW_MEMORY: reasonStr = "low_memory"; break;
        case ShutdownReason::HYGIENE_SLEEP: reasonStr = "hygiene_sleep"; break;
        default: break;
    }
    
    LOGI("Shutdown recorded: %s, uptime: %ums, heap: %u", 
         reasonStr, _state.lastUptimeMs, _state.freeHeapAtShutdown);
}

uint32_t BootTracker::getBootCount() {
    return _state.bootCount;
}

uint16_t BootTracker::getUnexpectedRestarts() {
    return _state.unexpectedRestarts;
}

bool BootTracker::wasLastBootUnexpected() {
    return _lastBootUnexpected;
}

uint32_t BootTracker::getLastSessionUptimeMs() {
    return _lastSessionState.lastUptimeMs;
}

uint8_t BootTracker::getLastShutdownReason() {
    return _lastSessionState.lastShutdownReason;
}

uint8_t BootTracker::getLastResetReason() {
    return _lastSessionState.lastResetReason;
}

uint32_t BootTracker::getFreeHeapAtShutdown() {
    return _lastSessionState.freeHeapAtShutdown;
}

void BootTracker::logState() {
    LOGI("=== Boot State ===");
    LOGI("  Boot count:          %u", _state.bootCount);
    LOGI("  Unexpected restarts: %u", _state.unexpectedRestarts);
    LOGI("  Last session uptime: %ums", _state.lastUptimeMs);
    LOGI("  Last heap at shutdown: %u bytes", _state.freeHeapAtShutdown);
    LOGI("  Last shutdown reason: %u", _state.lastShutdownReason);
    LOGI("  ESP reset reason:    %u", _state.lastResetReason);
}

void BootTracker::loadFromRtc() {
    _state = rtcBootState;
}

void BootTracker::saveToRtc() {
    _state.magic = kMagic;
    rtcBootState = _state;
}

void BootTracker::resetState() {
    memset(&_state, 0, sizeof(_state));
    _state.magic = kMagic;
    _state.bootCount = 0;
    saveToRtc();
}

}  // namespace SYSTEM
