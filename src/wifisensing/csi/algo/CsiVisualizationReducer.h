#pragma once

#include <cstdint>

#include "../data/CsiTypes.h"
#include "CsiVisualizationTypes.h"

namespace WIFISENSING {
namespace CSI {

class CsiVisualizationReducer {
public:
    void reset();
    CsiVisualizationSnapshot process(const CsiPacket& packet, uint32_t nowMs);
    CsiVisualizationSnapshot snapshot() const { return _snapshot; }

private:
    static constexpr float kQuietAlpha = 0.035f;
    static constexpr float kMediumAlpha = 0.18f;
    static constexpr float kFastAlpha = 0.42f;
    static constexpr float kDisplayDeadband = 6.0f;
    static constexpr float kMediumDelta = 14.0f;
    static constexpr float kFastDelta = 28.0f;
    static constexpr float kMotionDeadband = 5.0f;
    static constexpr float kMotionScale = 15.0f;
    static constexpr float kMotionRiseAlpha = 0.40f;
    static constexpr float kMotionFallAlpha = 0.08f;
    static constexpr float kContrastExpandAlpha = 0.12f;
    static constexpr float kContrastContractAlpha = 0.015f;
    static constexpr float kMinContrastSpan = 1.5f;

    static uint8_t clampByte(int value);
    static float sanitizeGain(float gain);

    uint8_t _lastTarget[CSI_VISUALIZATION_BIN_COUNT] = {};
    float _smoothed[CSI_VISUALIZATION_BIN_COUNT] = {};
    bool _hasSmoothed = false;
    uint16_t _lastWidth = 0;
    float _contrastMin = 0.0f;
    float _contrastMax = 0.0f;
    float _motionLevel = 0.0f;
    CsiVisualizationSnapshot _snapshot;
};

} // namespace CSI
} // namespace WIFISENSING
