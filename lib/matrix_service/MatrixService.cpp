#include "MatrixService.h"
#include <esp_log.h>
#include <freertos/task.h> // For vTaskDelay
#include <esp_heap_caps.h>
#include <new> // For placement new

static const char* TAG = "MatrixService";

MatrixService::MatrixService()
    : _renderer(), _state(), 
      _displayStartMs(0), _displayDurationMs(0), _autoClearing(false) {
}

void MatrixService::init(uint8_t pin) {
    _renderer.begin(pin);
}

void MatrixService::setCustomIcon(IconType type, const uint32_t* bitmap) {
    _state.setCustomIcon(type, bitmap);
    // Invalidate active icon so the SHOW_ICON guard allows re-rendering
    // with the updated bitmap data (even if the IconType hasn't changed)
    if (_activeIcon == type) {
        _activeIcon = IconType::NONE;
    }
}

void MatrixService::loop() {
    uint32_t now = millis();

    // ─────────────────────────────────────────────────────────────
    // Phase 1: Process Pending Commands
    // ─────────────────────────────────────────────────────────────
    
    MatrixCommand cmd;
    // Process one command per loop iteration for stability
    if (_state.poll(cmd)) {
        switch (cmd.type) {
            case CommandType::SET_BRIGHTNESS:
                _renderer.setBrightness(cmd.value8);
                break;
                
            case CommandType::SET_ROTATION:
                _renderer.setRotation(cmd.value8);
                break;
                
            case CommandType::CLEAR:
                _autoClearing = false;
                _activeIcon = IconType::NONE;
                {
                    // Single fetch under mutex (avoids redundant lock)
                    auto bgViz = _state.getBackgroundDataVisualization();
                    auto bg = _state.getBackgroundEffect();
                    if (!cmd.stopBackground && bgViz.active) {
                        _renderer.showDataVisualization(bgViz.config);
                        ESP_LOGD(TAG, "Restored background data visualization (Clear)");
                    } else if (!cmd.stopBackground && bg.active) {
                        if (bg.engine == static_cast<uint8_t>(MATRIX_FX::EffectEngine::Native3D)) {
                            _renderer.showNative3DEffect(
                                bg.mode,
                                bg.speed,
                                bg.color,
                                bg.color2,
                                bg.color3,
                                bg.reactivityProvider,
                                bg.reactivityGain);
                        } else {
                            _renderer.showEffect(bg.mode, bg.speed, bg.color, bg.color2, bg.color3);
                        }
                        ESP_LOGD(TAG, "Restored background effect (Clear)");
                    } else {
                        _renderer.clear();
                    }
                }
                break;
                
            case CommandType::SHOW_ICON:
                if (cmd.icon != _activeIcon) {
                    uint32_t iconBuf[64];
                    bool hasCustom = _state.getCustomIcon(cmd.icon, iconBuf);
                    _renderer.showIcon(cmd.icon, hasCustom ? iconBuf : nullptr);
                    _activeIcon = cmd.icon;
                }
                if (cmd.durationMs > 0) {
                    _displayStartMs = now;
                    _displayDurationMs = cmd.durationMs;
                    _autoClearing = true;
                } else {
                    _autoClearing = false;
                }
                break;
                
            case CommandType::SHOW_TEXT:
                _activeIcon = IconType::NONE;
                _renderer.showText(cmd.text, cmd.color);
                
                if (cmd.durationMs > 0) {
                    _displayStartMs = now;
                    _displayDurationMs = cmd.durationMs;
                    _autoClearing = true;
                } else {
                    _autoClearing = false;
                }
                break;
                
            case CommandType::SHOW_SOLID:
                _autoClearing = false;
                _activeIcon = IconType::NONE;
                _renderer.showSolid(cmd.color);
                break;
                
            case CommandType::SHOW_EFFECT:
                _activeIcon = IconType::NONE;
                if (cmd.effectEngine == static_cast<uint8_t>(MATRIX_FX::EffectEngine::Native3D)) {
                    _renderer.showNative3DEffect(
                        cmd.value8,
                        cmd.effectSpeedMs,
                        cmd.value32,
                        cmd.value32_2,
                        cmd.value32_3,
                        cmd.effectReactivityProvider,
                        cmd.effectReactivityGain);
                } else {
                    _renderer.showEffect(
                        cmd.value8,
                        cmd.effectSpeedMs,
                        cmd.value32,
                        cmd.value32_2,
                        cmd.value32_3);
                }
                
                if (cmd.durationMs > 0) {
                    _displayStartMs = now;
                    _displayDurationMs = cmd.durationMs;
                    _autoClearing = true;
                } else {
                    _autoClearing = false;
                }
                break;

            case CommandType::SHOW_DATA_VISUALIZATION:
                _activeIcon = IconType::NONE;
                _renderer.showDataVisualization(cmd.dataVisualizationConfig);

                if (cmd.durationMs > 0) {
                    _displayStartMs = now;
                    _displayDurationMs = cmd.durationMs;
                    _autoClearing = true;
                } else {
                    _autoClearing = false;
                }
                break;
                
            default:
                break;
        }
    }

    // ─────────────────────────────────────────────────────────────
    // Phase 2: Auto-clear timeout check
    // ─────────────────────────────────────────────────────────────
    
    if (_autoClearing && _displayDurationMs > 0) {
        if ((now - _displayStartMs) >= _displayDurationMs) {
            _autoClearing = false;
            _activeIcon = IconType::NONE;
            
            // Restore background effect if active
            auto bgViz = _state.getBackgroundDataVisualization();
            auto bg = _state.getBackgroundEffect();
            if (bgViz.active) {
                _renderer.showDataVisualization(bgViz.config);
                ESP_LOGD(TAG, "Restored background data visualization (Timeout)");
            } else if (bg.active) {
                if (bg.engine == static_cast<uint8_t>(MATRIX_FX::EffectEngine::Native3D)) {
                    _renderer.showNative3DEffect(
                        bg.mode,
                        bg.speed,
                        bg.color,
                        bg.color2,
                        bg.color3,
                        bg.reactivityProvider,
                        bg.reactivityGain);
                } else {
                    _renderer.showEffect(bg.mode, bg.speed, bg.color, bg.color2, bg.color3);
                }
                ESP_LOGD(TAG, "Restored background effect (Timeout)");
            } else {
                _renderer.clear();
                ESP_LOGD(TAG, "Auto-cleared display");
            }
        }
    }

    // ─────────────────────────────────────────────────────────────
    // Phase 3: Animation Loop
    // ─────────────────────────────────────────────────────────────
    _renderer.loop();
}

