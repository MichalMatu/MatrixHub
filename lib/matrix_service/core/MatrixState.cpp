#include "MatrixState.h"
#include "../../../src/system/utils/ScopeLock.h"
#include <esp_log.h> // Ensure logging available

// Reuse TAG if already defined, else define
#ifndef LOG_TAG
#define LOG_TAG "MatrixState"
#endif

MatrixState::MatrixState() {
    _mutex = xSemaphoreCreateMutexStatic(&_mutexBuffer);
    // Zero out custom icons
    memset(_customIcons, 0, sizeof(_customIcons));
}

void MatrixState::requestIcon(IconType icon, uint32_t durationMs) {
    SYSTEM::ScopeLock lock(_mutex);
    _pendingIcon = icon;
    _pendingIconDuration = durationMs;
    _flags.iconDirty = true;
    _currentMode = MatrixMode::ACTIVE_ICON;
    // Lower priority resets
    _flags.textDirty = false;
    _flags.colorDirty = false;
    _flags.effectDirty = false;
    _flags.dataVisualizationDirty = false;
}

void MatrixState::requestText(const char* text, uint32_t color, uint32_t durationMs) {
    SYSTEM::ScopeLock lock(_mutex);
    if (text) strlcpy(_pendingText, text, sizeof(_pendingText));
    else _pendingText[0] = '\0';
    _pendingTextColor = color;
    _pendingTextDuration = durationMs;
    _flags.textDirty = true;
    _currentMode = MatrixMode::ACTIVE_TEXT;
    _flags.colorDirty = false;
    _flags.effectDirty = false;
    _flags.dataVisualizationDirty = false;
}

void MatrixState::requestClear(bool stopBackground) {
    SYSTEM::ScopeLock lock(_mutex);
    _flags.stopBackgroundOnClear = stopBackground;
    _flags.clearDirty = true;
    
    if (stopBackground) {
        _bgEffect.active = false;
        _bgDataVisualization.active = false;
    }
    _currentMode = MatrixMode::OFF;
}

void MatrixState::requestSolid(uint32_t color) {
    SYSTEM::ScopeLock lock(_mutex);
    _activeSolidColor = color;
    _flags.colorDirty = true;
    _currentMode = MatrixMode::ACTIVE_SOLID;
}

void MatrixState::requestEffect(uint8_t mode,
                                uint32_t speed,
                                uint32_t color,
                                uint32_t color2,
                                uint32_t color3,
                                uint32_t durationMs,
                                uint8_t engine,
                                uint8_t reactivityProvider,
                                uint8_t reactivityGain) {
    SYSTEM::ScopeLock lock(_mutex);
    _pendingEffectMode = mode;
    _pendingEffectEngine = engine;
    _pendingEffectSpeed = speed;
    _pendingEffectColor = color;
    _pendingEffectColor2 = color2;
    _pendingEffectColor3 = color3;
    _pendingEffectDuration = durationMs;
    _pendingEffectReactivityProvider = reactivityProvider;
    _pendingEffectReactivityGain = reactivityGain;
    _flags.effectDirty = true;
    _flags.dataVisualizationDirty = false;
    
    // Background effect logic
    if (durationMs == 0) {
        _bgEffect.active = true;
        _bgDataVisualization.active = false;
        _bgEffect.mode = mode;
        _bgEffect.engine = engine;
        _bgEffect.speed = speed;
        _bgEffect.color = color;
        _bgEffect.color2 = color2;
        _bgEffect.color3 = color3;
        _bgEffect.reactivityProvider = reactivityProvider;
        _bgEffect.reactivityGain = reactivityGain;
    }
    _currentMode = MatrixMode::ACTIVE_EFFECT;
}

void MatrixState::requestDataVisualization(const MATRIX::MatrixDataVisualizationConfig& config, uint32_t durationMs) {
    SYSTEM::ScopeLock lock(_mutex);
    _pendingDataVisualizationConfig = config;
    _pendingDataVisualizationDuration = durationMs;
    _flags.dataVisualizationDirty = true;
    _flags.effectDirty = false;

    if (durationMs == 0) {
        _bgDataVisualization.active = true;
        _bgDataVisualization.config = config;
        _bgEffect.active = false;
    }
    _currentMode = MatrixMode::ACTIVE_DATA_VISUALIZATION;
}

void MatrixState::setBrightness(uint8_t brightness) {
    SYSTEM::ScopeLock lock(_mutex);
    _userTargetBrightness = brightness;
    
    // Calculate effective brightness
    uint8_t effective = _userTargetBrightness;
    if (_thermalLimit < effective) {
        effective = _thermalLimit;
    }
    
    // Only update if effective value changed
    if (_pendingBrightness != effective || !_flags.brightnessDirty) {
        _pendingBrightness = effective;
        _flags.brightnessDirty = true;
    }
}

void MatrixState::setThermalBrightnessLimit(uint8_t limit) {
    SYSTEM::ScopeLock lock(_mutex);
    _thermalLimit = limit;
    
    // Re-evaluate effective brightness
    uint8_t effective = _userTargetBrightness;
    if (_thermalLimit < effective) {
        effective = _thermalLimit;
    }
    
    // Force update if changed
    if (_pendingBrightness != effective) {
        _pendingBrightness = effective;
        _flags.brightnessDirty = true;
    }
}

