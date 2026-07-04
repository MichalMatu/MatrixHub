#include <unity.h>

#include "../../lib/matrix_service/visualization/MatrixDataVisualizationEngine.cpp"

namespace {

MATRIX::MatrixDataVisualizationConfig baseConfig() {
    MATRIX::MatrixDataVisualizationConfig config;
    config.enabled = true;
    config.minValue = 0.0f;
    config.maxValue = 100.0f;
    config.colorMin = 0x0000FF;
    config.colorMid = 0x00FF00;
    config.colorMax = 0xFF0000;
    config.brightnessMin = 16;
    config.brightnessMax = 160;
    config.smoothing = 0;
    config.staleBehavior = static_cast<uint8_t>(MATRIX::MatrixDataStaleBehavior::Dim);
    return config;
}

uint8_t countLit(const uint32_t* frame) {
    uint8_t lit = 0;
    for (uint8_t i = 0; i < 64; ++i) {
        if (frame[i] != 0) {
            lit++;
        }
    }
    return lit;
}

uint16_t colorEnergy(uint32_t color) {
    return static_cast<uint16_t>(((color >> 16) & 0xFFu) + ((color >> 8) & 0xFFu) + (color & 0xFFu));
}

void assertFramesEqual(const uint32_t* expected, const uint32_t* actual) {
    for (uint8_t i = 0; i < 64; ++i) {
        TEST_ASSERT_EQUAL_HEX32(expected[i], actual[i]);
    }
}

}  // namespace

void setUp(void) {}
void tearDown(void) {}

void test_normalize_value_clamps_to_byte_range() {
    TEST_ASSERT_EQUAL_UINT8(0, MATRIX_VIZ::MatrixDataVisualizationEngine::normalizeValue(-10.0f, 0.0f, 100.0f));
    TEST_ASSERT_EQUAL_UINT8(128, MATRIX_VIZ::MatrixDataVisualizationEngine::normalizeValue(50.0f, 0.0f, 100.0f));
    TEST_ASSERT_EQUAL_UINT8(255, MATRIX_VIZ::MatrixDataVisualizationEngine::normalizeValue(150.0f, 0.0f, 100.0f));
}

void test_gauge_renders_expected_number_of_pixels() {
    MATRIX_VIZ::MatrixDataVisualizationEngine engine;
    auto config = baseConfig();
    config.mode = static_cast<uint8_t>(MATRIX::MatrixDataVizMode::Gauge);
    engine.configure(config);

    MATRIX::MatrixDataVisualizationInput input;
    input.valid = true;
    input.value = 50.0f;
    input.timestampMs = 1;
    engine.setInput(input);

    uint32_t frame[64] = {};
    TEST_ASSERT_TRUE(engine.render(100, frame, 64));

    uint8_t lit = 0;
    for (uint32_t pixel : frame) {
        if (pixel != 0) {
            lit++;
        }
    }
    TEST_ASSERT_EQUAL_UINT8(32, lit);
}

void test_stale_blank_behavior_turns_frame_off() {
    MATRIX_VIZ::MatrixDataVisualizationEngine engine;
    auto config = baseConfig();
    config.staleBehavior = static_cast<uint8_t>(MATRIX::MatrixDataStaleBehavior::Blank);
    engine.configure(config);

    MATRIX::MatrixDataVisualizationInput input;
    input.valid = false;
    input.stale = true;
    engine.setInput(input);

    uint32_t frame[64] = {};
    TEST_ASSERT_TRUE(engine.render(100, frame, 64));
    for (uint32_t pixel : frame) {
        TEST_ASSERT_EQUAL_HEX32(0x000000, pixel);
    }
}

void test_heatmap_is_deterministic_for_same_bins() {
    MATRIX_VIZ::MatrixDataVisualizationEngine engine;
    auto config = baseConfig();
    config.mode = static_cast<uint8_t>(MATRIX::MatrixDataVizMode::Heatmap);
    engine.configure(config);

    MATRIX::MatrixDataVisualizationInput input;
    input.valid = true;
    input.value = 25.0f;
    input.timestampMs = 1;
    input.binCount = 64;
    for (uint8_t i = 0; i < 64; ++i) {
        input.bins[i] = static_cast<uint8_t>(i * 4);
    }
    engine.setInput(input);

    uint32_t frameA[64] = {};
    uint32_t frameB[64] = {};
    TEST_ASSERT_TRUE(engine.render(100, frameA, 64));
    TEST_ASSERT_TRUE(engine.render(100, frameB, 64));
    for (uint8_t i = 0; i < 64; ++i) {
        TEST_ASSERT_EQUAL_HEX32(frameA[i], frameB[i]);
    }
}