void MatrixService::showText(const char* text, uint32_t color, uint32_t durationMs) {
    ESP_LOGD(TAG, "showText='%s' duration=%lu", text ? text : "null", durationMs);
    _state.requestText(text, color, durationMs);
}

void MatrixService::showIcon(IconType icon, uint32_t durationMs) {
    ESP_LOGD(TAG, "showIcon=%d duration=%lu", (int)icon, durationMs);
    _state.requestIcon(icon, durationMs);
}

void MatrixService::clear(bool stopBackground) {
    _state.requestClear(stopBackground);
}

void MatrixService::clearBackgroundEffect() {
    // Keep this separate from clear(false): layered callers may want to retire
    // the remembered idle animation without blanking higher-priority content.
    _state.clearBackgroundEffect();
}

void MatrixService::clearBackgroundDataVisualization() {
    _state.clearBackgroundDataVisualization();
}

void MatrixService::setBrightness(uint8_t brightness) {
    _state.setBrightness(brightness);
}

void MatrixService::setThermalBrightnessLimit(uint8_t limit) {
    ESP_LOGD(TAG, "Thermal limit set to %u", limit);
    _state.setThermalBrightnessLimit(limit);
}

void MatrixService::setRotation(uint8_t rotation) {
    _state.setRotation(rotation);
}

void MatrixService::setScrollSpeed(uint16_t ms) {
    // Direct write is safe: uint16_t is atomic on ESP32,
    // and scroll speed is a non-critical soft parameter.
    _renderer.setScrollSpeed(ms);
}

void MatrixService::setEffectInput(const MATRIX_FX::MatrixFxInput& input) {
    _renderer.setEffectInput(input);
}

void MatrixService::setDataVisualizationInput(const MATRIX::MatrixDataVisualizationInput& input) {
    _renderer.setDataVisualizationInput(input);
}

void MatrixService::showEffect(uint8_t mode,
                               uint32_t speed,
                               uint32_t color,
                               uint32_t color2,
                               uint32_t color3,
                               uint32_t durationMs,
                               uint8_t engine,
                               uint8_t reactivityProvider,
                               uint8_t reactivityGain) {
    ESP_LOGD(TAG, "showEffect engine=%u mode=%u spd=%lu",
             engine,
             mode,
             static_cast<unsigned long>(speed));
    _state.requestEffect(
        mode,
        speed,
        color,
        color2,
        color3,
        durationMs,
        engine,
        reactivityProvider,
        reactivityGain);
}

void MatrixService::showDataVisualization(const MATRIX::MatrixDataVisualizationConfig& config, uint32_t durationMs) {
    ESP_LOGD(TAG, "showDataVisualization source=%u metric=%u mode=%u duration=%lu",
             config.source,
             config.metric,
             config.mode,
             static_cast<unsigned long>(durationMs));
    _state.requestDataVisualization(config, durationMs);
}

void MatrixService::showSolidColor(uint32_t color) {
    _state.requestSolid(color);
}

bool MatrixService::isActive() const {
    return _renderer.isActive();
}
