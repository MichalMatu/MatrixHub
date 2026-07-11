#include <unity.h>

#include <cstring>

#include "../../src/wifisensing/csi/algo/CsiBandMotionDetector.cpp"

using WIFISENSING::CSI::CsiBandMotionDetector;
using WIFISENSING::CSI::CsiBandRange;
using WIFISENSING::CSI::CsiMotionConfig;
using WIFISENSING::CSI::CsiMotionSnapshot;
using WIFISENSING::CSI::CsiMotionState;
using WIFISENSING::CSI::CsiPacket;

namespace {

constexpr uint16_t kWidth = 64;
constexpr uint32_t kStartMs = 1000;

CsiPacket makePacket(uint16_t width, int8_t amplitude) {
    CsiPacket packet{};
    packet.len = static_cast<size_t>(width) * 2;
    packet.compensate_gain = 1.0f;
    for (uint16_t i = 0; i < width; ++i) {
        packet.buf[2 * i] = amplitude;
        packet.buf[(2 * i) + 1] = 0;
    }
    return packet;
}

CsiPacket makePacketWithBand(uint16_t width, int8_t baselineAmplitude, uint16_t start, uint16_t end, int8_t amplitude) {
    CsiPacket packet = makePacket(width, baselineAmplitude);
    if (end >= width) {
        end = static_cast<uint16_t>(width - 1);
    }
    for (uint16_t i = start; i <= end; ++i) {
        packet.buf[2 * i] = amplitude;
    }
    return packet;
}

CsiMotionConfig enabledConfig() {
    CsiMotionConfig config;
    config.enabled = true;
    config.bandCount = 1;
    config.bands[0] = CsiBandRange{10, 17};
    config.baselineFrames = 30;
    config.topK = 4;
    config.enterThreshold = 6.0f;
    config.clearThreshold = 3.0f;
    config.holdMs = 100;
    config.clearHoldMs = 100;
    config.minNoise = 1.0f;
    config.minEnergy = 1.0f;
    config.noisyScoreThreshold = 500.0f;
    config.autoRecalibration = false;
    return config;
}

CsiMotionSnapshot trainBaseline(CsiBandMotionDetector& detector,
                                const CsiMotionConfig& config,
                                uint16_t width = kWidth,
                                int8_t amplitude = 10) {
    CsiMotionSnapshot snapshot{};
    for (uint16_t i = 0; i < config.baselineFrames; ++i) {
        snapshot = detector.process(makePacket(width, amplitude), kStartMs + i);
    }
    return snapshot;
}

} // namespace

void setUp(void) {
    WIFISENSING::CSI::TEST_HOOKS::setCsiMotionStorageAllocationFailure(false);
}

void tearDown(void) {
    WIFISENSING::CSI::TEST_HOOKS::setCsiMotionStorageAllocationFailure(false);
}

void test_disabled_returns_disabled_no_motion() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());

    CsiMotionConfig config;
    detector.configure(config);

    const auto snapshot = detector.process(makePacket(kWidth, 10), kStartMs);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CsiMotionState::Disabled), static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.motion);
}

void test_storage_allocation_failure_returns_unavailable() {
    WIFISENSING::CSI::TEST_HOOKS::setCsiMotionStorageAllocationFailure(true);

    CsiBandMotionDetector detector;
    TEST_ASSERT_FALSE(detector.begin());
    const auto snapshot = detector.process(makePacket(kWidth, 10), kStartMs);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CsiMotionState::Unavailable), static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.motion);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(WIFISENSING::CSI::CsiMotionResetReason::UnavailableStorage),
        static_cast<uint8_t>(snapshot.lastResetReason));
}

void test_no_bands_returns_needs_configuration() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());

    CsiMotionConfig config = enabledConfig();
    config.bandCount = 0;
    detector.configure(config);

    const auto snapshot = detector.process(makePacket(kWidth, 10), kStartMs);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::NeedsConfiguration),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.motion);
}

void test_baseline_converges_after_configured_frames() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);

    const auto snapshot = trainBaseline(detector, config);

    TEST_ASSERT_TRUE(snapshot.baselineReady);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CsiMotionState::Monitoring), static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_EQUAL_UINT16(kWidth, snapshot.width);
    TEST_ASSERT_EQUAL_UINT32(config.baselineFrames, snapshot.framesSeen);
    TEST_ASSERT_TRUE(snapshot.decisionValid);
}

