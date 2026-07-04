#pragma once

#include <Arduino.h>
#include "types/MatrixTypes.h"
#include "../../lib/matrix_service/effects/MatrixFxTypes.h"
#include "../../src/matrix/MatrixDataVisualizationTypes.h"

class MatrixService {
public:
    MatrixService() = default;

    void init(uint8_t pin) {}
    void loop() {}
    void showIcon(IconType icon, uint32_t duration) {}
    void showText(const char* text, uint32_t color, uint32_t duration) {
        (void)duration;
        lastText = text ? text : "";
        lastTextColor = color;
        showTextCalls++;
    }
    void setEffectInput(const MATRIX_FX::MatrixFxInput& input) {
        lastEffectInput = input;
        setEffectInputCalls++;
    }
    void setDataVisualizationInput(const MATRIX::MatrixDataVisualizationInput& input) {
        lastDataVisualizationInput = input;
        setDataVisualizationInputCalls++;
    }
    void showEffect(uint8_t mode,
                    uint32_t speed,
                    uint32_t color1,
                    uint32_t color2,
                    uint32_t color3,
                    uint32_t duration = 0,
                    uint8_t engine = 0,
                    uint8_t reactivityProvider = 0,
                    uint8_t reactivityGain = 0) {
        (void)duration;
        lastEffectMode = mode;
        lastEffectSpeed = speed;
        lastEffectColor1 = color1;
        lastEffectColor2 = color2;
        lastEffectColor3 = color3;
        lastEffectEngine = engine;
        lastEffectReactivityProvider = reactivityProvider;
        lastEffectReactivityGain = reactivityGain;
        showEffectCalls++;
    }
    void showSolidColor(uint16_t color) {}
    void showDataVisualization(const MATRIX::MatrixDataVisualizationConfig& config, uint32_t duration = 0) {
        (void)duration;
        lastDataVisualizationConfig = config;
        showDataVisualizationCalls++;
    }
    void clear(bool stopBackground = true) {
        lastClearStopBackground = stopBackground;
        clearCalls++;
    }
    void clearBackgroundEffect() { clearBackgroundEffectCalls++; }
    void clearBackgroundDataVisualization() { clearBackgroundDataVisualizationCalls++; }
    void setBrightness(uint8_t brightness) {}
    void setScrollSpeed(uint16_t speed) { lastScrollSpeed = speed; }
    uint8_t lastLimit = 255;
    void setThermalBrightnessLimit(uint8_t limit) { lastLimit = limit; }
    void setRotation(uint8_t rotation) {
        lastRotation = rotation;
        setRotationCalls++;
    }
    void setCustomIcon(IconType type, const uint32_t* bitmap) {
        const size_t index = static_cast<size_t>(type);
        if (index < 4) {
            customIconAssigned[index] = bitmap != nullptr;
        }
    }
    bool isActive() const { return false; }
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return 0; }

    const char* lastText = nullptr;
    uint32_t lastTextColor = 0;
    uint32_t showTextCalls = 0;
    uint32_t showEffectCalls = 0;
    uint32_t showDataVisualizationCalls = 0;
    uint32_t clearCalls = 0;
    uint32_t clearBackgroundEffectCalls = 0;
    uint32_t clearBackgroundDataVisualizationCalls = 0;
    bool lastClearStopBackground = false;
    uint8_t lastEffectMode = 0;
    uint32_t lastEffectSpeed = 0;
    uint8_t lastEffectEngine = 0;
    uint8_t lastEffectReactivityProvider = 0;
    uint8_t lastEffectReactivityGain = 0;
    uint32_t lastEffectColor1 = 0;
    uint32_t lastEffectColor2 = 0;
    uint32_t lastEffectColor3 = 0;
    uint16_t lastScrollSpeed = 0;
    uint8_t lastRotation = 0;
    uint32_t setRotationCalls = 0;
    uint32_t setEffectInputCalls = 0;
    uint32_t setDataVisualizationInputCalls = 0;
    MATRIX_FX::MatrixFxInput lastEffectInput{};
    MATRIX::MatrixDataVisualizationConfig lastDataVisualizationConfig{};
    MATRIX::MatrixDataVisualizationInput lastDataVisualizationInput{};
    bool customIconAssigned[4] = {false, false, false, false};
};
