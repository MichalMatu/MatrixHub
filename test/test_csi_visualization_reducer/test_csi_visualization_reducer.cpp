#include <unity.h>

#include <cstring>

#include "../../src/wifisensing/csi/algo/CsiVisualizationReducer.cpp"

using WIFISENSING::CSI::CSI_VISUALIZATION_BIN_COUNT;
using WIFISENSING::CSI::CsiPacket;
using WIFISENSING::CSI::CsiVisualizationReducer;

namespace {

CsiPacket makeGraphPacket(uint16_t width, bool inverted = false) {
    CsiPacket packet{};
    packet.len = static_cast<size_t>(width) * 2u;
    packet.compensate_gain = 1.0f;
    for (uint16_t i = 0; i < width; ++i) {
        const uint16_t position = inverted ? static_cast<uint16_t>(width - 1u - i) : i;
        const int8_t amplitude = static_cast<int8_t>(4 + ((position * 40u) / width));
        packet.buf[2u * i] = amplitude;
        packet.buf[(2u * i) + 1u] = 0;
    }
    return packet;
}

CsiPacket makeJitteredGraphPacket(uint16_t width) {
    CsiPacket packet = makeGraphPacket(width);
    for (uint16_t i = 0; i < width; ++i) {
        if ((i % 11u) == 0u && packet.buf[2u * i] < 120) {
            packet.buf[2u * i] = static_cast<int8_t>(packet.buf[2u * i] + 1);
        }
    }
    return packet;
}

uint16_t nonzeroBins(const uint8_t* bins) {
    uint16_t count = 0;
    for (uint8_t i = 0; i < CSI_VISUALIZATION_BIN_COUNT; ++i) {
        if (bins[i] != 0) {
            count++;
        }
    }
    return count;
}

uint16_t changedBins(const uint8_t* a, const uint8_t* b) {
    uint16_t changed = 0;
    for (uint8_t i = 0; i < CSI_VISUALIZATION_BIN_COUNT; ++i) {
        if (a[i] != b[i]) {
            changed++;
        }
    }
    return changed;
}

} // namespace

void setUp(void) {}
void tearDown(void) {}

void test_process_packet_publishes_visualization_without_baseline() {
    CsiVisualizationReducer reducer;
    const CsiPacket packet = makeGraphPacket(128);

    const auto snapshot = reducer.process(packet, 1234);

    TEST_ASSERT_TRUE(snapshot.valid);
    TEST_ASSERT_FALSE(snapshot.stale);
    TEST_ASSERT_EQUAL_UINT32(1234, snapshot.timestampMs);
    TEST_ASSERT_EQUAL_UINT16(128, snapshot.width);
    TEST_ASSERT_EQUAL_UINT8(CSI_VISUALIZATION_BIN_COUNT, snapshot.binCount);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, snapshot.value);
    TEST_ASSERT_GREATER_THAN_UINT16(0, nonzeroBins(snapshot.bins));
}

void test_same_graph_renders_same_bins() {
    CsiVisualizationReducer first;
    CsiVisualizationReducer second;
    const CsiPacket packet = makeGraphPacket(128);

    const auto a = first.process(packet, 100);
    const auto b = second.process(packet, 200);

    TEST_ASSERT_TRUE(a.valid);
    TEST_ASSERT_TRUE(b.valid);
    TEST_ASSERT_EQUAL_MEMORY(a.bins, b.bins, sizeof(a.bins));
    TEST_ASSERT_EQUAL_FLOAT(a.value, b.value);
}

void test_smoothing_limits_single_frame_jump() {
    CsiVisualizationReducer reducer;
    const auto initial = reducer.process(makeGraphPacket(128), 100);
    const auto smoothed = reducer.process(makeGraphPacket(128, true), 200);

    TEST_ASSERT_TRUE(initial.valid);
    TEST_ASSERT_TRUE(smoothed.valid);
    TEST_ASSERT_GREATER_THAN_UINT16(0, changedBins(initial.bins, smoothed.bins));
    TEST_ASSERT_LESS_THAN_UINT8(255, smoothed.bins[0]);
    TEST_ASSERT_GREATER_THAN_UINT8(0, smoothed.bins[CSI_VISUALIZATION_BIN_COUNT - 1]);
}

void test_small_csi_jitter_is_muted() {
    CsiVisualizationReducer reducer;
    reducer.process(makeGraphPacket(128), 100);
    const auto quiet = reducer.process(makeJitteredGraphPacket(128), 200);

    TEST_ASSERT_TRUE(quiet.valid);
    TEST_ASSERT_LESS_THAN_FLOAT(12.0f, quiet.value);
}

void test_large_csi_shape_change_raises_motion_value() {
    CsiVisualizationReducer reducer;
    reducer.process(makeGraphPacket(128), 100);
    const auto steady = reducer.process(makeGraphPacket(128), 200);
    const auto moved = reducer.process(makeGraphPacket(128, true), 300);

    TEST_ASSERT_TRUE(steady.valid);
    TEST_ASSERT_TRUE(moved.valid);
    TEST_ASSERT_LESS_THAN_FLOAT(1.0f, steady.value);
    TEST_ASSERT_GREATER_THAN_FLOAT(steady.value + 10.0f, moved.value);
}

void test_invalid_packet_resets_to_stale_snapshot() {
    CsiVisualizationReducer reducer;
    reducer.process(makeGraphPacket(64), 100);

    CsiPacket invalid{};
    invalid.len = 0;
    const auto snapshot = reducer.process(invalid, 200);

    TEST_ASSERT_FALSE(snapshot.valid);
    TEST_ASSERT_TRUE(snapshot.stale);
    TEST_ASSERT_EQUAL_UINT32(200, snapshot.timestampMs);
    TEST_ASSERT_EQUAL_UINT8(0, snapshot.binCount);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_process_packet_publishes_visualization_without_baseline);
    RUN_TEST(test_same_graph_renders_same_bins);
    RUN_TEST(test_smoothing_limits_single_frame_jump);
    RUN_TEST(test_small_csi_jitter_is_muted);
    RUN_TEST(test_large_csi_shape_change_raises_motion_value);
    RUN_TEST(test_invalid_packet_resets_to_stale_snapshot);
    return UNITY_END();
}
