#include "CsiScenarioFixtureRunnerTests.h"

#include <unity.h>

#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "CsiScenarioFixtureRunner.h"

using namespace WIFISENSING::CSI;
using namespace WIFISENSING::CSI::NATIVE_CAPTURE;

namespace {

std::string scenarioJson(const std::string& fixtureId,
                         const std::string& captureHash,
                         const std::string& timeline,
                         bool acceptanceReviewed = true,
                         const char* sourceKind = "synthetic_unit") {
    std::ostringstream json;
    json << R"json({
  "schema": "matrixhub.csi.scenario/v1",
  "fixture_id": ")json"
         << fixtureId << R"json(",
  "description": "Synthetic parser/metric unit test only.",
  "capture_file": "frames.mhcf",
  "capture_sha256": "sha256:)json"
         << captureHash << R"json(",
  "source": {"kind": ")json"
         << sourceKind << R"json("},
  "detector_config": {
    "enabled": true,
    "bands": [{"start": 0, "end": 7}],
    "baseline_frames": 30,
    "top_k": 4,
    "sensitivity": 1,
    "enter_threshold": 6.0,
    "clear_threshold": 3.0,
    "hold_ms": 1200,
    "clear_hold_ms": 2500,
    "min_noise": 4.0,
    "min_energy": 4.0,
    "noisy_threshold": 80.0,
    "auto_recalibration": false
  },
  "ground_truth": {
    "reviewed": true,
    "timeline": )json"
         << timeline << R"json(
  },
  "acceptance": {
    "reviewed": )json"
         << (acceptanceReviewed ? "true" : "false") << R"json(,
    "max_false_positive_ms": 0,
    "max_false_negative_ms": 0,
    "max_invalid_decision_ms": 0,
    "max_detection_latency_ms": 0,
    "max_clear_latency_ms": 0,
    "max_motion_dropout_ms": 0,
    "max_missed_motion_intervals": 0,
    "max_uncleared_transitions": 0
  }
})json";
    return json.str();
}

std::string knownNoneTimeline(uint32_t durationMs) {
    std::ostringstream timeline;
    timeline << R"json([{
      "start_ms": 0,
      "end_ms": )json"
             << durationMs << R"json(,
      "motion": "none",
      "occupancy": "empty",
      "environment": "stable",
      "evidence": "scripted_action",
      "confidence": "high",
      "note": "Synthetic parser/metric unit interval."
    }])json";
    return timeline.str();
}

std::string joinViolations(const std::vector<std::string>& violations) {
    std::ostringstream message;
    for (size_t index = 0; index < violations.size(); ++index) {
        if (index != 0) {
            message << "; ";
        }
        message << violations[index];
    }
    return message.str();
}

} // namespace

void test_scenario_parser_requires_reviewed_real_data_contract() {
    const std::string hash(64, '0');
    const std::string json = scenarioJson(
        "synthetic-parser",
        hash,
        knownNoneTimeline(101));
    CsiScenario scenario;
    std::string error;
    TEST_ASSERT_TRUE_MESSAGE(
        parseScenarioJson(
            json,
            "synthetic-parser",
            ScenarioSourcePolicy::AllowSyntheticUnit,
            scenario,
            error),
        error.c_str());
    TEST_ASSERT_EQUAL_STRING("synthetic-parser", scenario.fixtureId.c_str());
    TEST_ASSERT_EQUAL_UINT32(101, scenario.durationMs);
    TEST_ASSERT_EQUAL_UINT8(1, scenario.detectorConfig.bandCount);

    TEST_ASSERT_FALSE(parseScenarioJson(
        json,
        "synthetic-parser",
        ScenarioSourcePolicy::RealDeviceOnly,
        scenario,
        error));
    TEST_ASSERT_NOT_EQUAL(-1, error.find("real_device"));

    const std::string unreviewed = scenarioJson(
        "synthetic-parser",
        hash,
        knownNoneTimeline(101),
        false);
    TEST_ASSERT_FALSE(parseScenarioJson(
        unreviewed,
        "synthetic-parser",
        ScenarioSourcePolicy::AllowSyntheticUnit,
        scenario,
        error));
    TEST_ASSERT_NOT_EQUAL(-1, error.find("not reviewed"));
}

