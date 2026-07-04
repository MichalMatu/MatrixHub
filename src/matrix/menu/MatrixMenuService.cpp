/**
 * @file MatrixMenuService.cpp
 * @brief Button-controlled menu for Matrix LED display
 * 
 * Golden Standard Implementation:
 * - Lock-free isActive() for button responsiveness
 * - Minimal mutex hold time (state only, no rendering under lock)
 * - Stack-allocated fixed buffers (no heap/DRAM allocation)
 * - Configurable refresh intervals via System.h
 */

#include "MatrixMenuService.h"
#include <MatrixService.h>
#include "../../system/matrix_manager/MatrixManagerService.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <time.h>
#include <utility>
#include "../../config/App.h"
#include "../../sensors/runtime/SensorState.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../system/logging/Logging.h"
#include "../../config/System.h"
#include "../../system/utils/ScopeLock.h"

#undef LOG_TAG
#define LOG_TAG "MatrixMenu"

namespace MATRIX {

// Static buffer for text rendering (avoids heap allocation)
// Max: "long-hostname.local 192.168.100.100" = ~40 chars
static constexpr size_t kTextBufferSize = kMatrixTextCapacity;
static constexpr uint32_t kWifiActionTextColor = 0x00C8FF;
static constexpr uint32_t kWifiConfirmTextColor = 0xFFA000;
static constexpr uint32_t kWifiErrorTextColor = 0xFF0000;
static constexpr uint32_t kWifiOkTextColor = 0x00C800;

const char* wifiActionLabel(WifiMenuAction action) {
    switch (action) {
        case WifiMenuAction::Station:
            return "WIFI STA";
        case WifiMenuAction::AccessPoint:
            return "WIFI AP";
        case WifiMenuAction::Off:
            return "WIFI OFF";
        default:
            return "WIFI";
    }
}

const char* wifiActionActiveLabel(WifiMenuAction action) {
    switch (action) {
        case WifiMenuAction::Station:
            return "STA ACTIVE";
        case WifiMenuAction::AccessPoint:
            return "AP ACTIVE";
        case WifiMenuAction::Off:
            return "WIFI OFF";
        default:
            return "ACTIVE";
    }
}

MatrixMenuService::MatrixMenuService(MatrixService* matrixService, MATRIX_MANAGER::MatrixManagerService* matrixManager)
    : _matrixService(matrixService), _matrixManager(matrixManager) {
    configASSERT(matrixService != nullptr);
    _mutex = xSemaphoreCreateMutexStatic(&_mutexBuffer);
    // Sync atomic flag with actual state (safety for warm restarts)
    _isActive.store(_screen.load(std::memory_order_relaxed) != RTC::MatrixMenuScreen::NONE, std::memory_order_relaxed);
}

bool MatrixMenuService::isEnabled() const {
    return RTC::getConfig().matrix.menu.enabled;
}

bool MatrixMenuService::isActive() const {
    // Lock-free check for high-performance loops (ButtonHandler)
    return _isActive.load(std::memory_order_relaxed);
}

RTC::MatrixMenuScreen MatrixMenuService::current() const {
    return _screen.load(std::memory_order_relaxed);
}

void MatrixMenuService::setWifiModeActions(std::function<WifiMenuAction()> getMode,
                                           std::function<bool(WifiMenuAction)> setModeAndRestart) {
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
    if (!lock.isLocked()) {
        return;
    }
    _getWifiMode = std::move(getMode);
    _setWifiModeAndRestart = std::move(setModeAndRestart);
}

void MatrixMenuService::enterMenu() {
    if (isActive()) {
        return;
    }
    nextScreen();
}

void MatrixMenuService::nextScreen() {
    if (!_mutex) return;
    if (!isEnabled()) {
        exitMenu();
        return;
    }

    if (handleWifiConfirmationClick()) {
        return;
    }
    
    RTC::MatrixMenuScreen screenToRender = RTC::MatrixMenuScreen::NONE;
    
    {
        SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
        if (!lock.isLocked()) {
            LOGE("Failed to take mutex for nextScreen");
            return;
        }
        
        const auto cur = _screen.load(std::memory_order_relaxed);
        RTC::MatrixMenuScreen next = RTC::MatrixMenuScreen::TIME;
        switch (cur) {
            case RTC::MatrixMenuScreen::NONE:     next = RTC::MatrixMenuScreen::TIME;     break;
            case RTC::MatrixMenuScreen::TIME:     next = RTC::MatrixMenuScreen::SENSORS;  break;
            case RTC::MatrixMenuScreen::SENSORS:  next = RTC::MatrixMenuScreen::IP;       break;
            case RTC::MatrixMenuScreen::IP:                next = RTC::MatrixMenuScreen::WIFI_STATION;      break;
            case RTC::MatrixMenuScreen::WIFI_STATION:      next = RTC::MatrixMenuScreen::WIFI_ACCESS_POINT; break;
            case RTC::MatrixMenuScreen::WIFI_ACCESS_POINT: next = RTC::MatrixMenuScreen::WIFI_DISABLED;     break;
            case RTC::MatrixMenuScreen::WIFI_DISABLED:     next = RTC::MatrixMenuScreen::EXIT;              break;
            case RTC::MatrixMenuScreen::EXIT:
            default:                              next = RTC::MatrixMenuScreen::TIME;     break;
        }
        
        _screen.store(next, std::memory_order_relaxed);
        _isActive.store(true, std::memory_order_relaxed);
        clearWifiConfirmation();
        screenToRender = next;
        _lastRenderedScreen = RTC::MatrixMenuScreen::NONE;
        _lastRenderedText[0] = '\0'; // Force re-render on screen change
        LOGI("Menu screen: %u", static_cast<unsigned>(next));
    } // mutex released here
    
    (void)renderScreen(screenToRender, true);
}

void MatrixMenuService::selectCurrent() {
    if (!_mutex) return;
    if (!isEnabled()) {
        exitMenu();
        return;
    }

    RTC::MatrixMenuScreen selected = RTC::MatrixMenuScreen::NONE;
    WifiMenuAction action = WifiMenuAction::Station;
    bool isWifiAction = false;
    bool isCurrentMode = false;
    bool canChangeWifi = false;

    {
        SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
        if (!lock.isLocked()) {
            LOGE("Failed to take mutex for selectCurrent");
            return;
        }

        selected = _screen.load(std::memory_order_relaxed);
        if (selected == RTC::MatrixMenuScreen::NONE) {
            return;
        }

        if (selected == RTC::MatrixMenuScreen::EXIT) {
            clearWifiConfirmation();
        } else if (isWifiActionScreen(selected)) {
            action = actionForScreen(selected);
            isWifiAction = true;
            canChangeWifi = static_cast<bool>(_setWifiModeAndRestart);
            if (_getWifiMode) {
                isCurrentMode = _getWifiMode() == action;
            }
            if (canChangeWifi && !isCurrentMode) {
                _pendingWifiAction = action;
                _wifiConfirmActive = true;
                _wifiConfirmClicks = 0;
                _wifiConfirmDeadlineMs = millis() + FACTORY::RESET_CONFIRM_WINDOW_MS;
                _lastRenderedScreen = RTC::MatrixMenuScreen::NONE;
                _lastRenderedText[0] = '\0';
            }
        }
    }

    if (selected == RTC::MatrixMenuScreen::EXIT) {
        exitMenu();
        return;
    }

    if (!isWifiAction) {
        return;
    }

    if (!canChangeWifi) {
        showText("NO WIFI API", kWifiErrorTextColor, 2000);
        return;
    }

    if (isCurrentMode) {
        showText(wifiActionActiveLabel(action), kWifiOkTextColor, 2000);
        return;
    }

    showText("2x OK", kWifiConfirmTextColor, FACTORY::RESET_CONFIRM_WINDOW_MS);
}

void MatrixMenuService::exitMenu() {
    if (!_mutex) return;
    
    bool wasActive = false;
    {
        SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
        if (!lock.isLocked()) {
            LOGE("Failed to take mutex for exitMenu");
            return;
        }
        if (_screen.load(std::memory_order_relaxed) != RTC::MatrixMenuScreen::NONE) {
            _screen.store(RTC::MatrixMenuScreen::NONE, std::memory_order_relaxed);
            _lastRenderedScreen = RTC::MatrixMenuScreen::NONE;
            clearWifiConfirmation();
            _isActive.store(false, std::memory_order_relaxed);
            wasActive = true;
            LOGI("Menu force closed");
        }
    } // mutex released here
    
    if (wasActive) {
        // Clear MENU layer so Manager knows menu is no longer active
        // Manager will automatically re-render the next highest layer (like ALARM)
        if (_matrixManager) _matrixManager->clearLayer(MATRIX_MANAGER::Layer::MENU);
        else _matrixService->clear(false); // Fallback for testing/native
    }
}

void MatrixMenuService::invalidateCache() {
    if (!_isActive.load(std::memory_order_relaxed)) return;
    if (!_mutex) return;
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(10));
    if (!lock.isLocked()) return;
    _lastUpdateMs = 0;           // Force timer expiry on next update()
    _lastRenderedScreen = RTC::MatrixMenuScreen::NONE; // Force immediate re-render
    _lastRenderedText[0] = '\0'; // Clear dedup so text is re-sent
}

