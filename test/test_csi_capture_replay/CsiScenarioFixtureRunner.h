#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../../src/wifisensing/csi/algo/CsiMotionTypes.h"

namespace WIFISENSING {
namespace CSI {
namespace NATIVE_CAPTURE {

enum class ScenarioSourcePolicy : uint8_t {
    RealDeviceOnly = 0,
    AllowSyntheticUnit = 1,
};

enum class GroundTruthMotion : uint8_t {
    None = 0,
    Present = 1,
    Unknown = 2,
};

struct GroundTruthInterval {
    uint32_t startMs = 0;
    uint32_t endMs = 0;
    GroundTruthMotion motion = GroundTruthMotion::Unknown;
};

struct ReplayAcceptance {
    uint32_t maxFalsePositiveMs = 0;
    uint32_t maxFalseNegativeMs = 0;
    uint32_t maxInvalidDecisionMs = 0;
    uint32_t maxDetectionLatencyMs = 0;
    uint32_t maxClearLatencyMs = 0;
    uint32_t maxMotionDropoutMs = 0;
    uint32_t maxMissedMotionIntervals = 0;
    uint32_t maxUnclearedTransitions = 0;
};

struct CsiScenario {
    std::string fixtureId;
    std::string captureFile;
    std::string captureSha256;
    std::string sourceKind;
    CsiMotionConfig detectorConfig{};
    std::vector<GroundTruthInterval> timeline;
    ReplayAcceptance acceptance{};
    uint32_t durationMs = 0;
};

struct ReplayObservation {
    uint32_t relativeMs = 0;
    bool motion = false;
    CsiMotionState state = CsiMotionState::Disabled;
    bool decisionValid = false;
    uint32_t lastEvidenceMs = 0;
};

struct ReplayMetrics {
    uint32_t observationCount = 0;
    uint32_t truePositiveFrames = 0;
    uint32_t trueNegativeFrames = 0;
    uint32_t falsePositiveFrames = 0;
    uint32_t falseNegativeFrames = 0;
    uint32_t invalidDecisionFrames = 0;
    uint32_t unknownFrames = 0;

    uint64_t truePositiveMs = 0;
    uint64_t trueNegativeMs = 0;
    uint64_t falsePositiveMs = 0;
    uint64_t falseNegativeMs = 0;
    uint64_t invalidDecisionMs = 0;
    uint64_t unknownMs = 0;
    uint32_t maxFalsePositiveRunMs = 0;
    uint32_t maxFalseNegativeRunMs = 0;
    uint32_t maxInvalidDecisionRunMs = 0;

    uint32_t motionIntervalCount = 0;
    uint32_t detectedMotionIntervals = 0;
    uint32_t missedMotionIntervals = 0;
    uint32_t maxDetectionLatencyMs = 0;
    uint32_t motionDropoutCount = 0;
    uint32_t maxMotionDropoutMs = 0;

    uint32_t clearTransitionCount = 0;
    uint32_t clearedTransitions = 0;
    uint32_t unclearedTransitions = 0;
    uint32_t maxClearLatencyMs = 0;

    std::array<uint32_t, static_cast<size_t>(CsiMotionState::NeedsCalibration) + 1u>
        stateFrames{};
};

struct FixtureRunReport {
    std::string fixtureId;
    uint32_t frameCount = 0;
    ReplayMetrics metrics{};
    std::vector<std::string> acceptanceViolations;
};

bool parseScenarioJson(const std::string& json,
                       const std::string& expectedFixtureId,
                       ScenarioSourcePolicy sourcePolicy,
                       CsiScenario& scenario,
                       std::string& error);

bool evaluateReplay(const CsiScenario& scenario,
                    const std::vector<ReplayObservation>& observations,
                    ReplayMetrics& metrics,
                    std::string& error);

bool evaluateAcceptance(const ReplayMetrics& metrics,
                        const ReplayAcceptance& acceptance,
                        std::vector<std::string>& violations);

bool runFixtureDirectory(const std::string& fixtureDirectory,
                         FixtureRunReport& report,
                         std::string& error);

bool listFixtureDirectories(const std::string& root,
                            std::vector<std::string>& fixtureDirectories,
                            std::string& error);

std::string sha256Hex(const uint8_t* data, size_t size);

} // namespace NATIVE_CAPTURE
} // namespace CSI
} // namespace WIFISENSING
