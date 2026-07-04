#pragma once

#include "../../../src/config/UiConfig.h"
#include <cstddef>
#include <cstdint>

// Source of truth for matrix limits lives in src/config/UiConfig.h.
// Keep these aliases for the current module API surface to avoid a broad rename.
inline constexpr size_t kMatrixTextCapacity = UI::MATRIX::TEXT_CAPACITY;
inline constexpr size_t kMatrixTextPaddingChars = UI::MATRIX::TEXT_PADDING_CHARS;
inline constexpr size_t kMatrixRenderedTextCapacity =
    UI::MATRIX::RENDERED_TEXT_CAPACITY;

/**
 * @brief Icon types for display
 */
enum class IconType {
    NONE = 0,           // No icon (for clearing)
    ALARM_INFO,         // Green Checkmark (Info/Good)
    ALARM_WARNING,      // Orange Triangle (Warning)
    ALARM_CRITICAL      // Red Cross (Critical/Fail)
};

/**
 * @brief Current operational mode of the Matrix
 */
enum class MatrixMode {
    OFF = 0,
    PASSIVE = 1,      // Background status (Heartbeat, WiFi)
    ACTIVE_ICON = 2,  // User/Alarm Icon being shown
    ACTIVE_TEXT = 3,  // User/Alarm Text being shown
    ACTIVE_SOLID = 4, // User/Alarm Solid Color being shown
    ACTIVE_EFFECT = 5, // WS2812FX Effect running
    ACTIVE_DATA_VISUALIZATION = 6
};