void test_narrow_band_motion_triggers_after_hold() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);
    trainBaseline(detector, config);

    auto snapshot = detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 2000);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::MotionCandidate),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.motion);

    snapshot = detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 2100);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::MotionConfirmed),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_TRUE(snapshot.motion);
}

void test_single_frame_spike_does_not_trigger() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);
    trainBaseline(detector, config);

    auto snapshot = detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 2000);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::MotionCandidate),
        static_cast<uint8_t>(snapshot.state));

    snapshot = detector.process(makePacket(kWidth, 10), 2100);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CsiMotionState::Monitoring), static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.motion);
}

void test_motion_clears_after_clear_hold() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);
    trainBaseline(detector, config);

    detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 2000);
    auto snapshot = detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 2100);
    TEST_ASSERT_TRUE(snapshot.motion);

    snapshot = detector.process(makePacket(kWidth, 10), 2200);
    TEST_ASSERT_TRUE(snapshot.motion);

    snapshot = detector.process(makePacket(kWidth, 10), 2300);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CsiMotionState::Monitoring), static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.motion);
}

void test_visualization_bins_are_deterministic_and_return_to_baseline() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);
    trainBaseline(detector, config);

    const CsiPacket disturbed = makePacketWithBand(kWidth, 10, 10, 17, 20);
    const auto first = detector.process(disturbed, 2000);
    const auto second = detector.process(disturbed, 2100);

    TEST_ASSERT_EQUAL_UINT8(64, first.visualizationBinCount);
    TEST_ASSERT_EQUAL_UINT8(64, second.visualizationBinCount);
    TEST_ASSERT_GREATER_THAN_UINT8(0, first.visualizationBins[10]);
    TEST_ASSERT_EQUAL_MEMORY(first.visualizationBins, second.visualizationBins, sizeof(first.visualizationBins));

    const auto idle = detector.process(makePacket(kWidth, 10), 2300);
    TEST_ASSERT_EQUAL_UINT8(64, idle.visualizationBinCount);
    for (uint8_t i = 0; i < idle.visualizationBinCount; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, idle.visualizationBins[i]);
    }
}

void test_global_noise_enters_noisy_environment() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);
    trainBaseline(detector, config);

    auto snapshot = detector.process(makePacket(kWidth, 20), 2000);
    TEST_ASSERT_FALSE(snapshot.noisy);

    snapshot = detector.process(makePacket(kWidth, 20), 2600);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::NoisyEnvironment),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_TRUE(snapshot.noisy);
    TEST_ASSERT_FALSE(snapshot.motion);
}

void test_width_change_resets_baseline() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);
    trainBaseline(detector, config);

    const auto snapshot = detector.process(makePacket(80, 10), 3000);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CsiMotionState::Calibrating), static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.needsCalibration);
    TEST_ASSERT_FALSE(snapshot.baselineReady);
    TEST_ASSERT_EQUAL_UINT16(80, snapshot.width);
}

void test_dead_carriers_are_ignored() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    CsiMotionConfig config = enabledConfig();
    config.bands[0] = CsiBandRange{8, 15};
    detector.configure(config);

    for (uint16_t frame = 0; frame < config.baselineFrames; ++frame) {
        CsiPacket packet = makePacket(kWidth, 10);
        for (uint16_t i = 8; i <= 11; ++i) {
            packet.buf[2 * i] = 0;
        }
        detector.process(packet, kStartMs + frame);
    }

    CsiPacket packet = makePacket(kWidth, 10);
    for (uint16_t i = 8; i <= 11; ++i) {
        packet.buf[2 * i] = 30;
    }
    auto snapshot = detector.process(packet, 2000);

    TEST_ASSERT_FALSE(snapshot.motion);
    TEST_ASSERT_EQUAL_UINT16(kWidth - 4, snapshot.validCarrierCount);
    TEST_ASSERT_EQUAL_UINT16(8, snapshot.selectedCarrierCount);
}

void test_selected_band_only_detects_selected_band_changes() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);
    trainBaseline(detector, config);

    auto snapshot = detector.process(makePacketWithBand(kWidth, 10, 30, 37, 20), 2000);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CsiMotionState::Monitoring), static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.motion);

    snapshot = detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 2100);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::MotionCandidate),
        static_cast<uint8_t>(snapshot.state));
}

