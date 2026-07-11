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
    void showIcon(IconType icon, uint32_t duration) {
        (void)duration;
        lastIcon = icon;
        showIconCalls++;
    }
    void showText(const char* text, uint32_t color, uint32_t duration) {
        (void)duration;
        strlcpy(lastText, text ? text : "", sizeof(lastText));
        lastTextColor = color;
        showTextCalls++;
    }
    void setEffectInput(const MATRIX_FX::MatrixFxInput& input) {
        lastEffectInput = input;
        setEffectInputCalls++;
    }
    void setDataVisualizationInput(const MATRIX::MatrixDataVisualizationInput& input) {
        lastDataVisualizationInput = input;
        dataVisualizationStatus.input = input;
        setDataVisualizationInputCalls++;
    }
    void setDataVisualizationStatusConfig(
        const MATRIX::MatrixDataVisualizationConfig& config,
        bool active) {
        lastDataVisualizationConfig = config;
        dataVisualizationStatus.active = active;
        dataVisualizationStatus.config = config;
        setDataVisualizationStatusConfigCalls++;
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
    void showSolidColor(uint32_t color) {
        lastSolidColor = color;
        showSolidCalls++;
    }
    void showDataVisualization(const MATRIX::MatrixDataVisualizationConfig& config, uint32_t duration = 0) {
        (void)duration;
        lastDataVisualizationConfig = config;
        dataVisualizationStatus.active = config.enabled;
        dataVisualizationStatus.config = config;
        showDataVisualizationCalls++;
    }
    void clear(bool stopBackground = true) {
        lastClearStopBackground = stopBackground;
        if (stopBackground) {
            dataVisualizationStatus.active = false;
        }
        clearCalls++;
    }
    void clearBackgroundEffect() { clearBackgroundEffectCalls++; }
    void clearBackgroundDataVisualization() {
        dataVisualizationStatus.active = false;
        clearBackgroundDataVisualizationCalls++;
    }
    void setBrightness(uint8_t brightness) {}
    void blackoutForShutdown() {
        blackoutForShutdownCalls++;
        if (blackoutForShutdownHook) {
            blackoutForShutdownHook();
        }
    }
    void setScrollSpeed(uint16_t speed) { lastScrollSpeed = speed; }
    uint8_t lastLimit = 255;
    void setThermalBrightnessLimit(uint8_t limit) {
        lastLimit = limit;
        if (thermalLimitHistoryCount < sizeof(thermalLimitHistory)) {
            thermalLimitHistory[thermalLimitHistoryCount++] = limit;
        }
        setThermalBrightnessLimitCalls++;
        if (thermalBrightnessLimitHook) {
            thermalBrightnessLimitHook(limit);
        }
    }
    void resetThermalBrightnessHistory() {
        lastLimit = 255;
        setThermalBrightnessLimitCalls = 0;
        thermalLimitHistoryCount = 0;
        thermalBrightnessLimitHook = nullptr;
    }
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
    MATRIX::MatrixDataVisualizationStatusSnapshot getDataVisualizationStatusSnapshot() const {
        return dataVisualizationStatus;
    }
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return 0; }

    char lastText[kMatrixTextCapacity] = {0};
    uint32_t lastTextColor = 0;
    IconType lastIcon = IconType::NONE;
    uint32_t lastSolidColor = 0;
    uint32_t showIconCalls = 0;
    uint32_t showTextCalls = 0;
    uint32_t showSolidCalls = 0;
    uint32_t showEffectCalls = 0;
    uint32_t showDataVisualizationCalls = 0;
    uint32_t clearCalls = 0;
    uint32_t clearBackgroundEffectCalls = 0;
    uint32_t clearBackgroundDataVisualizationCalls = 0;
    uint32_t blackoutForShutdownCalls = 0;
    uint32_t setThermalBrightnessLimitCalls = 0;
    uint8_t thermalLimitHistory[16] = {};
    uint8_t thermalLimitHistoryCount = 0;
    void (*thermalBrightnessLimitHook)(uint8_t) = nullptr;
    void (*blackoutForShutdownHook)() = nullptr;
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
    uint32_t setDataVisualizationStatusConfigCalls = 0;
    MATRIX_FX::MatrixFxInput lastEffectInput{};
    MATRIX::MatrixDataVisualizationConfig lastDataVisualizationConfig{};
    MATRIX::MatrixDataVisualizationInput lastDataVisualizationInput{};
    MATRIX::MatrixDataVisualizationStatusSnapshot dataVisualizationStatus{};
    bool customIconAssigned[4] = {false, false, false, false};
};
