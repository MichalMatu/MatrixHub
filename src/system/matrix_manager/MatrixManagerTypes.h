#pragma once

#include <cstdint>
#include <cstring>
#include "types/MatrixTypes.h"
#include "core/MatrixCommand.h"

namespace MATRIX_MANAGER {

/**
 * @brief Z-index layers for Matrix display priority system.
 * Higher value = higher priority. Only the top active layer is rendered.
 */
enum class Layer : uint8_t {
    BACKGROUND    = 0,  // Effects, idle animations
    IDLE          = 1,  // Default state (heartbeat, etc.)
    ALARM         = 2,  // Warnings/Alerts from Alarm Service
    NOTIFICATION  = 3,  // Queued runtime/API messages
    SYSTEM_MODAL  = 4,  // Macro scripts running/printing (overrides alarms, but self-clears)
    MENU          = 5   // Button-activated menu (Absolute highest priority)
};

constexpr uint8_t LAYER_COUNT = 6;

/**
 * @brief Content descriptor for a single layer.
 */
struct LayerContent {
    bool active = false;
    CommandType type = CommandType::NONE;
    
    // Text content
    char text[kMatrixTextCapacity] = {0};
    uint32_t color = 0xFFFFFF;  // RGB888
    IconType icon = IconType::NONE;
    uint32_t durationMs = 0;
    
    // Effect fields
    uint8_t effectEngine = 0;
    uint8_t effectMode = 0;
    uint32_t effectSpeed = 0;
    uint32_t effectColor = 0;
    uint32_t effectColor2 = 0;
    uint32_t effectColor3 = 0;
    uint8_t effectReactivityProvider = 0;
    uint8_t effectReactivityGain = 0;
    MATRIX::MatrixDataVisualizationConfig dataVisualizationConfig{};

    void clear() {
        active = false;
        type = CommandType::NONE;
        text[0] = '\0';
        color = 0xFFFFFF;
        icon = IconType::NONE;
        durationMs = 0;
        effectEngine = 0;
        effectMode = 0;
        effectSpeed = 0;
        effectColor = 0;
        effectColor2 = 0;
        effectColor3 = 0;
        effectReactivityProvider = 0;
        effectReactivityGain = 0;
        dataVisualizationConfig = MATRIX::MatrixDataVisualizationConfig{};
    }
};

/**
 * @brief Single notification for the FIFO queue.
 */
struct Notification {
    char text[kMatrixTextCapacity] = {0};
    uint32_t color = 0xFFFFFF;   // RGB888
    uint32_t displayMs = 3000; // How long to show on screen

    void setText(const char* src) {
        if (src) {
            strlcpy(text, src, sizeof(text));
        } else {
            text[0] = '\0';
        }
    }
};

} // namespace MATRIX_MANAGER
