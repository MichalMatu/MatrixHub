#include "MatrixDataVisualizationEngine.h"

#include <cmath>
#include <cstring>

namespace MATRIX_VIZ {

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr uint8_t kWidth = MATRIX::kMatrixDataVizWidth;
constexpr uint8_t kHeight = MATRIX::kMatrixDataVizHeight;
constexpr uint8_t kPixels = MATRIX::kMatrixDataVizPixelCount;
}  // namespace

void MatrixDataVisualizationEngine::configure(const MATRIX::MatrixDataVisualizationConfig& config) {
    _config = config;
    MATRIX::normalizeMatrixDataVisualizationConfig(_config);
    _configured = true;
    _hasSmoothed = false;
    _lastInputTimestamp = 0;
    _trendHead = 0;
    _trendFilled = false;
    std::memset(_trend, 0, sizeof(_trend));
}

void MatrixDataVisualizationEngine::setInput(const MATRIX::MatrixDataVisualizationInput& input) {
    _input = input;
    if (_input.binCount > kPixels) {
        _input.binCount = kPixels;
    }
}

void MatrixDataVisualizationEngine::reset() {
    _configured = false;
    _hasSmoothed = false;
    _smoothedValue = 0.0f;
    _lastInputTimestamp = 0;
    _trendHead = 0;
    _trendFilled = false;
    std::memset(_trend, 0, sizeof(_trend));
    _input = MATRIX::MatrixDataVisualizationInput{};
}

uint8_t MatrixDataVisualizationEngine::normalizeValue(float value, float minValue, float maxValue) {
    if (!std::isfinite(value)) {
        value = minValue;
    }
    if (maxValue <= minValue) {
        maxValue = minValue + 1.0f;
    }
    const float scaled = (value - minValue) / (maxValue - minValue);
    return clampByte(static_cast<int>(std::lround(clampFloat(scaled, 0.0f, 1.0f) * 255.0f)));
}

uint32_t MatrixDataVisualizationEngine::gradientColor(
    uint32_t minColor,
    uint32_t midColor,
    uint32_t maxColor,
    uint8_t value) {
    const uint32_t a = value <= 127 ? minColor : midColor;
    const uint32_t b = value <= 127 ? midColor : maxColor;
    const uint16_t t = value <= 127
        ? static_cast<uint16_t>(value) * 2u
        : static_cast<uint16_t>(value - 128u) * 2u;
    const uint16_t inv = 255u - t;
    return rgb(
        static_cast<uint8_t>((channel(a, 16) * inv + channel(b, 16) * t) / 255u),
        static_cast<uint8_t>((channel(a, 8) * inv + channel(b, 8) * t) / 255u),
        static_cast<uint8_t>((channel(a, 0) * inv + channel(b, 0) * t) / 255u));
}

uint32_t MatrixDataVisualizationEngine::scaleColor(uint32_t color, uint8_t brightness) {
    return rgb(
        static_cast<uint8_t>((static_cast<uint16_t>(channel(color, 16)) * brightness) / 255u),
        static_cast<uint8_t>((static_cast<uint16_t>(channel(color, 8)) * brightness) / 255u),
        static_cast<uint8_t>((static_cast<uint16_t>(channel(color, 0)) * brightness) / 255u));
}

bool MatrixDataVisualizationEngine::render(uint32_t nowMs, uint32_t* outFrame, size_t pixelCount) {
    if (!_configured || !outFrame || pixelCount < kPixels || !_config.enabled) {
        return false;
    }

    if (shouldBlank()) {
        clear(outFrame);
        return true;
    }

    const bool stale = !_input.valid || _input.stale;
    if (stale && _config.staleBehavior == static_cast<uint8_t>(MATRIX::MatrixDataStaleBehavior::Gray)) {
        fillStaleGray(outFrame);
        return true;
    }

    const uint8_t normalized = currentValueNorm();
    const uint8_t brightness = effectiveBrightness(normalized, stale);

    if (_input.valid && _input.timestampMs != 0 && _input.timestampMs != _lastInputTimestamp) {
        pushTrend(normalized);
        _lastInputTimestamp = _input.timestampMs;
    }

    clear(outFrame);
    switch (static_cast<MATRIX::MatrixDataVizMode>(_config.mode)) {
        case MATRIX::MatrixDataVizMode::CenterRipple:
            renderCenterRipple(nowMs, outFrame, normalized, brightness);
            break;
        case MATRIX::MatrixDataVizMode::Heatmap:
            renderHeatmap(outFrame, normalized, brightness, stale);
            break;
        case MATRIX::MatrixDataVizMode::Trend:
            renderTrend(outFrame, normalized, brightness);
            break;
        case MATRIX::MatrixDataVizMode::SpectrumBars:
            renderSpectrumBars(nowMs, outFrame, normalized, brightness);
            break;
        case MATRIX::MatrixDataVizMode::PerimeterMeter:
            renderPerimeterMeter(nowMs, outFrame, normalized, brightness);
            break;
        case MATRIX::MatrixDataVizMode::Pulse:
            renderPulse(nowMs, outFrame, normalized, brightness);
            break;
        case MATRIX::MatrixDataVizMode::Gauge:
        default:
            renderGauge(outFrame, normalized, brightness);
            break;
    }
    return true;
}

