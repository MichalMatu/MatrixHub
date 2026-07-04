#include "CsiVisualizationReducer.h"

#include <cmath>
#include <cstring>

namespace WIFISENSING {
namespace CSI {
namespace {

constexpr float kGainMin = 0.25f;
constexpr float kGainMax = 4.0f;

float clampFloat(float value, float minValue, float maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

} // namespace

void CsiVisualizationReducer::reset() {
    _hasSmoothed = false;
    _lastWidth = 0;
    _contrastMin = 0.0f;
    _contrastMax = 0.0f;
    _motionLevel = 0.0f;
    std::memset(_lastTarget, 0, sizeof(_lastTarget));
    std::memset(_smoothed, 0, sizeof(_smoothed));
    _snapshot = CsiVisualizationSnapshot{};
}

CsiVisualizationSnapshot CsiVisualizationReducer::process(const CsiPacket& packet, uint32_t nowMs) {
    const uint16_t width = static_cast<uint16_t>(packet.len / 2u);
    if (width == 0 || width > (MAX_CSI_DATA_LEN / 2u)) {
        reset();
        _snapshot.timestampMs = nowMs;
        return _snapshot;
    }

    float graph[CSI_VISUALIZATION_BIN_COUNT] = {};
    float minGraph = 0.0f;
    float maxGraph = 0.0f;
    bool hasGraph = false;
    const float gain = sanitizeGain(packet.compensate_gain);
    const float gainSquared = gain * gain;

    for (uint8_t bin = 0; bin < CSI_VISUALIZATION_BIN_COUNT; ++bin) {
        const uint16_t start = static_cast<uint16_t>(
            (static_cast<uint32_t>(bin) * width) / CSI_VISUALIZATION_BIN_COUNT);
        uint16_t end = static_cast<uint16_t>(
            (static_cast<uint32_t>(bin + 1u) * width) / CSI_VISUALIZATION_BIN_COUNT);
        if (end <= start) {
            end = static_cast<uint16_t>(start + 1u);
        }
        if (end > width) {
            end = width;
        }

        float sum = 0.0f;
        uint16_t count = 0;
        for (uint16_t i = start; i < end; ++i) {
            const float re = static_cast<float>(packet.buf[2u * i]);
            const float im = static_cast<float>(packet.buf[(2u * i) + 1u]);
            const float energy = (re * re + im * im) * gainSquared;
            if (!std::isfinite(energy)) {
                continue;
            }
            sum += energy;
            count++;
        }

        const float avgEnergy = count > 0 ? sum / static_cast<float>(count) : 0.0f;
        const float graphValue = std::log1p(avgEnergy);
        graph[bin] = graphValue;
        if (!hasGraph) {
            minGraph = graphValue;
            maxGraph = graphValue;
            hasGraph = true;
        } else {
            if (graphValue < minGraph) {
                minGraph = graphValue;
            }
            if (graphValue > maxGraph) {
                maxGraph = graphValue;
            }
        }
    }

    if (!hasGraph) {
        reset();
        _snapshot.timestampMs = nowMs;
        return _snapshot;
    }

    const bool resetSmoothing = !_hasSmoothed || _lastWidth != width;
    if (resetSmoothing) {
        _contrastMin = minGraph;
        _contrastMax = maxGraph;
        _motionLevel = 0.0f;
    } else {
        _contrastMin += (minGraph - _contrastMin) *
                        (minGraph < _contrastMin ? kContrastExpandAlpha : kContrastContractAlpha);
        _contrastMax += (maxGraph - _contrastMax) *
                        (maxGraph > _contrastMax ? kContrastExpandAlpha : kContrastContractAlpha);
    }

    if ((_contrastMax - _contrastMin) < kMinContrastSpan) {
        const float center = (_contrastMin + _contrastMax) * 0.5f;
        _contrastMin = center - (kMinContrastSpan * 0.5f);
        _contrastMax = center + (kMinContrastSpan * 0.5f);
    }

    const float range = _contrastMax - _contrastMin;
    float motionSum = 0.0f;

    _snapshot = CsiVisualizationSnapshot{};
    _snapshot.valid = true;
    _snapshot.stale = false;
    _snapshot.timestampMs = nowMs;
    _snapshot.width = width;
    _snapshot.binCount = CSI_VISUALIZATION_BIN_COUNT;

    uint8_t target[CSI_VISUALIZATION_BIN_COUNT] = {};
    for (uint8_t bin = 0; bin < CSI_VISUALIZATION_BIN_COUNT; ++bin) {
        float normalized = 0.0f;
        if (range > 0.0001f) {
            normalized = (graph[bin] - _contrastMin) / range;
        } else if (_contrastMax > 0.0001f) {
            normalized = 0.5f;
        }
        const float byteValue = clampFloat(normalized, 0.0f, 1.0f) * 255.0f;
        target[bin] = clampByte(static_cast<int>(std::lround(byteValue)));

        if (resetSmoothing) {
            _smoothed[bin] = static_cast<float>(target[bin]);
        } else {
            const float targetDelta = std::fabs(static_cast<float>(target[bin]) -
                                                static_cast<float>(_lastTarget[bin]));
            if (targetDelta > kMotionDeadband) {
                motionSum += targetDelta - kMotionDeadband;
            }

            const float displayDelta = static_cast<float>(target[bin]) - _smoothed[bin];
            const float absDisplayDelta = std::fabs(displayDelta);
            if (absDisplayDelta >= kDisplayDeadband) {
                float alpha = kQuietAlpha;
                if (absDisplayDelta >= kFastDelta) {
                    alpha = kFastAlpha;
                } else if (absDisplayDelta >= kMediumDelta) {
                    alpha = kMediumAlpha;
                }
                _smoothed[bin] += displayDelta * alpha;
            } else {
                _smoothed[bin] += displayDelta * kQuietAlpha;
            }
        }

        _lastTarget[bin] = target[bin];
        const uint8_t out = clampByte(static_cast<int>(std::lround(_smoothed[bin])));
        _snapshot.bins[bin] = out;
    }

    const float instantMotion = resetSmoothing
        ? 0.0f
        : clampFloat((motionSum / static_cast<float>(CSI_VISUALIZATION_BIN_COUNT)) * kMotionScale,
                     0.0f,
                     100.0f);
    const float motionAlpha = instantMotion > _motionLevel ? kMotionRiseAlpha : kMotionFallAlpha;
    _motionLevel += (instantMotion - _motionLevel) * motionAlpha;

    _snapshot.value = clampFloat(_motionLevel, 0.0f, 100.0f);
    _hasSmoothed = true;
    _lastWidth = width;
    return _snapshot;
}

uint8_t CsiVisualizationReducer::clampByte(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return static_cast<uint8_t>(value);
}

float CsiVisualizationReducer::sanitizeGain(float gain) {
    if (!std::isfinite(gain)) {
        gain = 1.0f;
    }
    return clampFloat(gain, kGainMin, kGainMax);
}

} // namespace CSI
} // namespace WIFISENSING
