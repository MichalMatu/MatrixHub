#pragma once

#include <stdint.h>

#include "../data/CsiTypes.h"

namespace WIFISENSING {
namespace CSI {

constexpr uint8_t MAX_CSI_ALARM_BANDS = 4;
constexpr uint16_t MAX_CSI_SUBCARRIERS = MAX_CSI_DATA_LEN / 2;
constexpr uint32_t CSI_MOTION_STALE_AFTER_MS = 5000;

static_assert(MAX_CSI_ALARM_BANDS == 4, "CSI alarm UI/config assumes max 4 bands");
static_assert(MAX_CSI_SUBCARRIERS <= 256, "Review CSI motion storage before increasing CSI width");

struct CsiBandRange {
    uint16_t start = 0;
    uint16_t end = 0; // inclusive
};

enum class CsiMotionState : uint8_t {
    Disabled = 0,
    NeedsConfiguration = 1,
    Calibrating = 2,
    Monitoring = 3,
    MotionCandidate = 4,
    MotionConfirmed = 5,
    NoisyEnvironment = 6,
    Unavailable = 7,
    NeedsCalibration = 8,
};

enum class CsiMotionResetReason : uint8_t {
    None = 0,
    Startup = 1,
    ConfigChange = 2,
    ManualCalibration = 3,
    WidthChange = 4,
    UnavailableStorage = 5,
    InvalidFrame = 6,
    FrameGap = 7,
    SourceChange = 8,
    GainStabilized = 9,
};

struct CsiMotionSensitivityPreset {
    float enterThreshold;
    float clearThreshold;
};

inline uint8_t normalizeCsiMotionSensitivity(uint8_t sensitivity) {
    return sensitivity > 2 ? 2 : sensitivity;
}

inline CsiMotionSensitivityPreset csiMotionSensitivityPreset(uint8_t sensitivity) {
    switch (normalizeCsiMotionSensitivity(sensitivity)) {
        case 0:
            return {8.0f, 4.0f};
        case 2:
            return {4.5f, 2.2f};
        case 1:
        default:
            return {6.0f, 3.0f};
    }
}

struct CsiMotionConfig {
    bool enabled = false;
    uint8_t bandCount = 0;
    CsiBandRange bands[MAX_CSI_ALARM_BANDS]{};
    uint16_t baselineFrames = 150;
    uint8_t topK = 8;
    float enterThreshold = 6.0f;
    float clearThreshold = 3.0f;
    uint16_t holdMs = 1200;
    uint16_t clearHoldMs = 2500;
    float minNoise = 4.0f;
    float minEnergy = 4.0f;
    float noisyScoreThreshold = 80.0f;
    bool autoRecalibration = true;
    uint8_t sensitivity = 1;
};

struct CsiMotionSnapshot {
    CsiMotionState state = CsiMotionState::Disabled;
    // Last stable binary decision. During noisy/unavailable states this value
    // is retained, but decisionValid is false so consumers do not mistake an
    // unknown signal for a definitive clear edge.
    bool motion = false;
    bool decisionValid = false;
    bool hasFrame = false;
    bool baselineReady = false;
    bool noisy = false;
    bool needsCalibration = false;
    float score = 0.0f;
    float confidence = 0.0f;
    uint32_t framesSeen = 0;
    uint32_t lastFrameMs = 0;
    uint16_t width = 0;
    uint16_t selectedCarrierCount = 0;
    uint16_t validCarrierCount = 0;
    uint8_t bandCount = 0;
    uint8_t visualizationBins[64] = {};
    uint8_t visualizationBinCount = 0;
    CsiMotionResetReason lastResetReason = CsiMotionResetReason::None;
};

inline bool isCsiMotionDecisionValid(CsiMotionState state) {
    return state == CsiMotionState::Monitoring ||
           state == CsiMotionState::MotionConfirmed;
}

const char* toString(CsiMotionState state);
const char* toString(CsiMotionResetReason reason);

} // namespace CSI
} // namespace WIFISENSING
