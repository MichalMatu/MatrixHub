#include "MatrixState.h"
#include "../../../src/system/utils/ScopeLock.h"
#include <esp_log.h> // Ensure logging available

// Reuse TAG if already defined, else define
#ifndef LOG_TAG
#define LOG_TAG "MatrixState"
#endif

MatrixState::MatrixState() {
    _mutex = xSemaphoreCreateMutexStatic(&_mutexBuffer);
    configASSERT(_mutex != nullptr);
    // Zero out custom icons
    memset(_customIcons, 0, sizeof(_customIcons));
}

MatrixCommand& MatrixState::replacePendingContent(CommandType type) {
    _pendingContent = MatrixCommand{};
    _pendingContent.type = type;
    _flags.contentDirty = true;
    return _pendingContent;
}

void MatrixState::requestIcon(IconType icon, uint32_t durationMs) {
    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    configASSERT(lock.isLocked());
    if (!lock.isLocked()) return;
    MatrixCommand& command = replacePendingContent(CommandType::SHOW_ICON);
    command.icon = icon;
    command.durationMs = durationMs;
    _currentMode = MatrixMode::ACTIVE_ICON;
}

void MatrixState::requestText(const char* text, uint32_t color, uint32_t durationMs) {
    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    configASSERT(lock.isLocked());
    if (!lock.isLocked()) return;
    MatrixCommand& command = replacePendingContent(CommandType::SHOW_TEXT);
    if (text) strlcpy(command.text, text, sizeof(command.text));
    command.color = color;
    command.durationMs = durationMs;
    _currentMode = MatrixMode::ACTIVE_TEXT;
}

void MatrixState::requestClear(bool stopBackground) {
    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    configASSERT(lock.isLocked());
    if (!lock.isLocked()) return;
    MatrixCommand& command = replacePendingContent(CommandType::CLEAR);
    command.stopBackground = stopBackground;
    
    if (stopBackground) {
        _bgEffect.active = false;
        _bgDataVisualization.active = false;
    }
    _notificationColor = 0;
    _currentMode = MatrixMode::OFF;
}

void MatrixState::requestSolid(uint32_t color) {
    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    configASSERT(lock.isLocked());
    if (!lock.isLocked()) return;
    MatrixCommand& command = replacePendingContent(CommandType::SHOW_SOLID);
    command.color = color;
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
    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    configASSERT(lock.isLocked());
    if (!lock.isLocked()) return;
    MatrixCommand& command = replacePendingContent(CommandType::SHOW_EFFECT);
    command.value8 = mode;
    command.effectEngine = engine;
    command.effectSpeedMs = speed;
    command.value32 = color;
    command.value32_2 = color2;
    command.value32_3 = color3;
    command.durationMs = durationMs;
    command.effectReactivityProvider = reactivityProvider;
    command.effectReactivityGain = reactivityGain;
    
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
    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    configASSERT(lock.isLocked());
    if (!lock.isLocked()) return;
    MatrixCommand& command = replacePendingContent(CommandType::SHOW_DATA_VISUALIZATION);
    command.dataVisualizationConfig = config;
    command.durationMs = durationMs;

    if (durationMs == 0) {
        _bgDataVisualization.active = true;
        _bgDataVisualization.config = config;
        _bgEffect.active = false;
    }
    _currentMode = MatrixMode::ACTIVE_DATA_VISUALIZATION;
}

void MatrixState::setBrightness(uint8_t brightness) {
    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    configASSERT(lock.isLocked());
    if (!lock.isLocked()) return;
    _userTargetBrightness = brightness;
    
    // Calculate effective brightness
    uint8_t effective = _userTargetBrightness;
    if (_thermalLimit < effective) {
        effective = _thermalLimit;
    }
    
    _pendingBrightness = effective;
    // Coalesce all writers against the value already published to the renderer.
    // This also cancels a pending change if a newer update returns to the
    // currently visible value before MatrixTask polls the mailbox.
    _flags.brightnessDirty = (_pendingBrightness != _lastPublishedBrightness);
}