float MatrixDataVisualizationEngine::clampFloat(float value, float minValue, float maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

uint8_t MatrixDataVisualizationEngine::clampByte(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return static_cast<uint8_t>(value);
}

uint8_t MatrixDataVisualizationEngine::channel(uint32_t color, uint8_t shift) {
    return static_cast<uint8_t>((color >> shift) & 0xFFu);
}

uint32_t MatrixDataVisualizationEngine::rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8) |
           static_cast<uint32_t>(b);
}

uint8_t MatrixDataVisualizationEngine::xy(uint8_t x, uint8_t y) {
    return static_cast<uint8_t>(y * kWidth + x);
}

uint8_t MatrixDataVisualizationEngine::perimeterX(uint8_t index) {
    index = static_cast<uint8_t>(index % 28u);
    if (index < 8u) {
        return index;
    }
    if (index < 15u) {
        return 7u;
    }
    if (index < 22u) {
        return static_cast<uint8_t>(21u - index);
    }
    return 0u;
}

uint8_t MatrixDataVisualizationEngine::perimeterY(uint8_t index) {
    index = static_cast<uint8_t>(index % 28u);
    if (index < 8u) {
        return 7u;
    }
    if (index < 15u) {
        return static_cast<uint8_t>(14u - index);
    }
    if (index < 22u) {
        return 0u;
    }
    return static_cast<uint8_t>(index - 21u);
}

uint8_t MatrixDataVisualizationEngine::currentValueNorm() {
    const float raw = _input.valid ? _input.value : _config.minValue;
    if (!_hasSmoothed) {
        _smoothedValue = raw;
        _hasSmoothed = true;
    } else {
        const float alpha = clampFloat(static_cast<float>(100u - _config.smoothing) / 100.0f, 0.02f, 1.0f);
        _smoothedValue += (raw - _smoothedValue) * alpha;
    }
    return normalizeValue(_smoothedValue, _config.minValue, _config.maxValue);
}

uint8_t MatrixDataVisualizationEngine::effectiveBrightness(uint8_t normalized, bool stale) const {
    const uint16_t range = static_cast<uint16_t>(_config.brightnessMax - _config.brightnessMin);
    uint8_t value = static_cast<uint8_t>(_config.brightnessMin + (range * normalized) / 255u);
    if (stale) {
        value = static_cast<uint8_t>(value / 4u);
    }
    return value;
}

uint32_t MatrixDataVisualizationEngine::colorFor(uint8_t normalized, uint8_t brightness) const {
    return scaleColor(gradientColor(_config.colorMin, _config.colorMid, _config.colorMax, normalized), brightness);
}

uint8_t MatrixDataVisualizationEngine::binForColumn(uint8_t column, uint8_t fallback) const {
    if (_input.binCount == 0) {
        return fallback;
    }

    const uint8_t start = static_cast<uint8_t>((static_cast<uint16_t>(column) * _input.binCount) / kWidth);
    uint8_t end = static_cast<uint8_t>((static_cast<uint16_t>(column + 1u) * _input.binCount) / kWidth);
    if (end <= start) {
        end = static_cast<uint8_t>(start + 1u);
    }
    if (end > _input.binCount) {
        end = _input.binCount;
    }

    uint16_t sum = 0;
    uint8_t count = 0;
    for (uint8_t i = start; i < end; ++i) {
        sum += _input.bins[i];
        count++;
    }
    return count > 0 ? static_cast<uint8_t>(sum / count) : fallback;
}

bool MatrixDataVisualizationEngine::shouldBlank() const {
    const bool stale = !_input.valid || _input.stale;
    return stale && _config.staleBehavior == static_cast<uint8_t>(MATRIX::MatrixDataStaleBehavior::Blank);
}

void MatrixDataVisualizationEngine::clear(uint32_t* outFrame) const {
    for (uint8_t i = 0; i < kPixels; ++i) {
        outFrame[i] = 0;
    }
}

void MatrixDataVisualizationEngine::fillStaleGray(uint32_t* outFrame) const {
    const uint32_t gray = scaleColor(0x808080, _config.brightnessMin > 0 ? _config.brightnessMin : 8);
    for (uint8_t i = 0; i < kPixels; ++i) {
        outFrame[i] = gray;
    }
}

