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
    RUN_TEST(test_reset_reason_reports_manual_and_width_reset);
    return UNITY_END();
}
