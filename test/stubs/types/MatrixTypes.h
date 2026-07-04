#pragma once

#include "../../../src/config/UiConfig.h"
#include <cstddef>
#include <cstdint>

inline constexpr size_t kMatrixTextCapacity = UI::MATRIX::TEXT_CAPACITY;
inline constexpr size_t kMatrixTextPaddingChars = UI::MATRIX::TEXT_PADDING_CHARS;
inline constexpr size_t kMatrixRenderedTextCapacity =
    UI::MATRIX::RENDERED_TEXT_CAPACITY;

enum class IconType {
    NONE = 0,
    ALARM_INFO,
    ALARM_WARNING,
    ALARM_CRITICAL
};

enum class MatrixMode {
    OFF = 0,
    PASSIVE = 1,
    ACTIVE_ICON = 2,
    ACTIVE_TEXT = 3,
    ACTIVE_SOLID = 4,
    ACTIVE_EFFECT = 5,
    ACTIVE_DATA_VISUALIZATION = 6
};