void MatrixDataVisualizationEngine::pushTrend(uint8_t normalized) {
    _trend[_trendHead] = normalized;
    _trendHead = static_cast<uint8_t>((_trendHead + 1u) % kPixels);
    if (_trendHead == 0) {
        _trendFilled = true;
    }
}

uint8_t MatrixDataVisualizationEngine::trendAt(uint8_t index) const {
    const uint8_t available = _trendFilled ? kPixels : _trendHead;
    if (index >= available) {
        return 0;
    }
    const uint8_t start = _trendFilled ? _trendHead : 0;
    return _trend[(start + index) % kPixels];
}

void MatrixDataVisualizationEngine::renderGauge(uint32_t* outFrame, uint8_t normalized, uint8_t brightness) {
    const uint8_t lit = static_cast<uint8_t>((static_cast<uint16_t>(normalized) * kPixels + 127u) / 255u);
    for (uint8_t i = 0; i < lit; ++i) {
        const uint8_t x = i % kWidth;
        const uint8_t y = static_cast<uint8_t>(kHeight - 1u - (i / kWidth));
        const uint8_t localNorm = static_cast<uint8_t>((static_cast<uint16_t>(i) * 255u) / (kPixels - 1u));
        outFrame[xy(x, y)] = colorFor(localNorm > normalized ? normalized : localNorm, brightness);
    }
}

void MatrixDataVisualizationEngine::renderCenterRipple(
    uint32_t nowMs,
    uint32_t* outFrame,
    uint8_t normalized,
    uint8_t brightness) {
    const float phase = static_cast<float>((nowMs / 32u) % 256u) / 255.0f;
    const float valuePhase = static_cast<float>(normalized) / 255.0f;
    const float centerX = 3.5f;
    const float centerY = 3.5f;
    for (uint8_t y = 0; y < kHeight; ++y) {
        for (uint8_t x = 0; x < kWidth; ++x) {
            const float dx = static_cast<float>(x) - centerX;
            const float dy = static_cast<float>(y) - centerY;
            const float dist = std::sqrt(dx * dx + dy * dy) / 5.0f;
            const float wave = 0.5f + 0.5f * std::cos((dist - phase - valuePhase * 0.25f) * 2.0f * kPi);
            const uint8_t localNorm = clampByte(static_cast<int>(normalized * (0.35f + 0.65f * wave)));
            outFrame[xy(x, y)] = colorFor(localNorm, static_cast<uint8_t>(brightness * (0.30f + 0.70f * wave)));
        }
    }
}

void MatrixDataVisualizationEngine::renderHeatmap(
    uint32_t* outFrame,
    uint8_t normalized,
    uint8_t brightness,
    bool stale) {
    if (_input.binCount > 0) {
        for (uint8_t i = 0; i < kPixels; ++i) {
            const uint8_t srcIndex = static_cast<uint8_t>((static_cast<uint16_t>(i) * _input.binCount) / kPixels);
            const uint8_t bin = _input.bins[srcIndex < _input.binCount ? srcIndex : (_input.binCount - 1u)];
            const uint8_t effective = static_cast<uint8_t>(
                (static_cast<uint16_t>(bin) * 3u + static_cast<uint16_t>(normalized)) / 4u);
            outFrame[i] = colorFor(effective, effectiveBrightness(effective, stale));
        }
        return;
    }

    for (uint8_t y = 0; y < kHeight; ++y) {
        for (uint8_t x = 0; x < kWidth; ++x) {
            const uint8_t spatial = static_cast<uint8_t>((static_cast<uint16_t>(x + y) * 255u) / 14u);
            const uint8_t mixed = static_cast<uint8_t>(
                (static_cast<uint16_t>(spatial) + static_cast<uint16_t>(normalized) * 2u) / 3u);
            outFrame[xy(x, y)] = colorFor(mixed, brightness);
        }
    }
}