void MatrixMenuService::update() {
    if (!_isActive.load(std::memory_order_relaxed)) return;
    if (!_mutex) return;
    if (!isEnabled()) {
        exitMenu();
        return;
    }
    
    RTC::MatrixMenuScreen screenToRender = RTC::MatrixMenuScreen::NONE;
    bool forceRender = false;
    bool wifiConfirmTimedOut = false;
    {
        SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(10));
        if (!lock.isLocked()) return; // Skip this frame if busy
        
        const auto cur = _screen.load(std::memory_order_relaxed);
        if (cur == RTC::MatrixMenuScreen::NONE) return;
        
        const uint32_t now = millis();
        if (_wifiConfirmActive && static_cast<int32_t>(now - _wifiConfirmDeadlineMs) >= 0) {
            clearWifiConfirmation();
            wifiConfirmTimedOut = true;
        }

        if (cur != _lastRenderedScreen || (now - _lastUpdateMs) >= UI::MATRIX::MENU_REFRESH_MS) {
            screenToRender = cur;
            forceRender = (cur != _lastRenderedScreen);
        }
    } // mutex released here

    if (wifiConfirmTimedOut) {
        showText("CANCEL", kWifiErrorTextColor, 1500);
        return;
    }
    
    if (screenToRender != RTC::MatrixMenuScreen::NONE) {
        (void)renderScreen(screenToRender, forceRender);
    }
}