void test_sensitivity_presets_override_manual_thresholds() {
    CsiMotionConfig lowSensitivity = enabledConfig();
    lowSensitivity.sensitivity = 0;
    lowSensitivity.enterThreshold = 1.0f;
    lowSensitivity.clearThreshold = 0.5f;
    lowSensitivity.minNoise = 10.0f;

    CsiBandMotionDetector lowDetector;
    TEST_ASSERT_TRUE(lowDetector.begin());
    lowDetector.configure(lowSensitivity);
    trainBaseline(lowDetector, lowSensitivity);

    auto snapshot = lowDetector.process(makePacketWithBand(kWidth, 10, 10, 17, 13), 2000);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CsiMotionState::Monitoring), static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.motion);

    CsiMotionConfig highSensitivity = enabledConfig();
    highSensitivity.sensitivity = 2;
    highSensitivity.enterThreshold = 100.0f;
    highSensitivity.clearThreshold = 50.0f;
    highSensitivity.minNoise = 10.0f;

    CsiBandMotionDetector highDetector;
    TEST_ASSERT_TRUE(highDetector.begin());
    highDetector.configure(highSensitivity);
    trainBaseline(highDetector, highSensitivity);

    snapshot = highDetector.process(makePacketWithBand(kWidth, 10, 10, 17, 13), 2000);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::MotionCandidate),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.motion);
}

void test_global_gain_jump_does_not_confirm_motion_before_noisy_gate() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);
    trainBaseline(detector, config);

    CsiPacket disturbed = makePacket(kWidth, 10);
    disturbed.compensate_gain = 1.8f;

    auto snapshot = detector.process(disturbed, 2000);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CsiMotionState::Monitoring), static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.motion);

    snapshot = detector.process(disturbed, 2100);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CsiMotionState::Monitoring), static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.motion);

    snapshot = detector.process(disturbed, 2600);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::NoisyEnvironment),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_TRUE(snapshot.noisy);
    TEST_ASSERT_FALSE(snapshot.motion);
}

void test_confirmed_motion_is_not_cleared_by_broad_disturbance() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);
    trainBaseline(detector, config);

    detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 2000);
    auto snapshot = detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 2100);
    TEST_ASSERT_TRUE(snapshot.motion);

    snapshot = detector.process(makePacket(kWidth, 20), 2200);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::MotionConfirmed),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_TRUE(snapshot.motion);
    TEST_ASSERT_TRUE(snapshot.decisionValid);

    snapshot = detector.process(makePacket(kWidth, 20), 2800);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::NoisyEnvironment),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_TRUE(snapshot.motion);
    TEST_ASSERT_FALSE(snapshot.decisionValid);

    snapshot = detector.process(makePacket(kWidth, 10), 2900);
    TEST_ASSERT_TRUE(snapshot.motion);
    TEST_ASSERT_FALSE(snapshot.decisionValid);

    snapshot = detector.process(makePacket(kWidth, 10), 3000);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::Monitoring),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.motion);
    TEST_ASSERT_TRUE(snapshot.decisionValid);
}

void test_stronger_local_motion_cannot_turn_into_noisy_clear() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    CsiMotionConfig config = enabledConfig();
    config.noisyScoreThreshold = 80.0f;
    detector.configure(config);
    trainBaseline(detector, config);

    auto snapshot = detector.process(makePacketWithBand(kWidth, 10, 10, 17, 30), 2000);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::MotionCandidate),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.noisy);

    snapshot = detector.process(makePacketWithBand(kWidth, 10, 10, 17, 30), 2100);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::MotionConfirmed),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_TRUE(snapshot.motion);
}

void test_invalid_frame_cannot_bypass_motion_clear_hold() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);
    trainBaseline(detector, config);

    detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 2000);
    auto snapshot = detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 2100);
    TEST_ASSERT_TRUE(snapshot.motion);

    snapshot = detector.process(makePacket(2, 10), 2150);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::Unavailable),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_TRUE(snapshot.motion);
    TEST_ASSERT_FALSE(snapshot.decisionValid);

    snapshot = detector.process(makePacket(kWidth, 10), 2200);
    TEST_ASSERT_TRUE(snapshot.motion);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::MotionConfirmed),
        static_cast<uint8_t>(snapshot.state));
}

