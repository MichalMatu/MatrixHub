#pragma once

#include <cstddef>
#include <cstdint>

namespace MATRIX {

constexpr size_t kMatrixCustomIconCount = 3;
constexpr size_t kMatrixCustomIconPixels = 64;
constexpr uint32_t kMatrixRgbColorMask = 0x00FFFFFF;

constexpr uint32_t normalizeMatrixColor(uint32_t color) {
    return color & kMatrixRgbColorMask;
}

bool hasCustomIcon(size_t index);
bool copyCustomIcon(size_t index, uint32_t* outBuffer, size_t pixelCount = kMatrixCustomIconPixels);
bool setCustomIcon(size_t index, const uint32_t* bitmap, size_t pixelCount = kMatrixCustomIconPixels);
bool clearCustomIcon(size_t index);
void clearAllCustomIcons();
bool customIconEquals(size_t index, const uint32_t* bitmap, size_t pixelCount = kMatrixCustomIconPixels);

}  // namespace MATRIX