void MatrixMenuService::showText(const char* text, uint32_t color, uint32_t holdMs) {
    if (!text || text[0] == '\0') {
        return;
    }

    if (_matrixManager) {
        MATRIX_MANAGER::LayerContent layerContent;
        layerContent.active = true;
        layerContent.type = CommandType::SHOW_TEXT;
        strlcpy(layerContent.text, text, sizeof(layerContent.text));
        layerContent.color = color;
        _matrixManager->setLayer(MATRIX_MANAGER::Layer::MENU, layerContent);
    } else {
        _matrixService->showText(text, color, holdMs);
    }

    if (_mutex) {
        SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(10));
        if (lock.isLocked()) {
            strlcpy(_lastRenderedText, text, sizeof(_lastRenderedText));
            _scrollHoldUntilMs = millis() + holdMs;
            _lastRenderedScreen = _screen.load(std::memory_order_relaxed);
            _lastUpdateMs = millis();
        }
    }
}

void MatrixMenuService::clearWifiConfirmation() {
    _wifiConfirmActive = false;
    _wifiConfirmClicks = 0;
    _wifiConfirmDeadlineMs = 0;
}

bool MatrixMenuService::handleWifiConfirmationClick() {
    WifiMenuAction action = WifiMenuAction::Station;
    uint8_t clickCount = 0;
    bool shouldApply = false;
    bool expired = false;

    {
        SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
        if (!lock.isLocked()) {
            LOGE("Failed to take mutex for WiFi confirmation");
            return true;
        }

        if (!_wifiConfirmActive) {
            return false;
        }

        const uint32_t now = millis();
        if (static_cast<int32_t>(now - _wifiConfirmDeadlineMs) >= 0) {
            clearWifiConfirmation();
            expired = true;
        } else {
            _wifiConfirmClicks++;
            clickCount = _wifiConfirmClicks;
            action = _pendingWifiAction;
            _wifiConfirmDeadlineMs = now + FACTORY::RESET_CONFIRM_WINDOW_MS;
            if (_wifiConfirmClicks >= 2) {
                clearWifiConfirmation();
                shouldApply = true;
            }
        }
    }

    if (expired) {
        showText("CANCEL", kWifiErrorTextColor, 1500);
        return true;
    }

    if (!shouldApply) {
        showText(clickCount == 1 ? "1/2" : "2x OK", kWifiConfirmTextColor, FACTORY::RESET_CONFIRM_WINDOW_MS);
        return true;
    }

    const bool ok = _setWifiModeAndRestart ? _setWifiModeAndRestart(action) : false;
    showText(ok ? "RESTART" : "SAVE FAIL", ok ? kWifiOkTextColor : kWifiErrorTextColor, 2500);
    return true;
}