void MatrixDataVisualizationEngine::renderTrend(uint32_t* outFrame, uint8_t normalized, uint8_t brightness) {
    if (!_trendFilled && _trendHead == 0) {
        renderGauge(outFrame, normalized, brightness);
        return;
    }

    const uint8_t available = _trendFilled ? kPixels : _trendHead;
    for (uint8_t x = 0; x < kWidth; ++x) {
        if (available < kWidth && x < static_cast<uint8_t>(kWidth - available)) {
            continue;
        }
        const uint8_t sampleIndex = available >= kWidth
            ? static_cast<uint8_t>(available - kWidth + x)
            : static_cast<uint8_t>(x - (kWidth - available));
        const uint8_t colValue = trendAt(sampleIndex);
        const uint8_t yPoint = static_cast<uint8_t>(7u - ((static_cast<uint16_t>(colValue) * 7u) / 255u));
        outFrame[xy(x, yPoint)] = colorFor(colValue, brightness);
        if (yPoint < 7u) {
            outFrame[xy(x, static_cast<uint8_t>(yPoint + 1u))] =
                colorFor(colValue, static_cast<uint8_t>((static_cast<uint16_t>(brightness) * 96u) / 255u));
        }
        if (yPoint > 0u) {
            outFrame[xy(x, static_cast<uint8_t>(yPoint - 1u))] =
                colorFor(colValue, static_cast<uint8_t>((static_cast<uint16_t>(brightness) * 64u) / 255u));
        }
    }
}

void MatrixDataVisualizationEngine::renderSpectrumBars(
    uint32_t nowMs,
    uint32_t* outFrame,
    uint8_t normalized,
    uint8_t brightness) {
    (void)nowMs;
    for (uint8_t x = 0; x < kWidth; ++x) {
        const uint8_t centerDistance = x > 3u ? static_cast<uint8_t>(x - 3u) : static_cast<uint8_t>(3u - x);
        const uint16_t profile = static_cast<uint16_t>(255u - centerDistance * 24u);
        const uint8_t fallback = static_cast<uint8_t>((static_cast<uint16_t>(normalized) * profile) / 255u);
        const uint8_t value = binForColumn(x, fallback);
        const uint8_t height = static_cast<uint8_t>(
            (static_cast<uint16_t>(value) * kHeight + 127u) / 255u);
        const uint8_t colBrightness = static_cast<uint8_t>(
            (static_cast<uint16_t>(brightness) * (128u + value)) / 383u);

        for (uint8_t n = 0; n < height; ++n) {
            const uint8_t y = static_cast<uint8_t>(kHeight - 1u - n);
            const uint8_t localNorm = static_cast<uint8_t>(
                (static_cast<uint16_t>(n) * 255u) / (kHeight - 1u));
            outFrame[xy(x, y)] = colorFor(localNorm > value ? value : localNorm, colBrightness);
        }
    }
}

void MatrixDataVisualizationEngine::renderPerimeterMeter(
    uint32_t nowMs,
    uint32_t* outFrame,
    uint8_t normalized,
    uint8_t brightness) {
    (void)nowMs;
    constexpr uint8_t kPerimeterPixels = 28;
    const uint8_t lit = static_cast<uint8_t>(
        (static_cast<uint16_t>(normalized) * kPerimeterPixels + 127u) / 255u);

    for (uint8_t i = 0; i < lit; ++i) {
        const uint8_t localNorm = static_cast<uint8_t>(
            (static_cast<uint16_t>(i) * 255u) / (kPerimeterPixels - 1u));
        outFrame[xy(perimeterX(i), perimeterY(i))] =
            colorFor(localNorm > normalized ? normalized : localNorm, brightness);
    }

    const uint8_t glow = static_cast<uint8_t>((static_cast<uint16_t>(brightness) * 72u) / 255u);
    const uint32_t centerColor = colorFor(normalized, glow);
    outFrame[xy(3, 3)] = centerColor;
    outFrame[xy(4, 3)] = centerColor;
    outFrame[xy(3, 4)] = centerColor;
    outFrame[xy(4, 4)] = centerColor;
}

void MatrixDataVisualizationEngine::renderPulse(
    uint32_t nowMs,
    uint32_t* outFrame,
    uint8_t normalized,
    uint8_t brightness) {
    const float phase = static_cast<float>((nowMs / 48u) % 256u) / 255.0f;
    const float valuePhase = static_cast<float>(normalized) / 255.0f;
    const float centerX = 3.5f;
    const float centerY = 3.5f;
    for (uint8_t y = 0; y < kHeight; ++y) {
        for (uint8_t x = 0; x < kWidth; ++x) {
            const float dx = static_cast<float>(x) - centerX;
            const float dy = static_cast<float>(y) - centerY;
            const float dist = std::sqrt(dx * dx + dy * dy) / 5.0f;
            const float pulse = 0.5f + 0.5f * std::sin((phase + valuePhase * 0.4f - dist * 0.65f) * 2.0f * kPi);
            const uint8_t localNorm = clampByte(static_cast<int>(
                static_cast<float>(normalized) * (0.45f + 0.55f * pulse)));
            const uint8_t localBrightness = clampByte(static_cast<int>(
                static_cast<float>(brightness) * (0.22f + 0.78f * pulse)));
            outFrame[xy(x, y)] = colorFor(localNorm, localBrightness);
        }
    }
}

}  // namespace MATRIX_VIZ