void test_invalid_frame_breaks_motion_candidate_hold() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);
    trainBaseline(detector, config);

    auto snapshot = detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 2000);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::MotionCandidate),
        static_cast<uint8_t>(snapshot.state));

    snapshot = detector.process(makePacket(2, 10), 2200);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::Unavailable),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.motion);

    snapshot = detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 2300);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::MotionCandidate),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.motion);
}

void test_invalid_frame_breaks_quiet_clear_hold() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);
    trainBaseline(detector, config);

    detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 2000);
    auto snapshot = detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 2100);
    TEST_ASSERT_TRUE(snapshot.motion);

    snapshot = detector.process(makePacket(kWidth, 10), 2200);
    TEST_ASSERT_TRUE(snapshot.motion);
    snapshot = detector.process(makePacket(2, 10), 2600);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::Unavailable),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_TRUE(snapshot.motion);

    snapshot = detector.process(makePacket(kWidth, 10), 2700);
    TEST_ASSERT_TRUE(snapshot.motion);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::MotionConfirmed),
        static_cast<uint8_t>(snapshot.state));
}

void test_long_frame_gap_resets_candidate_instead_of_confirming() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);
    trainBaseline(detector, config);

    auto snapshot = detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 2000);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::MotionCandidate),
        static_cast<uint8_t>(snapshot.state));

    snapshot = detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 8001);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::MotionCandidate),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.motion);
    TEST_ASSERT_FALSE(snapshot.decisionValid);
    TEST_ASSERT_TRUE(snapshot.baselineReady);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(WIFISENSING::CSI::CsiMotionResetReason::FrameGap),
        static_cast<uint8_t>(snapshot.lastResetReason));
}

void test_stale_state_preserves_confirmed_motion_as_unknown() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);
    trainBaseline(detector, config);

    detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 2000);
    auto snapshot = detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 2100);
    TEST_ASSERT_TRUE(snapshot.motion);

    snapshot = detector.markDataUnavailable();
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::Unavailable),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_TRUE(snapshot.motion);
    TEST_ASSERT_FALSE(snapshot.decisionValid);

    snapshot = detector.process(makePacket(kWidth, 10), 8001);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::MotionConfirmed),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_TRUE(snapshot.motion);

    snapshot = detector.process(makePacket(kWidth, 10), 8101);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::Monitoring),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.motion);
    TEST_ASSERT_TRUE(snapshot.decisionValid);
}

void test_source_change_resets_baseline_at_same_width() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);
    trainBaseline(detector, config);

    CsiPacket changedSource = makePacket(kWidth, 10);
    changedSource.mac[5] = 1;
    changedSource.rx_ctrl.channel = 6;
    CsiMotionSnapshot snapshot{};
    for (uint8_t i = 0; i < 5; ++i) {
        snapshot = detector.process(changedSource, 2000 + i);
    }

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::Calibrating),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.baselineReady);
    TEST_ASSERT_FALSE(snapshot.decisionValid);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(WIFISENSING::CSI::CsiMotionResetReason::SourceChange),
        static_cast<uint8_t>(snapshot.lastResetReason));
}

void test_active_motion_requires_manual_calibration_after_width_change() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);
    trainBaseline(detector, config);

    detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 2000);
    auto snapshot = detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 2100);
    TEST_ASSERT_TRUE(snapshot.motion);

    for (uint16_t i = 0; i < config.baselineFrames; ++i) {
        snapshot = detector.process(makePacketWithBand(80, 10, 10, 17, 20), 2200 + i);
    }

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::NeedsCalibration),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_TRUE(snapshot.motion);
    TEST_ASSERT_TRUE(snapshot.baselineReady);
    TEST_ASSERT_TRUE(snapshot.needsCalibration);
    TEST_ASSERT_FALSE(snapshot.decisionValid);

    detector.resetBaseline();
    for (uint16_t i = 0; i < config.baselineFrames; ++i) {
        snapshot = detector.process(makePacket(80, 10), 3000 + i);
    }
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::Monitoring),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.motion);
    TEST_ASSERT_TRUE(snapshot.decisionValid);
}