WifiMenuAction MatrixMenuService::actionForScreen(RTC::MatrixMenuScreen screen) const {
    switch (screen) {
        case RTC::MatrixMenuScreen::WIFI_STATION:
            return WifiMenuAction::Station;
        case RTC::MatrixMenuScreen::WIFI_ACCESS_POINT:
            return WifiMenuAction::AccessPoint;
        case RTC::MatrixMenuScreen::WIFI_DISABLED:
            return WifiMenuAction::Off;
        default:
            return WifiMenuAction::Station;
    }
}

bool MatrixMenuService::isWifiActionScreen(RTC::MatrixMenuScreen screen) const {
    return screen == RTC::MatrixMenuScreen::WIFI_STATION ||
           screen == RTC::MatrixMenuScreen::WIFI_ACCESS_POINT ||
           screen == RTC::MatrixMenuScreen::WIFI_DISABLED;
}

uint32_t MatrixMenuService::estimateScrollDurationMs(const char* text, uint16_t scrollSpeedMs) const {
    if (!text || scrollSpeedMs == 0) return 0;
    const size_t len = strnlen(text, kTextBufferSize - 1);
    constexpr uint32_t kMatrixWidthPx = 8; // 8x8 matrix
    const uint32_t perCharMs = UI::MATRIX::FONT_WIDTH_PX * scrollSpeedMs;
    const uint32_t paddedLen = static_cast<uint32_t>(len) + 4; // MatrixRenderer adds 2 spaces front/back
    return (kMatrixWidthPx * scrollSpeedMs) + (paddedLen * perCharMs);
}

bool MatrixMenuService::renderScreen(RTC::MatrixMenuScreen screen, bool forceRender) {
    const auto& settings = RTC::getConfig().matrix.menu;
    if (!settings.enabled) {
        return false;
    }
    
    // Fixed-size stack buffer (no heap allocation)
    char textBuf[kTextBufferSize];
    
    switch (screen) {
        case RTC::MatrixMenuScreen::TIME:
            formatTime(textBuf, sizeof(textBuf));
            break;
        case RTC::MatrixMenuScreen::SENSORS:
            formatSensors(textBuf, sizeof(textBuf));
            break;
        case RTC::MatrixMenuScreen::IP:
            formatIP(textBuf, sizeof(textBuf));
            break;
        case RTC::MatrixMenuScreen::WIFI_STATION:
        case RTC::MatrixMenuScreen::WIFI_ACCESS_POINT:
        case RTC::MatrixMenuScreen::WIFI_DISABLED:
        case RTC::MatrixMenuScreen::EXIT:
            formatWifiAction(screen, textBuf, sizeof(textBuf));
            break;
        default:
            return false;
    }

    uint16_t scrollSpeedMs = settings.scrollSpeed;
    if (scrollSpeedMs < 20 || scrollSpeedMs > 120) {
        scrollSpeedMs = UI::MATRIX::SCROLL_INTERVAL_MS;
    }

    // Validate against the last committed render state first, but do not update
    // the dedup cache yet. If the actual render path aborts, we want the next
    // update() tick to retry instead of believing this frame was already shown.
    const uint32_t now = millis();
    if (_mutex) {
        SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(10));
        if (!lock.isLocked()) return false; // Skip this frame if cache is busy

        if (!forceRender && _scrollHoldUntilMs != 0 && (int32_t)(now - _scrollHoldUntilMs) < 0) {
            return false; // Keep current scroll running to completion
        }

        if (!forceRender && strcmp(textBuf, _lastRenderedText) == 0) {
            LOGD("Text unchanged, skipping render");
            return false;
        }
    } else {
        if (!forceRender && _scrollHoldUntilMs != 0 && (int32_t)(now - _scrollHoldUntilMs) < 0) {
            return false;
        }
        if (!forceRender && strcmp(textBuf, _lastRenderedText) == 0) {
            LOGD("Text unchanged, skipping render");
            return false;
        }
    }
    
    if (!_isActive.load(std::memory_order_relaxed) ||
        _screen.load(std::memory_order_relaxed) != screen) {
        return false;
    }
    if (_matrixManager) {
        // Render via manager to respect layer priority (no direct renderer writes)
        MATRIX_MANAGER::LayerContent layerContent;
        layerContent.active = true;
        layerContent.type = CommandType::SHOW_TEXT;
        strlcpy(layerContent.text, textBuf, sizeof(layerContent.text));
        layerContent.color = settings.textColor;
        _matrixManager->setLayer(MATRIX_MANAGER::Layer::MENU, layerContent);
    } else {
        // Fallback for tests/native or when manager is not present
        _matrixService->showText(textBuf, settings.textColor, 0);
    }

    const uint32_t scrollHoldUntilMs = now + estimateScrollDurationMs(textBuf, scrollSpeedMs);
    if (_mutex) {
        SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(10));
        if (lock.isLocked()) {
            strlcpy(_lastRenderedText, textBuf, sizeof(_lastRenderedText));
            _scrollHoldUntilMs = scrollHoldUntilMs;
            _lastRenderedScreen = screen;
            _lastUpdateMs = now;
        }
    } else {
        strlcpy(_lastRenderedText, textBuf, sizeof(_lastRenderedText));
        _scrollHoldUntilMs = scrollHoldUntilMs;
        _lastRenderedScreen = screen;
        _lastUpdateMs = now;
    }
    
    LOGD("Rendered: %s", textBuf);
    return true;
}

