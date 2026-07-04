#include "MatrixFxEngine3D.h"

#include <algorithm>
#include <cmath>

namespace MATRIX_FX {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kMinDtSec = 0.016f;
constexpr float kMaxDtSec = 0.120f;

float clampf(float value, float lo, float hi) {
    return std::min(hi, std::max(lo, value));
}

float fract(float value) {
    return value - std::floor(value);
}

uint8_t channel(uint32_t color, uint8_t shift) {
    return static_cast<uint8_t>((color >> shift) & 0xFF);
}

uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8) |
           static_cast<uint32_t>(b);
}

}  // namespace

MatrixFxEngine3D::MatrixFxEngine3D() {
    initParticles();
}

void MatrixFxEngine3D::configure(const MatrixFxConfig& config) {
    _config = config;
    _config.mode = normalizeNative3DMode(config.mode);
    _config.provider = static_cast<ReactiveProvider>(
        normalizeReactiveProvider(static_cast<uint8_t>(config.provider)));
    _config.reactivityGain = clampReactivityGain(config.reactivityGain);
    _configured = true;
    _lastRenderMs = 0;
    _lastDeltaSec = 0.031f;
    _phase = 0.0f;
    _tiltX = 0.0f;
    _tiltY = 0.0f;
    initParticles();
}

void MatrixFxEngine3D::setInput(const MatrixFxInput& input) {
    _input = input;
}

void MatrixFxEngine3D::reset() {
    _configured = false;
    _lastRenderMs = 0;
    _lastDeltaSec = 0.031f;
    _phase = 0.0f;
    _tiltX = 0.0f;
    _tiltY = 0.0f;
    initParticles();
}

bool MatrixFxEngine3D::render(uint32_t nowMs, uint32_t* outFrame, uint8_t pixelCount) {
    if (!_configured || !outFrame || pixelCount < kMatrixFxPixelCount) {
        return false;
    }

    const uint32_t deltaMs = _lastRenderMs == 0 ? 31 : nowMs - _lastRenderMs;
    _lastRenderMs = nowMs;
    const float dtSec = clampf(static_cast<float>(deltaMs) / 1000.0f, kMinDtSec, kMaxDtSec);
    _lastDeltaSec = dtSec;
    const float speedScale = clampf(1000.0f / static_cast<float>(std::max<uint32_t>(_config.speedMs, 50)), 0.08f, 20.0f);
    _phase = fract(_phase + dtSec * speedScale * 0.18f);
    updateInputSmoothing(dtSec);

    switch (static_cast<Native3DMode>(_config.mode)) {
        case Native3DMode::GyroCube:
            renderIridescentRipple(outFrame);
            break;
        case Native3DMode::GravityParticles:
            renderGravityParticles(outFrame);
            break;
        case Native3DMode::DepthTunnel:
            renderDepthTunnel(outFrame);
            break;
        case Native3DMode::LiquidWave:
            renderLiquidWave(outFrame);
            break;
        case Native3DMode::OilSheen:
            renderOilSheen(outFrame);
            break;
        case Native3DMode::AuroraCurtain:
            renderAuroraCurtain(outFrame);
            break;
        case Native3DMode::PlasmaCloud:
            renderPlasmaCloud(outFrame);
            break;
        case Native3DMode::OrbitalRings:
            renderOrbitalRings(outFrame);
            break;
        case Native3DMode::TiltVortex:
            renderTiltVortex(outFrame);
            break;
        case Native3DMode::CometField:
            renderCometField(outFrame);
            break;
        case Native3DMode::LavaLamp:
            renderLavaLamp(outFrame);
            break;
        case Native3DMode::PrismSweep:
            renderPrismSweep(outFrame);
            break;
        case Native3DMode::RainGlass:
            renderRainGlass(outFrame);
            break;
        case Native3DMode::BreathingTerrain:
            renderBreathingTerrain(outFrame);
            break;
        case Native3DMode::PaletteWave:
        default:
            renderPaletteWave(outFrame);
            break;
    }
    return true;
}

