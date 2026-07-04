#pragma once

#include <Arduino.h>
#include "../types/MatrixTypes.h"
#include "../../../src/matrix/MatrixDataVisualizationTypes.h"

enum class CommandType {
    NONE,
    CLEAR,
    SHOW_ICON,
    SHOW_TEXT,
    SHOW_SOLID,
    SHOW_EFFECT,
    SHOW_DATA_VISUALIZATION,
    SET_BRIGHTNESS,
    SET_ROTATION
};

struct MatrixCommand {
    CommandType type = CommandType::NONE;
    
    // Payload
    IconType icon = IconType::NONE;
    char text[kMatrixTextCapacity] = {0};
    uint32_t color = 0;
    uint32_t durationMs = 0;
    uint8_t value8 = 0; // Brightness/Rotation/EffectMode
    uint8_t effectEngine = 0;
    uint32_t effectSpeedMs = 0;
    uint32_t value32 = 0; // EffectColor
    uint32_t value32_2 = 0; // EffectColor2
    uint32_t value32_3 = 0; // EffectColor3
    uint8_t effectReactivityProvider = 0;
    uint8_t effectReactivityGain = 0;
    MATRIX::MatrixDataVisualizationConfig dataVisualizationConfig{};
    
    // Flags
    bool stopBackground = true;
};