void test_replay_metrics_measure_errors_latency_hold_and_clear() {
    CsiScenario scenario;
    scenario.durationMs = 501;
    scenario.timeline = {
        {0, 100, GroundTruthMotion::None},
        {100, 300, GroundTruthMotion::Present},
        {300, 501, GroundTruthMotion::None},
    };
    const CsiMotionState state = CsiMotionState::Monitoring;
    const std::vector<ReplayObservation> observations = {
        {0, false, state, true, 0},
        {50, true, state, true, 50},
        {100, false, state, true, 100},
        {150, true, state, true, 150},
        {200, false, state, true, 200},
        {250, true, state, true, 250},
        {300, true, CsiMotionState::NoisyEnvironment, false, 300},
        {350, false, state, true, 350},
        {400, false, state, true, 400},
        {450, false, state, true, 450},
        {500, false, state, true, 500},
    };

    ReplayMetrics metrics;
    std::string error;
    TEST_ASSERT_TRUE_MESSAGE(
        evaluateReplay(scenario, observations, metrics, error),
        error.c_str());
    TEST_ASSERT_EQUAL_UINT32(11, metrics.observationCount);
    TEST_ASSERT_EQUAL_UINT32(2, metrics.truePositiveFrames);
    TEST_ASSERT_EQUAL_UINT32(5, metrics.trueNegativeFrames);
    TEST_ASSERT_EQUAL_UINT32(2, metrics.falsePositiveFrames);
    TEST_ASSERT_EQUAL_UINT32(2, metrics.falseNegativeFrames);
    TEST_ASSERT_EQUAL_UINT32(1, metrics.invalidDecisionFrames);
    TEST_ASSERT_EQUAL_UINT64(100, metrics.falsePositiveMs);
    TEST_ASSERT_EQUAL_UINT64(100, metrics.falseNegativeMs);
    TEST_ASSERT_EQUAL_UINT64(50, metrics.invalidDecisionMs);
    TEST_ASSERT_EQUAL_UINT32(50, metrics.maxFalsePositiveRunMs);
    TEST_ASSERT_EQUAL_UINT32(50, metrics.maxFalseNegativeRunMs);
    TEST_ASSERT_EQUAL_UINT32(50, metrics.maxInvalidDecisionRunMs);
    TEST_ASSERT_EQUAL_UINT32(1, metrics.motionIntervalCount);
    TEST_ASSERT_EQUAL_UINT32(1, metrics.detectedMotionIntervals);
    TEST_ASSERT_EQUAL_UINT32(0, metrics.missedMotionIntervals);
    TEST_ASSERT_EQUAL_UINT32(50, metrics.maxDetectionLatencyMs);
    TEST_ASSERT_EQUAL_UINT32(1, metrics.motionDropoutCount);
    TEST_ASSERT_EQUAL_UINT32(50, metrics.maxMotionDropoutMs);
    TEST_ASSERT_EQUAL_UINT32(1, metrics.clearTransitionCount);
    TEST_ASSERT_EQUAL_UINT32(1, metrics.clearedTransitions);
    TEST_ASSERT_EQUAL_UINT32(0, metrics.unclearedTransitions);
    TEST_ASSERT_EQUAL_UINT32(50, metrics.maxClearLatencyMs);

    ReplayAcceptance acceptance;
    acceptance.maxFalsePositiveMs = 100;
    acceptance.maxFalseNegativeMs = 100;
    acceptance.maxInvalidDecisionMs = 50;
    acceptance.maxDetectionLatencyMs = 50;
    acceptance.maxClearLatencyMs = 50;
    acceptance.maxMotionDropoutMs = 50;
    std::vector<std::string> violations;
    TEST_ASSERT_TRUE(evaluateAcceptance(metrics, acceptance, violations));
    TEST_ASSERT_TRUE(violations.empty());

    acceptance.maxFalsePositiveMs = 99;
    TEST_ASSERT_FALSE(evaluateAcceptance(metrics, acceptance, violations));
    TEST_ASSERT_EQUAL_UINT32(1, violations.size());
    TEST_ASSERT_NOT_EQUAL(-1, violations.front().find("false_positive_ms=100"));

    acceptance.maxFalsePositiveMs = 100;
    acceptance.maxInvalidDecisionMs = 49;
    TEST_ASSERT_FALSE(evaluateAcceptance(metrics, acceptance, violations));
    TEST_ASSERT_EQUAL_UINT32(1, violations.size());
    TEST_ASSERT_NOT_EQUAL(-1, violations.front().find("invalid_decision_ms=50"));
}

