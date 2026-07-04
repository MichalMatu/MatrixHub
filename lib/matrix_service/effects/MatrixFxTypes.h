#pragma once

#include <cstdint>

namespace MATRIX_FX {

constexpr uint8_t kMatrixFxWidth = 8;
constexpr uint8_t kMatrixFxHeight = 8;
constexpr uint8_t kMatrixFxPixelCount = kMatrixFxWidth * kMatrixFxHeight;

enum class EffectEngine : uint8_t {
    LegacyWs2812Fx = 0,
    Native3D = 1
};

enum class ReactiveProvider : uint8_t {
    None = 0,
    Imu = 1
};

enum class Native3DMode : uint8_t {
    CenterRipple = 0,
    GyroCube = CenterRipple,
    GravityParticles = 1,
    DepthTunnel = 2,
    LiquidWave = 3,
    OilSheen = 4,
    AuroraCurtain = 5,
    PlasmaCloud = 6,
    OrbitalRings = 7,
    TiltVortex = 8,
    CometField = 9,
    LavaLamp = 10,
    PrismSweep = 11,
    RainGlass = 12,
    BreathingTerrain = 13,
    PaletteWave = 14
};

constexpr uint8_t kEffectEngineMax = static_cast<uint8_t>(EffectEngine::Native3D);
constexpr uint8_t kReactiveProviderMax = static_cast<uint8_t>(ReactiveProvider::Imu);
constexpr uint8_t kNative3DModeMax = static_cast<uint8_t>(Native3DMode::PaletteWave);
constexpr uint8_t kDefaultReactivityGain = 80;
constexpr uint8_t kMaxReactivityGain = 200;

struct MatrixFxInput {
    bool imuValid = false;
    float ax = 0.0f;
    float ay = 0.0f;
    float az = 1.0f;
    float gx = 0.0f;
    float gy = 0.0f;
    float gz = 0.0f;
    float motionEnergy = 0.0f;
    uint32_t timestampMs = 0;
};

struct MatrixFxConfig {
    uint8_t mode = static_cast<uint8_t>(Native3DMode::CenterRipple);
    uint32_t speedMs = 1000;
    uint32_t color1 = 0x00FF00;
    uint32_t color2 = 0xFF0000;
    uint32_t color3 = 0x0000FF;
    ReactiveProvider provider = ReactiveProvider::None;
    uint8_t reactivityGain = kDefaultReactivityGain;
};

constexpr uint8_t normalizeEffectEngine(uint8_t value) {
    return value <= kEffectEngineMax
        ? value
        : static_cast<uint8_t>(EffectEngine::LegacyWs2812Fx);
}

constexpr uint8_t normalizeReactiveProvider(uint8_t value) {
    return value <= kReactiveProviderMax
        ? value
        : static_cast<uint8_t>(ReactiveProvider::None);
}

constexpr uint8_t normalizeNative3DMode(uint8_t value) {
    return value <= kNative3DModeMax
        ? value
        : static_cast<uint8_t>(Native3DMode::CenterRipple);
}

constexpr uint8_t clampReactivityGain(uint8_t value) {
    return value > kMaxReactivityGain ? kMaxReactivityGain : value;
}

}  // namespace MATRIX_FX
