#pragma once

#include <stdint.h>

#include "CsiMotionTypes.h"

namespace WIFISENSING {
namespace CSI {

struct CsiMotionStorage {
    float energy[MAX_CSI_SUBCARRIERS];
    float mean[MAX_CSI_SUBCARRIERS];
    float m2[MAX_CSI_SUBCARRIERS];
    float noise[MAX_CSI_SUBCARRIERS];
    uint16_t baselineCount[MAX_CSI_SUBCARRIERS];
    uint8_t valid[MAX_CSI_SUBCARRIERS];
    float topScores[32];
};

class CsiBandMotionDetector {
public:
    CsiBandMotionDetector() = default;
    ~CsiBandMotionDetector();

    bool begin();
    void end();
    void configure(const CsiMotionConfig& config);
    void resetBaseline(CsiMotionResetReason reason = CsiMotionResetReason::ManualCalibration);
    void restoreRetainedMotion(bool motion);
    CsiMotionSnapshot markDataUnavailable(
        CsiMotionResetReason reason = CsiMotionResetReason::FrameGap);
    CsiMotionSnapshot process(const CsiPacket& packet, uint32_t nowMs);
    CsiMotionSnapshot snapshot() const { return _snapshot; }
    bool isEnabled() const { return _config.enabled; }
    bool storageReady() const { return _storage != nullptr; }

private:
    enum class SourceIdentityResult : uint8_t {
        Accepted,
        Ignore,
        Changed,
    };

    struct ScoreResult {
        float score = 0.0f;
        float globalHighRatio = 0.0f;
        uint16_t selectedCarrierCount = 0;
        uint16_t validSelectedCarrierCount = 0;
        uint16_t validCarrierCount = 0;
    };

    void normalizeConfig(CsiMotionConfig& config) const;
    void resetBaselineForWidth(uint16_t width);
    void resetTemporalStateAfterGap();
    SourceIdentityResult updateSourceIdentity(const CsiPacket& packet);
    void clearBaselineArrays();
    void computeEnergy(const CsiPacket& packet, uint16_t width);
    void accumulateBaseline(uint16_t width);
    void finalizeBaseline(uint16_t width);
    ScoreResult scoreSelectedBands(uint16_t width);
    bool updateNoisyGate(const ScoreResult& score, uint32_t nowMs);
    void updateBaselineEwma(uint16_t width, uint32_t nowMs);
    CsiMotionSnapshot makeSnapshot(CsiMotionState state, const ScoreResult* score = nullptr);
    void fillVisualizationBins(CsiMotionSnapshot& snapshot, uint16_t width) const;

    CsiMotionStorage* _storage = nullptr;
    CsiMotionConfig _config;
    CsiMotionSnapshot _snapshot;
    bool _baselineReady = false;
    bool _motion = false;
    bool _requiresManualCalibration = false;
    uint16_t _width = 0;
    uint32_t _framesSeen = 0;
    uint16_t _baselineFramesSeen = 0;
    uint32_t _candidateSinceMs = 0;
    uint32_t _clearSinceMs = 0;
    uint32_t _noisyClearSinceMs = 0;
    uint32_t _globalHighSinceMs = 0;
    uint32_t _quietSinceMs = 0;
    uint32_t _lastBaselineUpdateMs = 0;
    bool _quietTracking = false;
    bool _baselineUpdateStarted = false;
    uint32_t _lastFrameMs = 0;
    bool _hasLastFrame = false;
    bool _sourceIdentityKnown = false;
    uint8_t _sourceMac[6] = {};
    uint8_t _sourceChannel = 0;
    uint8_t _sourceSecondaryChannel = 0;
    uint8_t _pendingSourceMac[6] = {};
    uint8_t _pendingSourceChannel = 0;
    uint8_t _pendingSourceSecondaryChannel = 0;
    uint8_t _pendingSourceFrames = 0;
    bool _storageAllocationFailed = false;
    CsiMotionResetReason _lastResetReason = CsiMotionResetReason::None;

    static constexpr uint32_t kFrameGapResetMs = 5000;
    static constexpr uint8_t kSourceSwitchConfirmFrames = 5;
    static constexpr uint32_t kBaselineAdaptDelayMs = 30000;
    static constexpr uint32_t kBaselineAdaptTimeConstantMs = 120000;
    static constexpr uint32_t kBaselineAdaptMaxStepMs = 1000;
};

} // namespace CSI
} // namespace WIFISENSING