void test_retained_motion_cannot_be_cleared_by_startup_baseline() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);
    detector.restoreRetainedMotion(true);

    CsiMotionSnapshot snapshot{};
    for (uint16_t i = 0; i < config.baselineFrames; ++i) {
        snapshot = detector.process(
            makePacketWithBand(kWidth, 10, 10, 17, 20),
            kStartMs + i);
        TEST_ASSERT_TRUE(snapshot.motion);
        TEST_ASSERT_FALSE(snapshot.decisionValid);
    }

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::NeedsCalibration),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_TRUE(snapshot.needsCalibration);
    TEST_ASSERT_TRUE(snapshot.baselineReady);
}

void test_interleaved_foreign_source_does_not_reset_baseline() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);
    auto snapshot = trainBaseline(detector, config);
    const uint32_t acceptedFrames = snapshot.framesSeen;

    CsiPacket foreign = makePacket(kWidth, 30);
    foreign.mac[5] = 2;
    foreign.rx_ctrl.channel = 6;
    const CsiPacket established = makePacket(kWidth, 10);

    for (uint8_t i = 0; i < 10; ++i) {
        snapshot = detector.process(foreign, 2000 + (i * 2));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(CsiMotionState::Monitoring),
            static_cast<uint8_t>(snapshot.state));
        snapshot = detector.process(established, 2001 + (i * 2));
    }

    TEST_ASSERT_TRUE(snapshot.baselineReady);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::Monitoring),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_EQUAL_UINT32(acceptedFrames + 10, snapshot.framesSeen);
}

void test_first_word_invalid_carriers_are_excluded() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    CsiMotionConfig config = enabledConfig();
    config.bands[0] = CsiBandRange{0, 7};
    detector.configure(config);

    CsiMotionSnapshot snapshot{};
    for (uint16_t frame = 0; frame < config.baselineFrames; ++frame) {
        CsiPacket packet = makePacket(kWidth, 10);
        packet.firstWordInvalid = true;
        packet.buf[0] = static_cast<int8_t>(frame);
        packet.buf[2] = static_cast<int8_t>(-static_cast<int16_t>(frame));
        snapshot = detector.process(packet, kStartMs + frame);
    }
    TEST_ASSERT_TRUE(snapshot.baselineReady);

    CsiPacket invalidSpike = makePacket(kWidth, 10);
    invalidSpike.firstWordInvalid = true;
    invalidSpike.buf[0] = 100;
    invalidSpike.buf[2] = -100;
    snapshot = detector.process(invalidSpike, 2000);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::Monitoring),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.motion);
    TEST_ASSERT_EQUAL_UINT16(kWidth - 2, snapshot.validCarrierCount);
}

void test_hold_timer_handles_millis_wrap() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);

    CsiMotionSnapshot snapshot{};
    uint32_t now = 0xFFFFF000u;
    for (uint16_t i = 0; i < config.baselineFrames; ++i) {
        snapshot = detector.process(makePacket(kWidth, 10), now + i);
    }
    TEST_ASSERT_TRUE(snapshot.baselineReady);

    snapshot = detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 0xFFFFFFF0u);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::MotionCandidate),
        static_cast<uint8_t>(snapshot.state));

    snapshot = detector.process(makePacketWithBand(kWidth, 10, 10, 17, 20), 0x00000070u);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::MotionConfirmed),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_TRUE(snapshot.motion);
}

void test_auto_recalibration_is_time_based_not_frame_count_based() {
    CsiMotionConfig config = enabledConfig();
    config.autoRecalibration = true;
    config.minNoise = 10.0f;

    CsiBandMotionDetector fastDetector;
    CsiBandMotionDetector slowDetector;
    TEST_ASSERT_TRUE(fastDetector.begin());
    TEST_ASSERT_TRUE(slowDetector.begin());
    fastDetector.configure(config);
    slowDetector.configure(config);
    trainBaseline(fastDetector, config);
    trainBaseline(slowDetector, config);

    CsiMotionSnapshot fast{};
    CsiMotionSnapshot slow{};
    for (uint32_t now = 2000; now <= 152000; now += 100) {
        fast = fastDetector.process(makePacket(kWidth, 11), now);
    }
    for (uint32_t now = 2000; now <= 152000; now += 500) {
        slow = slowDetector.process(makePacket(kWidth, 11), now);
    }

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::Monitoring),
        static_cast<uint8_t>(fast.state));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::Monitoring),
        static_cast<uint8_t>(slow.state));
    TEST_ASSERT_FLOAT_WITHIN(0.15f, fast.score, slow.score);
}