void MatrixState::setRotation(uint8_t rotation) {
    SYSTEM::ScopeLock lock(_mutex);
    _pendingRotation = rotation;
    _flags.rotationDirty = true;
}

void MatrixState::setNotificationColor(uint32_t color) {
    SYSTEM::ScopeLock lock(_mutex);
    if (_currentMode == MatrixMode::OFF || _currentMode == MatrixMode::PASSIVE) {
        if (_notificationColor != color) {
            _notificationColor = color;
            _flags.colorDirty = true; // Treats as Solid request but using notification color storage
            _activeSolidColor = color; // Reuse solid logic
            _currentMode = MatrixMode::PASSIVE;
        }
    }
}

bool MatrixState::poll(MatrixCommand& cmd) {
    SYSTEM::ScopeLock lock(_mutex);
    
    // 1. Hardware updates (Highest Priority check but doesn't consume Main State)
    if (_flags.brightnessDirty) {
        cmd.type = CommandType::SET_BRIGHTNESS;
        cmd.value8 = _pendingBrightness;
        _flags.brightnessDirty = false;
        return true;
    }
    
    if (_flags.rotationDirty) {
        cmd.type = CommandType::SET_ROTATION;
        cmd.value8 = _pendingRotation;
        _flags.rotationDirty = false;
        return true;
    }
    
    // 2. Logic Updates (Priority Order)
    if (_flags.clearDirty) {
        cmd.type = CommandType::CLEAR;
        cmd.stopBackground = _flags.stopBackgroundOnClear;
        _flags.clearDirty = false;
        _flags.iconDirty = false;
        _flags.textDirty = false;
        _flags.colorDirty = false;
        _flags.effectDirty = false;
        _flags.dataVisualizationDirty = false;
        return true;
    }
    
    if (_flags.iconDirty) {
        cmd.type = CommandType::SHOW_ICON;
        cmd.icon = _pendingIcon;
        cmd.durationMs = _pendingIconDuration;
        _flags.iconDirty = false;
        return true;
    }
    
    if (_flags.textDirty) {
        cmd.type = CommandType::SHOW_TEXT;
        strlcpy(cmd.text, _pendingText, sizeof(cmd.text));
        cmd.color = _pendingTextColor;
        cmd.durationMs = _pendingTextDuration;
        _flags.textDirty = false;
        return true;
    }
    
    if (_flags.colorDirty) {
        cmd.type = CommandType::SHOW_SOLID;
        cmd.color = _activeSolidColor;
        _flags.colorDirty = false;
        return true;
    }
    
    if (_flags.effectDirty) {
        cmd.type = CommandType::SHOW_EFFECT;
        cmd.value8 = _pendingEffectMode;  // Mode
        cmd.effectEngine = _pendingEffectEngine;
        cmd.effectSpeedMs = _pendingEffectSpeed;
        cmd.value32 = _pendingEffectColor; // Color
        cmd.value32_2 = _pendingEffectColor2; // Color2
        cmd.value32_3 = _pendingEffectColor3; // Color3
        cmd.durationMs = _pendingEffectDuration;
        cmd.effectReactivityProvider = _pendingEffectReactivityProvider;
        cmd.effectReactivityGain = _pendingEffectReactivityGain;
        _flags.effectDirty = false;
        return true;
    }

    if (_flags.dataVisualizationDirty) {
        cmd.type = CommandType::SHOW_DATA_VISUALIZATION;
        cmd.dataVisualizationConfig = _pendingDataVisualizationConfig;
        cmd.durationMs = _pendingDataVisualizationDuration;
        _flags.dataVisualizationDirty = false;
        return true;
    }
    
    return false;
}

MatrixState::BgEffect MatrixState::getBackgroundEffect() const {
    BgEffect copy;
    SYSTEM::ScopeLock lock(_mutex);
    copy = _bgEffect;
    return copy;
}

void MatrixState::clearBackgroundEffect() {
    SYSTEM::ScopeLock lock(_mutex);
    _bgEffect.active = false;
}

MatrixState::BgDataVisualization MatrixState::getBackgroundDataVisualization() const {
    BgDataVisualization copy;
    SYSTEM::ScopeLock lock(_mutex);
    copy = _bgDataVisualization;
    return copy;
}

void MatrixState::clearBackgroundDataVisualization() {
    SYSTEM::ScopeLock lock(_mutex);
    _bgDataVisualization.active = false;
}

void MatrixState::setCustomIcon(IconType type, const uint32_t* bitmap) {
    SYSTEM::ScopeLock lock(_mutex);
    
    // Map IconType (1,2,3) to index (0,1,2)
    int index = (int)type - 1;
    if (index >= 0 && index < 3) {
        if (bitmap) {
            memcpy(_customIcons[index], bitmap, sizeof(uint32_t) * 64);
        } else {
            memset(_customIcons[index], 0, sizeof(uint32_t) * 64);
        }
    }
}

bool MatrixState::getCustomIcon(IconType type, uint32_t* outBuffer) const {
    int index = (int)type - 1;
    if (index < 0 || index >= 3 || !outBuffer) return false;

    SYSTEM::ScopeLock lock(_mutex);
    
    // Determine has state by checking if any pixel is non-zero
    bool has = false;
    for (int i = 0; i < 64; ++i) {
        if (_customIcons[index][i] != 0) {
            has = true;
            break;
        }
    }
    
    if (has) {
        memcpy(outBuffer, _customIcons[index], sizeof(uint32_t) * 64);
    }
    return has;
}