void MatrixState::setThermalBrightnessLimit(uint8_t limit) {
    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    configASSERT(lock.isLocked());
    if (!lock.isLocked()) return;
    _thermalLimit = limit;
    
    // Re-evaluate effective brightness
    uint8_t effective = _userTargetBrightness;
    if (_thermalLimit < effective) {
        effective = _thermalLimit;
    }
    
    _pendingBrightness = effective;
    _flags.brightnessDirty = (_pendingBrightness != _lastPublishedBrightness);
}

void MatrixState::setRotation(uint8_t rotation) {
    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    configASSERT(lock.isLocked());
    if (!lock.isLocked()) return;
    _pendingRotation = rotation;
    _flags.rotationDirty = true;
}

void MatrixState::setNotificationColor(uint32_t color) {
    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    configASSERT(lock.isLocked());
    if (!lock.isLocked()) return;
    if (_currentMode == MatrixMode::OFF || _currentMode == MatrixMode::PASSIVE) {
        if (_notificationColor != color) {
            _notificationColor = color;
            MatrixCommand& command = replacePendingContent(CommandType::SHOW_SOLID);
            command.color = color;
            _currentMode = MatrixMode::PASSIVE;
        }
    }
}

bool MatrixState::poll(MatrixCommand& cmd) {
    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    configASSERT(lock.isLocked());
    if (!lock.isLocked()) return false;
    
    // 1. Hardware updates (Highest Priority check but doesn't consume Main State)
    if (_flags.brightnessDirty) {
        cmd.type = CommandType::SET_BRIGHTNESS;
        cmd.value8 = _pendingBrightness;
        _lastPublishedBrightness = _pendingBrightness;
        _flags.brightnessDirty = false;
        return true;
    }
    
    if (_flags.rotationDirty) {
        cmd.type = CommandType::SET_ROTATION;
        cmd.value8 = _pendingRotation;
        _flags.rotationDirty = false;
        return true;
    }
    
    // 2. Visible content is a single atomic latest-wins snapshot. Hardware
    // settings above may delay it, but can never expose an older content type
    // on a later MatrixTask tick.
    if (_flags.contentDirty) {
        cmd = _pendingContent;
        _flags.contentDirty = false;
        return true;
    }
    
    return false;
}

bool MatrixState::hasPendingCommands() const {
    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    configASSERT(lock.isLocked());
    if (!lock.isLocked()) return true;

    return _flags.brightnessDirty || _flags.rotationDirty || _flags.contentDirty;
}

MatrixState::BgEffect MatrixState::getBackgroundEffect() const {
    BgEffect copy{};
    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    configASSERT(lock.isLocked());
    if (!lock.isLocked()) return copy;
    copy = _bgEffect;
    return copy;
}

void MatrixState::clearBackgroundEffect() {
    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    configASSERT(lock.isLocked());
    if (!lock.isLocked()) return;
    _bgEffect.active = false;
    if (_flags.contentDirty &&
        _pendingContent.type == CommandType::SHOW_EFFECT &&
        _pendingContent.durationMs == 0) {
        _pendingContent = MatrixCommand{};
        _flags.contentDirty = false;
    }
}

MatrixState::BgDataVisualization MatrixState::getBackgroundDataVisualization() const {
    BgDataVisualization copy{};
    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    configASSERT(lock.isLocked());
    if (!lock.isLocked()) return copy;
    copy = _bgDataVisualization;
    return copy;
}

void MatrixState::clearBackgroundDataVisualization() {
    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    configASSERT(lock.isLocked());
    if (!lock.isLocked()) return;
    _bgDataVisualization.active = false;
    if (_flags.contentDirty &&
        _pendingContent.type == CommandType::SHOW_DATA_VISUALIZATION &&
        _pendingContent.durationMs == 0) {
        _pendingContent = MatrixCommand{};
        _flags.contentDirty = false;
    }
}

void MatrixState::setCustomIcon(IconType type, const uint32_t* bitmap) {
    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    configASSERT(lock.isLocked());
    if (!lock.isLocked()) return;
    
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

    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    configASSERT(lock.isLocked());
    if (!lock.isLocked()) return false;
    
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