void test_reset_reason_reports_manual_and_width_reset() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);

    auto snapshot = trainBaseline(detector, config);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(WIFISENSING::CSI::CsiMotionResetReason::WidthChange),
        static_cast<uint8_t>(snapshot.lastResetReason));

    detector.resetBaseline();
    snapshot = detector.snapshot();
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(WIFISENSING::CSI::CsiMotionResetReason::ManualCalibration),
        static_cast<uint8_t>(snapshot.lastResetReason));

    snapshot = detector.process(makePacket(80, 10), 4000);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(WIFISENSING::CSI::CsiMotionResetReason::WidthChange),
        static_cast<uint8_t>(snapshot.lastResetReason));
}

void test_manual_reset_snapshot_is_unknown_until_calibration_finishes() {
    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    const CsiMotionConfig config = enabledConfig();
    detector.configure(config);

    trainBaseline(detector, config);
    TEST_ASSERT_TRUE(detector.snapshot().decisionValid);

    detector.resetBaseline(WIFISENSING::CSI::CsiMotionResetReason::ManualCalibration);
    const CsiMotionSnapshot snapshot = detector.snapshot();
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::Calibrating),
        static_cast<uint8_t>(snapshot.state));
    TEST_ASSERT_FALSE(snapshot.decisionValid);
    TEST_ASSERT_FALSE(snapshot.motion);
    TEST_ASSERT_FALSE(snapshot.needsCalibration);
    TEST_ASSERT_FALSE(snapshot.baselineReady);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_disabled_returns_disabled_no_motion);
    RUN_TEST(test_storage_allocation_failure_returns_unavailable);
    RUN_TEST(test_no_bands_returns_needs_configuration);
    RUN_TEST(test_baseline_converges_after_configured_frames);
    RUN_TEST(test_narrow_band_motion_triggers_after_hold);
    RUN_TEST(test_single_frame_spike_does_not_trigger);
    RUN_TEST(test_motion_clears_after_clear_hold);
    RUN_TEST(test_visualization_bins_are_deterministic_and_return_to_baseline);
    RUN_TEST(test_global_noise_enters_noisy_environment);
    RUN_TEST(test_width_change_resets_baseline);
    RUN_TEST(test_dead_carriers_are_ignored);
    RUN_TEST(test_selected_band_only_detects_selected_band_changes);
    RUN_TEST(test_sensitivity_presets_override_manual_thresholds);
    RUN_TEST(test_global_gain_jump_does_not_confirm_motion_before_noisy_gate);
    RUN_TEST(test_confirmed_motion_is_not_cleared_by_broad_disturbance);
    RUN_TEST(test_stronger_local_motion_cannot_turn_into_noisy_clear);
    RUN_TEST(test_invalid_frame_cannot_bypass_motion_clear_hold);
    RUN_TEST(test_invalid_frame_breaks_motion_candidate_hold);
    RUN_TEST(test_invalid_frame_breaks_quiet_clear_hold);
    RUN_TEST(test_long_frame_gap_resets_candidate_instead_of_confirming);
    RUN_TEST(test_stale_state_preserves_confirmed_motion_as_unknown);
    RUN_TEST(test_source_change_resets_baseline_at_same_width);
    RUN_TEST(test_active_motion_requires_manual_calibration_after_width_change);
    RUN_TEST(test_retained_motion_cannot_be_cleared_by_startup_baseline);
    RUN_TEST(test_interleaved_foreign_source_does_not_reset_baseline);
    RUN_TEST(test_first_word_invalid_carriers_are_excluded);
    RUN_TEST(test_hold_timer_handles_millis_wrap);
    RUN_TEST(test_auto_recalibration_is_time_based_not_frame_count_based);
    RUN_TEST(test_reset_reason_reports_manual_and_width_reset);
    RUN_TEST(test_manual_reset_snapshot_is_unknown_until_calibration_finishes);
    return UNITY_END();
}
