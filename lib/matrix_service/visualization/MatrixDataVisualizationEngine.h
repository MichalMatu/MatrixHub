#pragma once

#include <cstddef>
#include <cstdint>

#include "../../../src/matrix/MatrixDataVisualizationTypes.h"

namespace MATRIX_VIZ {

class MatrixDataVisualizationEngine {
public:
    void configure(const MATRIX::MatrixDataVisualizationConfig& config);
    void setInput(const MATRIX::MatrixDataVisualizationInput& input);
    void reset();

    bool render(uint32_t nowMs, uint32_t* outFrame, size_t pixelCount);

    static uint8_t normalizeValue(float value, float minValue, float maxValue);
    static uint32_t gradientColor(uint32_t minColor, uint32_t midColor, uint32_t maxColor, uint8_t value);
    static uint32_t scaleColor(uint32_t color, uint8_t brightness);

private:
    static float clampFloat(float value, float minValue, float maxValue);
    static uint8_t clampByte(int value);
    static uint8_t channel(uint32_t color, uint8_t shift);
    static uint32_t rgb(uint8_t r, uint8_t g, uint8_t b);
    static uint8_t xy(uint8_t x, uint8_t y);
    static uint8_t perimeterX(uint8_t index);
    static uint8_t perimeterY(uint8_t index);

    uint8_t currentValueNorm();
    uint8_t effectiveBrightness(uint8_t normalized, bool stale) const;
    uint32_t colorFor(uint8_t normalized, uint8_t brightness) const;
    uint8_t binForColumn(uint8_t column, uint8_t fallback) const;
    bool shouldBlank() const;
    void clear(uint32_t* outFrame) const;
    void fillStaleGray(uint32_t* outFrame) const;
    void pushTrend(uint8_t normalized);
    uint8_t trendAt(uint8_t index) const;

    void renderGauge(uint32_t* outFrame, uint8_t normalized, uint8_t brightness);
    void renderCenterRipple(uint32_t nowMs, uint32_t* outFrame, uint8_t normalized, uint8_t brightness);
    void renderHeatmap(uint32_t* outFrame, uint8_t normalized, uint8_t brightness, bool stale);
    void renderTrend(uint32_t* outFrame, uint8_t normalized, uint8_t brightness);
    void renderSpectrumBars(uint32_t nowMs, uint32_t* outFrame, uint8_t normalized, uint8_t brightness);
    void renderPerimeterMeter(uint32_t nowMs, uint32_t* outFrame, uint8_t normalized, uint8_t brightness);
    void renderPulse(uint32_t nowMs, uint32_t* outFrame, uint8_t normalized, uint8_t brightness);

    MATRIX::MatrixDataVisualizationConfig _config{};
    MATRIX::MatrixDataVisualizationInput _input{};
    bool _configured = false;
    bool _hasSmoothed = false;
    float _smoothedValue = 0.0f;
    uint32_t _lastInputTimestamp = 0;
    uint8_t _trend[MATRIX::kMatrixDataVizPixelCount] = {};
    uint8_t _trendHead = 0;
    bool _trendFilled = false;
};

}  // namespace MATRIX_VIZ