void MatrixFxEngine3D::initParticles() {
    for (uint8_t i = 0; i < kParticleCount; ++i) {
        _particles[i].x = 0.5f + static_cast<float>((i * 5) % 7);
        _particles[i].y = 0.5f + static_cast<float>((i * 3) % 7);
        _particles[i].vx = (i % 2 == 0 ? 0.20f : -0.16f) + static_cast<float>(i) * 0.008f;
        _particles[i].vy = (i % 3 == 0 ? -0.18f : 0.14f) - static_cast<float>(i) * 0.006f;
        _particles[i].color = palette(static_cast<float>(i) / static_cast<float>(kParticleCount));
    }
    _particlesInitialized = true;
}

void MatrixFxEngine3D::updateInputSmoothing(float dtSec) {
    const bool useImu = _config.provider == ReactiveProvider::Imu && _input.imuValid;
    const float targetTiltX = useImu ? clampf(_input.ax, -1.0f, 1.0f) : 0.0f;
    const float targetTiltY = useImu ? clampf(_input.ay, -1.0f, 1.0f) : 0.0f;
    const float alpha = clampf(dtSec * 8.0f, 0.05f, 0.65f);
    _tiltX += (targetTiltX - _tiltX) * alpha;
    _tiltY += (targetTiltY - _tiltY) * alpha;
}

float MatrixFxEngine3D::reactiveGain() const {
    return static_cast<float>(_config.reactivityGain) / 100.0f;
}

void MatrixFxEngine3D::clearFrame(uint32_t* outFrame, uint32_t color) const {
    for (uint8_t i = 0; i < kMatrixFxPixelCount; ++i) {
        outFrame[i] = color;
    }
}

void MatrixFxEngine3D::fadeFrame(uint32_t* outFrame, uint8_t amount) const {
    const float scale = clampf(1.0f - static_cast<float>(amount) / 255.0f, 0.0f, 1.0f);
    for (uint8_t i = 0; i < kMatrixFxPixelCount; ++i) {
        outFrame[i] = scaleColor(outFrame[i], scale);
    }
}

uint32_t MatrixFxEngine3D::scaleColor(uint32_t color, float scale) const {
    scale = clampf(scale, 0.0f, 1.0f);
    return rgb(
        static_cast<uint8_t>(static_cast<float>(channel(color, 16)) * scale),
        static_cast<uint8_t>(static_cast<float>(channel(color, 8)) * scale),
        static_cast<uint8_t>(static_cast<float>(channel(color, 0)) * scale));
}

uint32_t MatrixFxEngine3D::hsvColor(float hue, float saturation, float value) const {
    hue = fract(hue);
    saturation = clampf(saturation, 0.0f, 1.0f);
    value = clampf(value, 0.0f, 1.0f);

    const float scaled = hue * 6.0f;
    const int sector = static_cast<int>(std::floor(scaled));
    const float part = scaled - static_cast<float>(sector);
    const float p = value * (1.0f - saturation);
    const float q = value * (1.0f - saturation * part);
    const float t = value * (1.0f - saturation * (1.0f - part));

    float r = value;
    float g = t;
    float b = p;
    switch (sector % 6) {
        case 0:
            r = value; g = t; b = p;
            break;
        case 1:
            r = q; g = value; b = p;
            break;
        case 2:
            r = p; g = value; b = t;
            break;
        case 3:
            r = p; g = q; b = value;
            break;
        case 4:
            r = t; g = p; b = value;
            break;
        default:
            r = value; g = p; b = q;
            break;
    }

    return rgb(
        static_cast<uint8_t>(r * 255.0f),
        static_cast<uint8_t>(g * 255.0f),
        static_cast<uint8_t>(b * 255.0f));
}