void test_spectrum_bars_scalar_fallback_is_deterministic() {
    MATRIX_VIZ::MatrixDataVisualizationEngine engine;
    auto config = baseConfig();
    config.mode = static_cast<uint8_t>(MATRIX::MatrixDataVizMode::SpectrumBars);
    engine.configure(config);

    MATRIX::MatrixDataVisualizationInput input;
    input.valid = true;
    input.value = 62.0f;
    input.timestampMs = 1;
    engine.setInput(input);

    uint32_t frameA[64] = {};
    uint32_t frameB[64] = {};
    TEST_ASSERT_TRUE(engine.render(100, frameA, 64));
    TEST_ASSERT_TRUE(engine.render(2400, frameB, 64));
    assertFramesEqual(frameA, frameB);
}

void test_perimeter_meter_is_deterministic_for_same_value() {
    MATRIX_VIZ::MatrixDataVisualizationEngine engine;
    auto config = baseConfig();
    config.mode = static_cast<uint8_t>(MATRIX::MatrixDataVizMode::PerimeterMeter);
    engine.configure(config);

    MATRIX::MatrixDataVisualizationInput input;
    input.valid = true;
    input.value = 74.0f;
    input.timestampMs = 1;
    engine.setInput(input);

    uint32_t frameA[64] = {};
    uint32_t frameB[64] = {};
    TEST_ASSERT_TRUE(engine.render(100, frameA, 64));
    TEST_ASSERT_TRUE(engine.render(2400, frameB, 64));
    assertFramesEqual(frameA, frameB);
}

void test_each_visualization_mode_renders_non_empty_frame() {
    for (uint8_t mode = 0; mode <= static_cast<uint8_t>(MATRIX::MatrixDataVizMode::Pulse); ++mode) {
        MATRIX_VIZ::MatrixDataVisualizationEngine engine;
        auto config = baseConfig();
        config.mode = mode;
        engine.configure(config);

        MATRIX::MatrixDataVisualizationInput input;
        input.valid = true;
        input.value = 65.0f;
        input.timestampMs = 100 + mode;
        input.binCount = 64;
        for (uint8_t i = 0; i < 64; ++i) {
            input.bins[i] = static_cast<uint8_t>((i * 7u + mode * 11u) & 0xFFu);
        }
        engine.setInput(input);

        uint32_t frame[64] = {};
        TEST_ASSERT_TRUE(engine.render(1000 + mode * 17u, frame, 64));
        TEST_ASSERT_GREATER_THAN_UINT8(0, countLit(frame));
    }
}

void test_stale_dim_behavior_dims_heatmap_bins() {
    MATRIX::MatrixDataVisualizationInput input;
    input.valid = true;
    input.stale = true;
    input.value = 100.0f;
    input.timestampMs = 1;
    input.binCount = 64;
    for (uint8_t i = 0; i < 64; ++i) {
        input.bins[i] = 255;
    }

    MATRIX_VIZ::MatrixDataVisualizationEngine staleEngine;
    auto staleConfig = baseConfig();
    staleConfig.mode = static_cast<uint8_t>(MATRIX::MatrixDataVizMode::Heatmap);
    staleConfig.staleBehavior = static_cast<uint8_t>(MATRIX::MatrixDataStaleBehavior::Dim);
    staleEngine.configure(staleConfig);
    staleEngine.setInput(input);

    uint32_t staleFrame[64] = {};
    TEST_ASSERT_TRUE(staleEngine.render(100, staleFrame, 64));

    input.stale = false;
    MATRIX_VIZ::MatrixDataVisualizationEngine freshEngine;
    auto freshConfig = baseConfig();
    freshConfig.mode = static_cast<uint8_t>(MATRIX::MatrixDataVizMode::Heatmap);
    freshEngine.configure(freshConfig);
    freshEngine.setInput(input);

    uint32_t freshFrame[64] = {};
    TEST_ASSERT_TRUE(freshEngine.render(100, freshFrame, 64));

    TEST_ASSERT_GREATER_THAN_UINT16(colorEnergy(staleFrame[0]) * 2u, colorEnergy(freshFrame[0]));
    TEST_ASSERT_GREATER_THAN_UINT16(0, colorEnergy(staleFrame[0]));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_normalize_value_clamps_to_byte_range);
    RUN_TEST(test_gauge_renders_expected_number_of_pixels);
    RUN_TEST(test_stale_blank_behavior_turns_frame_off);
    RUN_TEST(test_heatmap_is_deterministic_for_same_bins);
    RUN_TEST(test_spectrum_bars_scalar_fallback_is_deterministic);
    RUN_TEST(test_perimeter_meter_is_deterministic_for_same_value);
    RUN_TEST(test_each_visualization_mode_renders_non_empty_frame);
    RUN_TEST(test_stale_dim_behavior_dims_heatmap_bins);
    return UNITY_END();
}