void MatrixMenuService::formatTime(char* buf, size_t bufSize) {
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    snprintf(buf, bufSize, "%02d.%02d %02d:%02d",
             timeinfo.tm_mday,
             timeinfo.tm_mon + 1,
             timeinfo.tm_hour,
             timeinfo.tm_min);
}

void MatrixMenuService::formatSensors(char* buf, size_t bufSize) {
    auto snapshot = SENSORS::SensorState::getSnapshot();
    
    if (snapshot.timestamp_ms > 0) {
        // Optimization: cast floats to int to avoid expensive float string formatting (%.0f)
        snprintf(buf, bufSize, "T:%d H:%d C:%u",
                 (int)(snapshot.temp + 0.5f),
                 (int)(snapshot.humid + 0.5f),
                 snapshot.co2);
    } else {
        snprintf(buf, bufSize, "T:-- H:-- C:--");
    }
}

void MatrixMenuService::formatIP(char* buf, size_t bufSize) {
    wifi_mode_t mode = WiFi.getMode();
    
    // Zero-alloc IP formatting (avoid Arduino String heap allocation)
    char ipBuf[16] = {0};
    
    if (mode == WIFI_STA || mode == WIFI_AP_STA) {
        if (WiFi.status() == WL_CONNECTED) {
            IPAddress ip = WiFi.localIP();
            snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", 
                     ip[0], ip[1], ip[2], ip[3]);
        }
    }
    if (ipBuf[0] == '\0' && (mode == WIFI_AP || mode == WIFI_AP_STA)) {
        IPAddress ip = WiFi.softAPIP();
        snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", 
                 ip[0], ip[1], ip[2], ip[3]);
    }
    
    if (ipBuf[0] == '\0' || strcmp(ipBuf, "0.0.0.0") == 0) {
        strncpy(buf, "No WiFi", bufSize - 1);
        buf[bufSize - 1] = '\0';
        return;
    }
    
    const char* hostname = WiFi.getHostname();
    if (hostname && hostname[0] != '\0') {
        snprintf(buf, bufSize, "%s.local %s", hostname, ipBuf);
    } else {
        snprintf(buf, bufSize, "%s", ipBuf);
    }
}

void MatrixMenuService::formatWifiAction(RTC::MatrixMenuScreen screen, char* buf, size_t bufSize) {
    switch (screen) {
        case RTC::MatrixMenuScreen::WIFI_STATION:
        case RTC::MatrixMenuScreen::WIFI_ACCESS_POINT:
        case RTC::MatrixMenuScreen::WIFI_DISABLED:
            snprintf(buf, bufSize, "%s", wifiActionLabel(actionForScreen(screen)));
            return;
        case RTC::MatrixMenuScreen::EXIT:
            snprintf(buf, bufSize, "EXIT");
            return;
        default:
            snprintf(buf, bufSize, "");
            return;
    }
}

}  // namespace MATRIX
