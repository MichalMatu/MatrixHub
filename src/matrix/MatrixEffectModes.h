#pragma once

#include <cstdint>

#include "../config/UiConfig.h"
#include "../../lib/matrix_service/effects/MatrixFxTypes.h"

namespace MATRIX {

constexpr uint8_t kMatrixEffectModeMax = 69;

// Matrix effects now use one compact range shared by the UI, config layer and
// renderer, so validation can stay a simple upper-bound check.
constexpr bool isSupportedMatrixEffectMode(uint8_t mode) {
    return mode <= kMatrixEffectModeMax;
}

// Normalize any out-of-range value back to the safe default so config loads,
// API writes and renderer calls all converge on the same behaviour.
constexpr uint8_t normalizeMatrixEffectMode(uint8_t mode) {
    return isSupportedMatrixEffectMode(mode) ? mode : UI::MATRIX::DEFAULT_EFFECT_MODE;
}

constexpr uint8_t normalizeMatrixEffectEngine(uint8_t engine) {
    return MATRIX_FX::normalizeEffectEngine(engine);
}

constexpr uint8_t normalizeMatrixEffectModeForEngine(uint8_t mode, uint8_t engine) {
    return normalizeMatrixEffectEngine(engine) == static_cast<uint8_t>(MATRIX_FX::EffectEngine::Native3D)
        ? MATRIX_FX::normalizeNative3DMode(mode)
        : normalizeMatrixEffectMode(mode);
}

constexpr uint8_t normalizeMatrixEffectReactivityProvider(uint8_t provider) {
    return MATRIX_FX::normalizeReactiveProvider(provider);
}

constexpr uint8_t normalizeMatrixEffectReactivityGain(uint8_t gain) {
    return MATRIX_FX::clampReactivityGain(gain);
}

}  // namespace MATRIX
