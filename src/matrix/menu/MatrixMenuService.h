/**
 * @file MatrixMenuService.h
 * @brief Button-controlled menu for Matrix LED display
 * 
 * Golden Standard Implementation:
 * - Single button: navigate screens (TIME → SENSORS → IP → TIME)
 * - Menu enter/select is handled immediately when the medium hold threshold is crossed
 * - Lock-free isActive() for button responsiveness
 * - Thread-safe state management with minimal mutex hold time
 */

#pragma once

#include <Arduino.h>
#include <atomic>
#include <functional>
#include "../../system/rtc/types/RtcMatrixTypes.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class MatrixService;
namespace MATRIX_MANAGER { class MatrixManagerService; }

namespace MATRIX {

enum class WifiMenuAction : uint8_t {
    Station = 0,
    AccessPoint,
    Off
};

class MatrixMenuService {
public:
    explicit MatrixMenuService(MatrixService* matrixService, MATRIX_MANAGER::MatrixManagerService* matrixManager = nullptr);

    /// Lock-free check for button handler (always fast)
    bool isActive() const;
    
    /// Check if menu feature is enabled in settings
    bool isEnabled() const;
    
    /// Open menu on the first screen (TIME) if it is currently closed
    void enterMenu();

    /// Navigate to next screen (cycles TIME -> SENSORS -> IP -> TIME)
    void nextScreen();

    /// Select current menu item. WiFi actions require follow-up short clicks.
    void selectCurrent();
    
    /// Force close menu immediately
    void exitMenu();
    
    /// Periodic update (call from task loop)
    void update();
    
    /// Force re-render on next update() (e.g. after rotation change)
    void invalidateCache();
    
    /// Get current screen (lock-free read)
    RTC::MatrixMenuScreen current() const;

    void setWifiModeActions(std::function<WifiMenuAction()> getMode,
                            std::function<bool(WifiMenuAction)> setModeAndRestart);

private:
    // Thread synchronization
    SemaphoreHandle_t _mutex = nullptr;
    StaticSemaphore_t _mutexBuffer;
    
    // Dependencies (injected, not owned)
    MatrixService* _matrixService;
    MATRIX_MANAGER::MatrixManagerService* _matrixManager;
    
    // State
    std::atomic<bool> _isActive{false};
    std::atomic<RTC::MatrixMenuScreen> _screen{RTC::MatrixMenuScreen::NONE};
    RTC::MatrixMenuScreen _lastRenderedScreen = RTC::MatrixMenuScreen::NONE;
    uint32_t _lastUpdateMs = 0;
    std::function<WifiMenuAction()> _getWifiMode;
    std::function<bool(WifiMenuAction)> _setWifiModeAndRestart;
    bool _wifiConfirmActive = false;
    WifiMenuAction _pendingWifiAction = WifiMenuAction::Station;
    uint8_t _wifiConfirmClicks = 0;
    uint32_t _wifiConfirmDeadlineMs = 0;
    
    // Text dedup cache — prevents scroll reset when content unchanged
    char _lastRenderedText[64] = {0};
    uint32_t _scrollHoldUntilMs = 0;

    // Rendering (called OUTSIDE mutex)
    bool renderScreen(RTC::MatrixMenuScreen screen, bool forceRender);
    void showText(const char* text, uint32_t color, uint32_t holdMs);
    void clearWifiConfirmation();
    bool handleWifiConfirmationClick();
    WifiMenuAction actionForScreen(RTC::MatrixMenuScreen screen) const;
    bool isWifiActionScreen(RTC::MatrixMenuScreen screen) const;
    uint32_t estimateScrollDurationMs(const char* text, uint16_t scrollSpeedMs) const;
    
    // Formatters (use fixed buffers, no heap)
    void formatTime(char* buf, size_t bufSize);
    void formatSensors(char* buf, size_t bufSize);
    void formatIP(char* buf, size_t bufSize);
    void formatWifiAction(RTC::MatrixMenuScreen screen, char* buf, size_t bufSize);
};

}  // namespace MATRIX