uint32_t MatrixFxEngine3D::blend(uint32_t base, uint32_t add, float alpha) const {
    alpha = clampf(alpha, 0.0f, 1.0f);
    const uint16_t r = std::min<uint16_t>(255, channel(base, 16) + static_cast<uint16_t>(channel(add, 16) * alpha));
    const uint16_t g = std::min<uint16_t>(255, channel(base, 8) + static_cast<uint16_t>(channel(add, 8) * alpha));
    const uint16_t b = std::min<uint16_t>(255, channel(base, 0) + static_cast<uint16_t>(channel(add, 0) * alpha));
    return rgb(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
}

uint32_t MatrixFxEngine3D::mixColor(uint32_t a, uint32_t b, float t) const {
    t = clampf(t, 0.0f, 1.0f);
    const float u = 1.0f - t;
    return rgb(
        static_cast<uint8_t>(channel(a, 16) * u + channel(b, 16) * t),
        static_cast<uint8_t>(channel(a, 8) * u + channel(b, 8) * t),
        static_cast<uint8_t>(channel(a, 0) * u + channel(b, 0) * t));
}

uint32_t MatrixFxEngine3D::palette(float t) const {
    t = fract(t);
    if (t < 0.5f) {
        return mixColor(_config.color1, _config.color2, t * 2.0f);
    }
    return mixColor(_config.color2, _config.color3, (t - 0.5f) * 2.0f);
}

uint32_t MatrixFxEngine3D::paletteColor(float t, float brightness) const {
    return scaleColor(palette(t), brightness);
}

float MatrixFxEngine3D::wave01(float value) const {
    return 0.5f + 0.5f * std::sin(value);
}

float MatrixFxEngine3D::softNoise(float x, float y, float z) const {
    const float a = std::sin(x * 1.37f + y * 0.91f + z * 1.13f);
    const float b = std::sin(x * 2.11f - y * 1.47f + z * 0.73f + 1.7f);
    const float c = std::cos(x * 0.79f + y * 2.03f - z * 1.41f + 0.4f);
    return clampf(0.5f + (a * 0.26f + b * 0.16f + c * 0.12f), 0.0f, 1.0f);
}

void MatrixFxEngine3D::addPixel(uint32_t* outFrame, int x, int y, uint32_t color, float alpha) const {
    if (x < 0 || x >= kMatrixFxWidth || y < 0 || y >= kMatrixFxHeight) {
        return;
    }
    const uint8_t index = static_cast<uint8_t>(y * kMatrixFxWidth + x);
    outFrame[index] = blend(outFrame[index], color, alpha);
}

void MatrixFxEngine3D::drawLine(uint32_t* outFrame, int x0, int y0, int x1, int y1, uint32_t color, float alpha) const {
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        addPixel(outFrame, x0, y0, color, alpha);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void MatrixFxEngine3D::drawRect(uint32_t* outFrame, int left, int top, int right, int bottom, uint32_t color, float alpha) const {
    if (left > right || top > bottom) {
        return;
    }
    drawLine(outFrame, left, top, right, top, color, alpha);
    drawLine(outFrame, right, top, right, bottom, color, alpha);
    drawLine(outFrame, right, bottom, left, bottom, color, alpha);
    drawLine(outFrame, left, bottom, left, top, color, alpha);
}

void MatrixFxEngine3D::renderIridescentRipple(uint32_t* outFrame) {
    const float gain = reactiveGain();
    const float centerX = 3.5f - _tiltX * 1.15f * gain;
    const float centerY = 3.5f - _tiltY * 1.15f * gain;
    const float tiltHue = (_tiltX * 0.12f + _tiltY * 0.08f) * gain;
    const float motionLift = clampf(_input.motionEnergy * 0.18f, 0.0f, 0.18f);

    for (uint8_t y = 0; y < kMatrixFxHeight; ++y) {
        for (uint8_t x = 0; x < kMatrixFxWidth; ++x) {
            const float dx = static_cast<float>(x) - centerX;
            const float dy = static_cast<float>(y) - centerY;
            const float dist = std::sqrt(dx * dx + dy * dy);
            const float expanding = 1.0f - std::abs(fract(dist * 0.62f - _phase * 2.05f) - 0.5f) * 2.0f;
            const float contracting = 1.0f - std::abs(fract((4.9f - dist) * 0.58f - _phase * 1.25f) - 0.5f) * 2.0f;
            const float ripple = clampf(expanding * expanding + contracting * contracting * 0.48f, 0.0f, 1.0f);
            const float centerGlow = clampf(1.0f - dist / 5.0f, 0.0f, 1.0f);
            const float oilSheen = std::sin((dx * _tiltY - dy * _tiltX) * 1.2f + _phase * 2.0f * kPi) * 0.035f;
            const float hue = _phase * 0.48f + dist * 0.055f + tiltHue + oilSheen;
            const float saturation = clampf(0.58f + ripple * 0.36f + centerGlow * 0.10f, 0.45f, 1.0f);
            const float value = clampf(0.08f + ripple * 0.76f + centerGlow * 0.18f + motionLift, 0.04f, 1.0f);
            outFrame[y * kMatrixFxWidth + x] = hsvColor(hue, saturation, value);
        }
    }
}

void MatrixFxEngine3D::renderGravityParticles(uint32_t* outFrame) {
    if (!_particlesInitialized) {
        initParticles();
    }

    fadeFrame(outFrame, 88);
    const uint32_t ambient = paletteColor(_phase * 0.10f, 0.025f);
    for (uint8_t i = 0; i < kMatrixFxPixelCount; ++i) {
        outFrame[i] = blend(outFrame[i], ambient, 0.35f);
    }

    const float dt = _lastDeltaSec;
    const float frameScale = dt * 20.0f;
    const float gain = reactiveGain();
    const float gravityX = -_tiltX * gain * 5.2f;
    const float gravityY = -_tiltY * gain * 5.2f;
    const float idleSpin = std::sin(_phase * 2.0f * kPi) * 0.08f;

    for (uint8_t i = 0; i < kParticleCount; ++i) {
        Particle& p = _particles[i];
        p.vx += (gravityX + idleSpin) * dt;
        p.vy += (gravityY - idleSpin) * dt;
        p.vx *= 0.90f;
        p.vy *= 0.90f;
        p.x += p.vx * frameScale;
        p.y += p.vy * frameScale;

        if (p.x < 0.0f) {
            p.x = 0.0f;
            p.vx = std::abs(p.vx) * 0.70f;
        } else if (p.x > 7.0f) {
            p.x = 7.0f;
            p.vx = -std::abs(p.vx) * 0.70f;
        }
        if (p.y < 0.0f) {
            p.y = 0.0f;
            p.vy = std::abs(p.vy) * 0.70f;
        } else if (p.y > 7.0f) {
            p.y = 7.0f;
            p.vy = -std::abs(p.vy) * 0.70f;
        }

        const float energy = clampf(std::sqrt(p.vx * p.vx + p.vy * p.vy) * 2.2f, 0.25f, 1.0f);
        const int left = static_cast<int>(std::floor(p.x));
        const int top = static_cast<int>(std::floor(p.y));
        const float fx = p.x - static_cast<float>(left);
        const float fy = p.y - static_cast<float>(top);

        for (uint8_t oy = 0; oy < 2; ++oy) {
            for (uint8_t ox = 0; ox < 2; ++ox) {
                const float wx = ox == 0 ? 1.0f - fx : fx;
                const float wy = oy == 0 ? 1.0f - fy : fy;
                addPixel(outFrame, left + ox, top + oy, p.color, energy * wx * wy * 0.95f);
            }
        }

        addPixel(outFrame, left - 1, top, p.color, energy * 0.10f);
        addPixel(outFrame, left + 2, top, p.color, energy * 0.10f);
        addPixel(outFrame, left, top - 1, p.color, energy * 0.10f);
        addPixel(outFrame, left, top + 2, p.color, energy * 0.10f);
    }
}

void MatrixFxEngine3D::renderDepthTunnel(uint32_t* outFrame) {
    const float gain = reactiveGain();
    const float centerX = 3.5f - _tiltX * 1.10f * gain;
    const float centerY = 3.5f - _tiltY * 1.10f * gain;

    for (uint8_t y = 0; y < kMatrixFxHeight; ++y) {
        for (uint8_t x = 0; x < kMatrixFxWidth; ++x) {
            const float dx = std::abs(static_cast<float>(x) - centerX);
            const float dy = std::abs(static_cast<float>(y) - centerY);
            const float squareRadius = std::max(dx, dy);
            const float diagonal = (dx + dy) * 0.11f;
            const float ring = 1.0f - std::abs(fract(squareRadius * 0.58f - _phase * 1.35f + diagonal) - 0.5f) * 2.0f;
            const float depth = clampf(squareRadius / 4.1f, 0.0f, 1.0f);
            const float brightness = clampf(0.08f + ring * ring * (0.30f + depth * 0.48f), 0.06f, 0.88f);
            outFrame[y * kMatrixFxWidth + x] = scaleColor(palette(depth * 0.65f + _phase * 0.12f), brightness);
        }
    }

    addPixel(outFrame, static_cast<int>(std::round(centerX)), static_cast<int>(std::round(centerY)), _config.color3, 0.65f);
}

void MatrixFxEngine3D::renderLiquidWave(uint32_t* outFrame) {
    const float gain = reactiveGain();
    const float waveTiltX = -_tiltX * 0.65f * gain;
    const float waveTiltY = -_tiltY * 0.65f * gain;

    for (uint8_t y = 0; y < kMatrixFxHeight; ++y) {
        for (uint8_t x = 0; x < kMatrixFxWidth; ++x) {
            const float nx = (static_cast<float>(x) - 3.5f) / 3.5f;
            const float ny = static_cast<float>(y) / 7.0f;
            const float perspective = 0.55f + ny * 1.65f;
            const float surfaceX = nx * perspective + waveTiltX;
            const float surfaceY = ny + waveTiltY * 0.35f;
            const float wave =
                std::sin(surfaceX * 3.1f + _phase * 2.0f * kPi + surfaceY * 1.6f) * (0.18f + ny * 0.26f);
            const float band = 1.0f - std::abs(fract((surfaceY + wave) * 4.0f - _phase * 1.15f) - 0.5f) * 2.0f;
            const float depthLight = 0.16f + ny * 0.38f;
            const float bright = clampf(depthLight + band * (0.28f + ny * 0.34f), 0.05f, 1.0f);
            outFrame[y * kMatrixFxWidth + x] = scaleColor(palette(0.48f + surfaceY * 0.28f + wave * 0.25f), bright);
        }
    }
}

void MatrixFxEngine3D::renderOilSheen(uint32_t* outFrame) {
    const float gain = reactiveGain();
    const float tiltFlow = (_tiltX * 0.22f - _tiltY * 0.17f) * gain;
    const float driftX = _tiltX * 0.42f * gain;
    const float driftY = _tiltY * 0.42f * gain;

    for (uint8_t y = 0; y < kMatrixFxHeight; ++y) {
        for (uint8_t x = 0; x < kMatrixFxWidth; ++x) {
            const float nx = (static_cast<float>(x) - 3.5f) / 3.5f;
            const float ny = (static_cast<float>(y) - 3.5f) / 3.5f;
            const float film =
                std::sin((nx + driftX) * 5.2f + _phase * 2.0f * kPi) * 0.42f +
                std::sin((ny - driftY) * 4.3f - _phase * 1.45f * kPi) * 0.32f +
                std::sin((nx * ny + tiltFlow) * 7.0f + _phase * 1.1f * kPi) * 0.26f;
            const float edgeGlow = clampf(1.0f - std::sqrt(nx * nx + ny * ny) * 0.72f, 0.0f, 1.0f);
            const float bright = clampf(0.11f + std::abs(film) * 0.48f + edgeGlow * 0.24f, 0.05f, 0.92f);
            outFrame[y * kMatrixFxWidth + x] = paletteColor(0.50f + film * 0.34f + tiltFlow, bright);
        }
    }
}

void MatrixFxEngine3D::renderAuroraCurtain(uint32_t* outFrame) {
    const float gain = reactiveGain();
    const float wind = _tiltX * 0.95f * gain;
    const float lift = _tiltY * 0.35f * gain;

    for (uint8_t y = 0; y < kMatrixFxHeight; ++y) {
        for (uint8_t x = 0; x < kMatrixFxWidth; ++x) {
            const float nx = static_cast<float>(x) / 7.0f;
            const float ny = static_cast<float>(y) / 7.0f;
            const float foldA = wave01((nx * 3.8f + wind) * kPi + _phase * 2.2f * kPi);
            const float foldB = wave01((nx * 7.1f - ny * 1.7f - wind * 0.6f) * kPi - _phase * 1.3f * kPi);
            const float veil = clampf(foldA * foldA * 0.72f + foldB * 0.28f, 0.0f, 1.0f);
            const float verticalFade = clampf(1.05f - ny * 0.92f + lift * 0.18f, 0.10f, 1.0f);
            const float bright = clampf(0.04f + veil * veil * verticalFade * 0.86f, 0.02f, 0.96f);
            outFrame[y * kMatrixFxWidth + x] = paletteColor(nx * 0.52f + veil * 0.22f + _phase * 0.08f + wind * 0.10f, bright);
        }
    }
}

void MatrixFxEngine3D::renderPlasmaCloud(uint32_t* outFrame) {
    const float gain = reactiveGain();
    const float z = _phase * 4.2f;
    const float driftX = _tiltX * 0.85f * gain;
    const float driftY = _tiltY * 0.85f * gain;

    for (uint8_t y = 0; y < kMatrixFxHeight; ++y) {
        for (uint8_t x = 0; x < kMatrixFxWidth; ++x) {
            const float nx = (static_cast<float>(x) - 3.5f) / 3.5f;
            const float ny = (static_cast<float>(y) - 3.5f) / 3.5f;
            const float n1 = softNoise(nx * 2.2f + driftX, ny * 2.2f - driftY, z);
            const float n2 = softNoise(nx * 4.0f - driftY, ny * 3.5f + driftX, z * 0.62f + 1.7f);
            const float plasma = clampf(n1 * 0.68f + n2 * 0.32f, 0.0f, 1.0f);
            const float bright = clampf(0.07f + plasma * plasma * 0.84f, 0.04f, 0.95f);
            outFrame[y * kMatrixFxWidth + x] = paletteColor(plasma * 0.82f + _phase * 0.10f, bright);
        }
    }
}

void MatrixFxEngine3D::renderOrbitalRings(uint32_t* outFrame) {
    const float gain = reactiveGain();
    const float centerX = 3.5f - _tiltX * 1.35f * gain;
    const float centerY = 3.5f - _tiltY * 1.35f * gain;
    const float tiltSpin = (_tiltX - _tiltY) * 0.55f * gain;

    for (uint8_t y = 0; y < kMatrixFxHeight; ++y) {
        for (uint8_t x = 0; x < kMatrixFxWidth; ++x) {
            const float dx = static_cast<float>(x) - centerX;
            const float dy = static_cast<float>(y) - centerY;
            const float dist = std::sqrt(dx * dx + dy * dy);
            const float angle = std::atan2(dy, dx);
            const float orbit = wave01(dist * 4.35f - _phase * 3.2f * kPi);
            const float spokes = wave01(angle * 3.0f + _phase * 2.0f * kPi + tiltSpin);
            const float glow = clampf(1.0f - dist / 5.3f, 0.0f, 1.0f);
            const float bright = clampf(0.05f + orbit * orbit * 0.52f + spokes * glow * 0.26f, 0.03f, 0.94f);
            outFrame[y * kMatrixFxWidth + x] = paletteColor(dist * 0.13f + angle / (2.0f * kPi) * 0.18f + _phase * 0.16f, bright);
        }
    }
}

void MatrixFxEngine3D::renderTiltVortex(uint32_t* outFrame) {
    const float gain = reactiveGain();
    const float centerX = 3.5f - _tiltX * 1.0f * gain;
    const float centerY = 3.5f - _tiltY * 1.0f * gain;
    const float twist = (_tiltX + _tiltY) * 1.4f * gain;

    for (uint8_t y = 0; y < kMatrixFxHeight; ++y) {
        for (uint8_t x = 0; x < kMatrixFxWidth; ++x) {
            const float dx = static_cast<float>(x) - centerX;
            const float dy = static_cast<float>(y) - centerY;
            const float dist = std::sqrt(dx * dx + dy * dy);
            const float angle = std::atan2(dy, dx);
            const float vortex = wave01(angle * 2.7f + dist * 3.1f - _phase * 3.6f * kPi + twist);
            const float core = clampf(1.0f - dist / 5.0f, 0.0f, 1.0f);
            const float bright = clampf(0.05f + vortex * vortex * (0.28f + core * 0.56f), 0.03f, 0.98f);
            outFrame[y * kMatrixFxWidth + x] = paletteColor(angle / (2.0f * kPi) + dist * 0.09f + _phase * 0.18f + twist * 0.04f, bright);
        }
    }
}

void MatrixFxEngine3D::renderCometField(uint32_t* outFrame) {
    clearFrame(outFrame, paletteColor(_phase * 0.18f, 0.035f));
    const float gain = reactiveGain();

    for (uint8_t comet = 0; comet < 4; ++comet) {
        const float angle = 0.42f + static_cast<float>(comet) * 1.37f + _tiltX * 0.75f * gain;
        const float progress = fract(_phase * (0.80f + static_cast<float>(comet) * 0.09f) +
                                     static_cast<float>(comet) * 0.23f);
        const float path = progress * 10.0f - 1.5f;
        const float vx = std::cos(angle);
        const float vy = std::sin(angle);
        const float px = 3.5f + vx * (path - 4.0f) + _tiltX * 1.15f * gain;
        const float py = 3.5f + vy * (path - 4.0f) + _tiltY * 1.15f * gain;
        const uint32_t color = palette(static_cast<float>(comet) * 0.24f + _phase * 0.18f);

        for (uint8_t y = 0; y < kMatrixFxHeight; ++y) {
            for (uint8_t x = 0; x < kMatrixFxWidth; ++x) {
                const float dx = static_cast<float>(x) - px;
                const float dy = static_cast<float>(y) - py;
                const float along = dx * vx + dy * vy;
                const float cross = std::abs(dx * vy - dy * vx);
                const float head = clampf(1.0f - std::sqrt(dx * dx + dy * dy) / 1.25f, 0.0f, 1.0f);
                const float tail = clampf(-along / 3.7f, 0.0f, 1.0f) * clampf(1.0f - cross / 1.15f, 0.0f, 1.0f);
                addPixel(outFrame, x, y, color, clampf(head * 0.95f + tail * 0.52f, 0.0f, 1.0f));
            }
        }
    }
}

void MatrixFxEngine3D::renderLavaLamp(uint32_t* outFrame) {
    const float gain = reactiveGain();

    for (uint8_t y = 0; y < kMatrixFxHeight; ++y) {
        for (uint8_t x = 0; x < kMatrixFxWidth; ++x) {
            const float nx = (static_cast<float>(x) - 3.5f) / 3.5f;
            const float ny = (static_cast<float>(y) - 3.5f) / 3.5f;
            float field = 0.0f;
            for (uint8_t blob = 0; blob < 4; ++blob) {
                const float phase = _phase * (1.0f + static_cast<float>(blob) * 0.17f) +
                                    static_cast<float>(blob) * 0.21f;
                const float bx = std::sin(phase * 2.0f * kPi + static_cast<float>(blob)) * 0.52f +
                                 _tiltX * 0.36f * gain;
                const float by = std::cos(phase * 1.55f * kPi + static_cast<float>(blob) * 1.3f) * 0.58f +
                                 _tiltY * 0.36f * gain;
                const float dx = nx - bx;
                const float dy = ny - by;
                field += 0.18f / (0.08f + dx * dx + dy * dy);
            }
            const float lava = clampf(field * 0.33f, 0.0f, 1.0f);
            const float bright = clampf(0.05f + lava * 0.88f, 0.03f, 0.96f);
            outFrame[y * kMatrixFxWidth + x] = paletteColor(lava * 0.76f + ny * 0.10f + _phase * 0.08f, bright);
        }
    }
}

void MatrixFxEngine3D::renderPrismSweep(uint32_t* outFrame) {
    const float gain = reactiveGain();
    const float angle = _phase * 2.0f * kPi + (_tiltX - _tiltY) * 1.2f * gain;
    const float vx = std::cos(angle);
    const float vy = std::sin(angle);

    for (uint8_t y = 0; y < kMatrixFxHeight; ++y) {
        for (uint8_t x = 0; x < kMatrixFxWidth; ++x) {
            const float nx = (static_cast<float>(x) - 3.5f) / 3.5f;
            const float ny = (static_cast<float>(y) - 3.5f) / 3.5f;
            const float plane = nx * vx + ny * vy;
            const float band = 1.0f - std::abs(fract(plane * 2.6f - _phase * 1.45f) - 0.5f) * 2.0f;
            const float split = 1.0f - std::abs(fract(plane * 5.2f + _phase * 0.7f) - 0.5f) * 2.0f;
            const float bright = clampf(0.06f + band * band * 0.64f + split * split * 0.18f, 0.03f, 0.96f);
            outFrame[y * kMatrixFxWidth + x] = paletteColor(0.50f + plane * 0.35f + band * 0.18f, bright);
        }
    }
}

void MatrixFxEngine3D::renderRainGlass(uint32_t* outFrame) {
    const float gain = reactiveGain();
    float gx = _tiltX * gain;
    float gy = _tiltY * gain + 0.45f;
    const float len = std::sqrt(gx * gx + gy * gy);
    if (len > 0.001f) {
        gx /= len;
        gy /= len;
    }

    for (uint8_t y = 0; y < kMatrixFxHeight; ++y) {
        for (uint8_t x = 0; x < kMatrixFxWidth; ++x) {
            const float nx = (static_cast<float>(x) - 3.5f) / 3.5f;
            const float ny = (static_cast<float>(y) - 3.5f) / 3.5f;
            const float along = nx * gx + ny * gy;
            const float cross = -nx * gy + ny * gx;
            const float lane = wave01(cross * 18.0f + std::sin(along * 5.0f + _phase * 2.0f * kPi));
            const float drop = 1.0f - std::abs(fract(along * 3.6f - _phase * 2.2f + lane * 0.25f) - 0.5f) * 2.0f;
            const float streak = drop * drop * drop * lane;
            const float mist = softNoise(nx * 2.0f, ny * 2.0f, _phase * 2.0f) * 0.12f;
            const float bright = clampf(0.04f + streak * 0.82f + mist, 0.02f, 0.92f);
            outFrame[y * kMatrixFxWidth + x] = paletteColor(0.18f + along * 0.22f + lane * 0.35f + _phase * 0.08f, bright);
        }
    }
}

void MatrixFxEngine3D::renderBreathingTerrain(uint32_t* outFrame) {
    const float gain = reactiveGain();
    const float breath = wave01(_phase * 2.0f * kPi);
    const float lightX = clampf(_tiltX * gain, -1.0f, 1.0f);
    const float lightY = clampf(_tiltY * gain, -1.0f, 1.0f);

    for (uint8_t y = 0; y < kMatrixFxHeight; ++y) {
        for (uint8_t x = 0; x < kMatrixFxWidth; ++x) {
            const float nx = (static_cast<float>(x) - 3.5f) / 3.5f;
            const float ny = (static_cast<float>(y) - 3.5f) / 3.5f;
            const float height = softNoise(nx * 2.4f + _phase * 1.1f + lightX * 0.6f,
                                           ny * 2.4f - _phase * 0.9f + lightY * 0.6f,
                                           _phase * 2.1f);
            const float slopeLight = clampf((nx * lightX + ny * lightY) * 0.20f + 0.20f, 0.0f, 0.40f);
            const float bright = clampf(0.05f + height * 0.58f + breath * 0.20f + slopeLight, 0.04f, 0.98f);
            outFrame[y * kMatrixFxWidth + x] = paletteColor(height * 0.82f + _phase * 0.08f, bright);
        }
    }
}

void MatrixFxEngine3D::renderPaletteWave(uint32_t* outFrame) {
    const float gain = reactiveGain();
    const float waveTiltX = -_tiltX * 1.25f * gain;
    const float waveTiltY = -_tiltY * 1.05f * gain;
    const float tiltShift = (_tiltX * 0.16f + _tiltY * 0.11f) * gain;

    for (uint8_t y = 0; y < kMatrixFxHeight; ++y) {
        for (uint8_t x = 0; x < kMatrixFxWidth; ++x) {
            const float nx = (static_cast<float>(x) - 3.5f) / 3.5f;
            const float ny = static_cast<float>(y) / 7.0f;
            const float perspective = 0.55f + ny * 1.65f;
            const float surfaceX = nx * perspective + waveTiltX;
            const float surfaceY = ny + waveTiltY * 0.35f;
            const float tiltSlope = (nx * _tiltX + (ny - 0.5f) * _tiltY) * gain;
            const float wave =
                std::sin(surfaceX * 3.1f + _phase * 2.0f * kPi + surfaceY * 1.6f + tiltSlope * 1.4f) *
                (0.18f + ny * 0.26f);
            const float band = 1.0f - std::abs(fract((surfaceY + wave) * 4.0f - _phase * 1.15f) - 0.5f) * 2.0f;
            const float depthLight = 0.16f + ny * 0.38f;
            const float bright = clampf(depthLight + band * (0.28f + ny * 0.34f) + std::abs(tiltSlope) * 0.10f, 0.05f, 1.0f);
            outFrame[y * kMatrixFxWidth + x] = paletteColor(0.08f + surfaceY * 0.54f + wave * 0.24f + tiltShift, bright);
        }
    }
}

}  // namespace MATRIX_FX
