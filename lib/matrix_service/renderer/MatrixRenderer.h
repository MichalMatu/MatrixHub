#pragma once

#include <Arduino.h>
#include <LedMatrix.h>
#include "../types/MatrixTypes.h"
#include "../../src/config/System.h" // For UI constants
#include "../effects/MatrixFxEngine3D.h"
#include "../visualization/MatrixDataVisualizationEngine.h"
#include "IconDrawer.h"
#include "TextDrawer.h"

class MatrixRenderer {
public:
    MatrixRenderer();
    
    void begin(uint8_t pin);
    void loop();
    
    // Command Interface
    void showText(const char* text, uint32_t color);
    void showIcon(IconType icon, const uint32_t* customBitmap = nullptr);
    void showSolid(uint32_t color);
    void showEffect(uint8_t mode, uint32_t speed, uint32_t color, uint32_t color2, uint32_t color3);
    void showNative3DEffect(uint8_t mode,
                            uint32_t speed,
                            uint32_t color,
                            uint32_t color2,
                            uint32_t color3,
                            uint8_t reactivityProvider,
                            uint8_t reactivityGain);
    void setEffectInput(const MATRIX_FX::MatrixFxInput& input);
    void showDataVisualization(const MATRIX::MatrixDataVisualizationConfig& config);
    void setDataVisualizationInput(const MATRIX::MatrixDataVisualizationInput& input);
    void clear();
    void setBrightness(uint8_t brightness);
    void setRotation(uint8_t rotation);
    void setScrollSpeed(uint16_t ms);
    
    // State Check
    bool isActive() const;
    
private:
    bool isDisplayMuted() const { return _brightness == 0; }
    void blackout();

    LedMatrix* _matrix;
    uint8_t _pin;
    uint8_t _brightness;
    
    // State Flags
    bool _scrolling;
    bool _effectRunning;
    bool _nativeEffectRunning = false;
    bool _dataVisualizationRunning = false;
    IconType _activeIcon = IconType::NONE;
    bool _hasActiveIconBitmap = false;
    uint32_t _activeIconBitmap[64];
    
    // Scrolling State
    char _text[kMatrixRenderedTextCapacity] = {0};
    uint32_t _color;       // RGB888
    int _x;                // Current scroll position (pixels)
    int _minX;             // Leftmost scroll position before reset
    uint32_t _lastScrollUpdate;
    uint16_t _scrollSpeed = UI::MATRIX::SCROLL_INTERVAL_MS;
    uint32_t _effectSpeedMs = UI::MATRIX::DEFAULT_EFFECT_SPEED;
    uint32_t _nativeFrame[MATRIX_FX::kMatrixFxPixelCount] = {};
    MATRIX_FX::MatrixFxEngine3D _nativeEngine;
    uint32_t _dataVisualizationFrame[MATRIX::kMatrixDataVizPixelCount] = {};
    MATRIX_VIZ::MatrixDataVisualizationEngine _dataVisualizationEngine;
};