void test_replay_metrics_mark_frame_staleness_unavailable() {
    CsiScenario scenario;
    scenario.durationMs = 6002;
    scenario.timeline = {
        {0, 5500, GroundTruthMotion::None},
        {5500, 6002, GroundTruthMotion::Present},
    };
    const std::vector<ReplayObservation> observations = {
        {0, false, CsiMotionState::Monitoring, true, 0},
        {6001, true, CsiMotionState::MotionConfirmed, true, 6001},
    };

    ReplayMetrics metrics;
    std::string error;
    TEST_ASSERT_TRUE_MESSAGE(
        evaluateReplay(scenario, observations, metrics, error),
        error.c_str());
    TEST_ASSERT_EQUAL_UINT64(1000, metrics.invalidDecisionMs);
    TEST_ASSERT_EQUAL_UINT32(1000, metrics.maxInvalidDecisionRunMs);
    TEST_ASSERT_EQUAL_UINT32(0, metrics.invalidDecisionFrames);
    TEST_ASSERT_EQUAL_UINT64(501, metrics.falseNegativeMs);
    TEST_ASSERT_EQUAL_UINT32(501, metrics.maxDetectionLatencyMs);
    TEST_ASSERT_EQUAL_UINT32(1, metrics.detectedMotionIntervals);
    TEST_ASSERT_EQUAL_UINT32(0, metrics.missedMotionIntervals);
}

void test_fixture_runner_sha256_matches_known_vector() {
    static constexpr uint8_t value[] = {'a', 'b', 'c'};
    TEST_ASSERT_EQUAL_STRING(
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        sha256Hex(value, sizeof(value)).c_str());
}

void test_real_csi_fixture_corpus_when_explicitly_enabled() {
    const char* root = std::getenv("MATRIXHUB_CSI_FIXTURE_ROOT");
    if (!root || root[0] == '\0') {
        TEST_IGNORE_MESSAGE(
            "Real CSI corpus NOT RUN; use scripts/tests/run_csi_fixture_replay.py");
    }

    std::vector<std::string> fixtureDirectories;
    std::string error;
    TEST_ASSERT_TRUE_MESSAGE(
        listFixtureDirectories(root, fixtureDirectories, error),
        error.c_str());
    TEST_ASSERT_GREATER_THAN_UINT32(0, fixtureDirectories.size());

    uint64_t motionIntervals = 0;
    uint64_t clearTransitions = 0;
    for (const auto& fixtureDirectory : fixtureDirectories) {
        FixtureRunReport report;
        if (!runFixtureDirectory(
                fixtureDirectory,
                report,
                error)) {
            const std::string message = fixtureDirectory + ": " + error;
            TEST_FAIL_MESSAGE(message.c_str());
        }
        std::printf(
            "[CSI fixture] %s frames=%u fp_ms=%llu fn_ms=%llu "
            "invalid_ms=%llu detect_ms=%u dropout_ms=%u clear_ms=%u "
            "missed=%u uncleared=%u\n",
            report.fixtureId.c_str(),
            report.frameCount,
            static_cast<unsigned long long>(report.metrics.falsePositiveMs),
            static_cast<unsigned long long>(report.metrics.falseNegativeMs),
            static_cast<unsigned long long>(report.metrics.invalidDecisionMs),
            report.metrics.maxDetectionLatencyMs,
            report.metrics.maxMotionDropoutMs,
            report.metrics.maxClearLatencyMs,
            report.metrics.missedMotionIntervals,
            report.metrics.unclearedTransitions);
        if (!report.acceptanceViolations.empty()) {
            const std::string message =
                report.fixtureId + ": " + joinViolations(report.acceptanceViolations);
            TEST_FAIL_MESSAGE(message.c_str());
        }
        motionIntervals += report.metrics.motionIntervalCount;
        clearTransitions += report.metrics.clearTransitionCount;
    }
    TEST_ASSERT_TRUE_MESSAGE(
        motionIntervals > 0,
        "Real CSI corpus has no reviewed motion-present interval");
    TEST_ASSERT_TRUE_MESSAGE(
        clearTransitions > 0,
        "Real CSI corpus has no reviewed motion-to-known-quiet transition");
}
