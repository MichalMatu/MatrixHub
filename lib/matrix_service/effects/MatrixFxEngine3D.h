#pragma once

#include "MatrixFxTypes.h"

namespace MATRIX_FX {

class MatrixFxEngine3D {
public:
    MatrixFxEngine3D();

    void configure(const MatrixFxConfig& config);
    void setInput(const MatrixFxInput& input);
    bool render(uint32_t nowMs, uint32_t* outFrame, uint8_t pixelCount);
    void reset();

    bool isConfigured() const { return _configured; }

private:
    struct Particle {
        float x;
        float y;
        float vx;
        float vy;
        uint32_t color;
    };

    static constexpr uint8_t kParticleCount = 10;

    MatrixFxConfig _config{};
    MatrixFxInput _input{};
    Particle _particles[kParticleCount]{};
    bool _configured = false;
    bool _particlesInitialized = false;
    uint32_t _lastRenderMs = 0;
    float _lastDeltaSec = 0.031f;
    float _phase = 0.0f;
    float _tiltX = 0.0f;
    float _tiltY = 0.0f;

    void initParticles();
    void renderIridescentRipple(uint32_t* outFrame);
    void renderGravityParticles(uint32_t* outFrame);
    void renderDepthTunnel(uint32_t* outFrame);
    void renderLiquidWave(uint32_t* outFrame);
    void renderOilSheen(uint32_t* outFrame);
    void renderAuroraCurtain(uint32_t* outFrame);
    void renderPlasmaCloud(uint32_t* outFrame);
    void renderOrbitalRings(uint32_t* outFrame);
    void renderTiltVortex(uint32_t* outFrame);
    void renderCometField(uint32_t* outFrame);
    void renderLavaLamp(uint32_t* outFrame);
    void renderPrismSweep(uint32_t* outFrame);
    void renderRainGlass(uint32_t* outFrame);
    void renderBreathingTerrain(uint32_t* outFrame);
    void renderPaletteWave(uint32_t* outFrame);

    void clearFrame(uint32_t* outFrame, uint32_t color = 0) const;
    void fadeFrame(uint32_t* outFrame, uint8_t amount) const;
    void addPixel(uint32_t* outFrame, int x, int y, uint32_t color, float alpha) const;
    void drawLine(uint32_t* outFrame, int x0, int y0, int x1, int y1, uint32_t color, float alpha) const;
    void drawRect(uint32_t* outFrame, int left, int top, int right, int bottom, uint32_t color, float alpha) const;
    uint32_t blend(uint32_t base, uint32_t add, float alpha) const;
    uint32_t scaleColor(uint32_t color, float scale) const;
    uint32_t hsvColor(float hue, float saturation, float value) const;
    uint32_t mixColor(uint32_t a, uint32_t b, float t) const;
    uint32_t palette(float t) const;
    uint32_t paletteColor(float t, float brightness) const;
    float wave01(float value) const;
    float softNoise(float x, float y, float z) const;
    float reactiveGain() const;
    void updateInputSmoothing(float dtSec);
};

}  // namespace MATRIX_FX
