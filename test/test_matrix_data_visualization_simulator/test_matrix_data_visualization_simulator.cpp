#include <unity.h>

#include <cstdio>
#include <cstdlib>

#include "../../lib/matrix_service/visualization/MatrixDataVisualizationEngine.cpp"

namespace {

struct FrameStats {
    uint32_t hash = 2166136261u;
    uint16_t lit = 0;
    uint32_t energy = 0;
};

MATRIX::MatrixDataVisualizationConfig configFor(MATRIX::MatrixDataVizMode mode) {
    MATRIX::MatrixDataVisualizationConfig config;
    config.enabled = true;
    config.mode = static_cast<uint8_t>(mode);
    config.minValue = 0.0f;
    config.maxValue = 100.0f;
    config.colorMin = 0x000040;
    config.colorMid = 0x00A060;
    config.colorMax = 0xFF3000;
    config.brightnessMin = 16;
    config.brightnessMax = 180;
    config.smoothing = 0;
    config.staleBehavior = static_cast<uint8_t>(MATRIX::MatrixDataStaleBehavior::Dim);
    return config;
}

MATRIX::MatrixDataVisualizationInput scalarInput(float value, uint32_t timestampMs) {
    MATRIX::MatrixDataVisualizationInput input;
    input.valid = true;
    input.value = value;
    input.timestampMs = timestampMs;
    return input;
}

MATRIX::MatrixDataVisualizationInput csiInput(bool inverted = false) {
    MATRIX::MatrixDataVisualizationInput input = scalarInput(50.0f, 1000);
    input.binCount = MATRIX::kMatrixDataVizPixelCount;
    for (uint8_t i = 0; i < MATRIX::kMatrixDataVizPixelCount; ++i) {
        input.bins[i] = inverted
            ? static_cast<uint8_t>(255u - i * 4u)
            : static_cast<uint8_t>(i * 4u);
    }
    return input;
}

FrameStats statsFor(const uint32_t* frame) {
    FrameStats stats;
    for (uint8_t i = 0; i < MATRIX::kMatrixDataVizPixelCount; ++i) {
        const uint32_t pixel = frame[i];
        stats.hash ^= pixel;
        stats.hash *= 16777619u;
        if (pixel != 0) {
            stats.lit++;
        }
        stats.energy += static_cast<uint8_t>((pixel >> 16) & 0xFFu);
        stats.energy += static_cast<uint8_t>((pixel >> 8) & 0xFFu);
        stats.energy += static_cast<uint8_t>(pixel & 0xFFu);
    }
    return stats;
}

uint8_t changedPixels(const uint32_t* a, const uint32_t* b) {
    uint8_t changed = 0;
    for (uint8_t i = 0; i < MATRIX::kMatrixDataVizPixelCount; ++i) {
        if (a[i] != b[i]) {
            changed++;
        }
    }
    return changed;
}

FrameStats renderFrame(const MATRIX::MatrixDataVisualizationConfig& config,
                       const MATRIX::MatrixDataVisualizationInput& input,
                       uint32_t nowMs,
                       uint32_t* outFrame) {
    MATRIX_VIZ::MatrixDataVisualizationEngine engine;
    engine.configure(config);
    engine.setInput(input);
    TEST_ASSERT_TRUE(engine.render(nowMs, outFrame, MATRIX::kMatrixDataVizPixelCount));
    return statsFor(outFrame);
}

uint16_t colorEnergy(uint32_t color) {
    return static_cast<uint16_t>(((color >> 16) & 0xFFu) + ((color >> 8) & 0xFFu) + (color & 0xFFu));
}

char shadeFor(uint32_t pixel, uint16_t maxEnergy) {
    static constexpr char kShades[] = " .:-=+*#%@";
    if (pixel == 0 || maxEnergy == 0) {
        return ' ';
    }
    const uint16_t energy = colorEnergy(pixel);
    const uint8_t index = static_cast<uint8_t>((static_cast<uint32_t>(energy) * 9u) / maxEnergy);
    return kShades[index > 9u ? 9u : index];
}

void printPreview(const char* title,
                  const MATRIX::MatrixDataVisualizationConfig& config,
                  const MATRIX::MatrixDataVisualizationInput& input,
                  uint32_t nowMs) {
    uint32_t frame[64] = {};
    MATRIX_VIZ::MatrixDataVisualizationEngine engine;
    engine.configure(config);
    engine.setInput(input);
    if (!engine.render(nowMs, frame, 64)) {
        std::printf("\n%s\n<render failed>\n", title);
        return;
    }

    const FrameStats stats = statsFor(frame);
    uint16_t maxPixelEnergy = 0;
    for (uint8_t i = 0; i < MATRIX::kMatrixDataVizPixelCount; ++i) {
        const uint16_t energy = colorEnergy(frame[i]);
        if (energy > maxPixelEnergy) {
            maxPixelEnergy = energy;
        }
    }
    std::printf("\n%s hash=%08lx lit=%u energy=%lu\n", title,
                static_cast<unsigned long>(stats.hash),
                static_cast<unsigned>(stats.lit),
                static_cast<unsigned long>(stats.energy));
    for (uint8_t y = 0; y < MATRIX::kMatrixDataVizHeight; ++y) {
        std::printf("  ");
        for (uint8_t x = 0; x < MATRIX::kMatrixDataVizWidth; ++x) {
            std::printf("%c%c", shadeFor(frame[y * MATRIX::kMatrixDataVizWidth + x], maxPixelEnergy), ' ');
        }
        std::printf("\n");
    }
}

void maybePrintPreviews() {
    const char* enabled = std::getenv("MATRIX_VIZ_PREVIEW");
    if (!enabled || enabled[0] == '\0' || enabled[0] == '0') {
        return;
    }

    printPreview(
        "CSI heatmap rising graph",
        configFor(MATRIX::MatrixDataVizMode::Heatmap),
        csiInput(false),
        100);
    printPreview(
        "CSI heatmap inverted graph",
        configFor(MATRIX::MatrixDataVizMode::Heatmap),
        csiInput(true),
        100);
    printPreview(
        "CSI spectrum rising graph",
        configFor(MATRIX::MatrixDataVizMode::SpectrumBars),
        csiInput(false),
        100);
    printPreview(
        "CSI spectrum inverted graph",
        configFor(MATRIX::MatrixDataVizMode::SpectrumBars),
        csiInput(true),
        100);
    printPreview(
        "Gauge low scalar",
        configFor(MATRIX::MatrixDataVizMode::Gauge),
        scalarInput(25.0f, 1000),
        100);
    printPreview(
        "Gauge high scalar",
        configFor(MATRIX::MatrixDataVizMode::Gauge),
        scalarInput(80.0f, 1000),
        100);
}

} // namespace

void setUp(void) {}
void tearDown(void) {}

void test_non_animated_modes_are_stable_for_constant_inputs() {
    const MATRIX::MatrixDataVizMode modes[] = {
        MATRIX::MatrixDataVizMode::Gauge,
        MATRIX::MatrixDataVizMode::Heatmap,
        MATRIX::MatrixDataVizMode::Trend,
        MATRIX::MatrixDataVizMode::SpectrumBars,
        MATRIX::MatrixDataVizMode::PerimeterMeter,
    };

    for (MATRIX::MatrixDataVizMode mode : modes) {
        MATRIX_VIZ::MatrixDataVisualizationEngine engine;
        engine.configure(configFor(mode));
        engine.setInput(mode == MATRIX::MatrixDataVizMode::Heatmap ? csiInput() : scalarInput(64.0f, 1000));

        uint32_t first[64] = {};
        uint32_t second[64] = {};
        TEST_ASSERT_TRUE(engine.render(100, first, 64));
        TEST_ASSERT_TRUE(engine.render(2400, second, 64));
        TEST_ASSERT_EQUAL_UINT8(0, changedPixels(first, second));
    }
}

void test_simulated_sources_render_non_empty_frames() {
    struct Scenario {
        MATRIX::MatrixDataVizMode mode;
        MATRIX::MatrixDataVisualizationInput input;
    };

    Scenario scenarios[] = {
        {MATRIX::MatrixDataVizMode::Gauge, scalarInput(800.0f, 1000)},
        {MATRIX::MatrixDataVizMode::Heatmap, scalarInput(45.0f, 1000)},
        {MATRIX::MatrixDataVizMode::Trend, scalarInput(70.0f, 1000)},
        {MATRIX::MatrixDataVizMode::SpectrumBars, csiInput()},
    };

    scenarios[0].input.value = 40.0f; // SCD4x CO2 normalized by this harness range.
    scenarios[1].input.value = 23.0f; // BLE thermometer style scalar.
    scenarios[2].input.value = 72.0f; // WiFi RSSI quality style scalar.

    for (const auto& scenario : scenarios) {
        uint32_t frame[64] = {};
        const FrameStats stats = renderFrame(configFor(scenario.mode), scenario.input, 100, frame);
        TEST_ASSERT_GREATER_THAN_UINT16(0, stats.lit);
        TEST_ASSERT_GREATER_THAN_UINT32(0, stats.energy);
    }
}

void test_changing_scalar_input_changes_frame_predictably() {
    uint32_t lowFrame[64] = {};
    uint32_t highFrame[64] = {};
    const auto config = configFor(MATRIX::MatrixDataVizMode::Gauge);

    const FrameStats low = renderFrame(config, scalarInput(20.0f, 1000), 100, lowFrame);
    const FrameStats high = renderFrame(config, scalarInput(85.0f, 1000), 100, highFrame);

    TEST_ASSERT_GREATER_THAN_UINT8(10, changedPixels(lowFrame, highFrame));
    TEST_ASSERT_GREATER_THAN_UINT16(low.lit, high.lit);
    TEST_ASSERT_GREATER_THAN_UINT32(low.energy, high.energy);
}

void test_stale_behaviors_have_distinct_frame_outputs() {
    auto input = csiInput();
    input.stale = true;

    uint32_t dimFrame[64] = {};
    auto dimConfig = configFor(MATRIX::MatrixDataVizMode::Heatmap);
    dimConfig.staleBehavior = static_cast<uint8_t>(MATRIX::MatrixDataStaleBehavior::Dim);
    const FrameStats dim = renderFrame(dimConfig, input, 100, dimFrame);

    uint32_t grayFrame[64] = {};
    auto grayConfig = dimConfig;
    grayConfig.staleBehavior = static_cast<uint8_t>(MATRIX::MatrixDataStaleBehavior::Gray);
    const FrameStats gray = renderFrame(grayConfig, input, 100, grayFrame);

    uint32_t blankFrame[64] = {};
    auto blankConfig = dimConfig;
    blankConfig.staleBehavior = static_cast<uint8_t>(MATRIX::MatrixDataStaleBehavior::Blank);
    const FrameStats blank = renderFrame(blankConfig, input, 100, blankFrame);

    TEST_ASSERT_GREATER_THAN_UINT16(0, dim.lit);
    TEST_ASSERT_GREATER_THAN_UINT16(0, gray.lit);
    TEST_ASSERT_EQUAL_UINT16(0, blank.lit);
    TEST_ASSERT_NOT_EQUAL(dim.hash, gray.hash);
    TEST_ASSERT_NOT_EQUAL(gray.hash, blank.hash);
}

void test_csi_bins_map_stably_and_shape_changes_are_visible() {
    uint32_t first[64] = {};
    uint32_t same[64] = {};
    uint32_t inverted[64] = {};
    const auto config = configFor(MATRIX::MatrixDataVizMode::SpectrumBars);

    const FrameStats firstStats = renderFrame(config, csiInput(false), 100, first);
    const FrameStats sameStats = renderFrame(config, csiInput(false), 2400, same);
    const FrameStats invertedStats = renderFrame(config, csiInput(true), 100, inverted);

    TEST_ASSERT_EQUAL_UINT32(firstStats.hash, sameStats.hash);
    TEST_ASSERT_GREATER_THAN_UINT8(0, changedPixels(first, inverted));
    TEST_ASSERT_NOT_EQUAL(firstStats.hash, invertedStats.hash);
}

void test_brightness_and_gradient_are_monotonic() {
    const uint32_t low = MATRIX_VIZ::MatrixDataVisualizationEngine::gradientColor(
        0x000000, 0x404040, 0xFFFFFF, 0);
    const uint32_t mid = MATRIX_VIZ::MatrixDataVisualizationEngine::gradientColor(
        0x000000, 0x404040, 0xFFFFFF, 127);
    const uint32_t high = MATRIX_VIZ::MatrixDataVisualizationEngine::gradientColor(
        0x000000, 0x404040, 0xFFFFFF, 255);

    TEST_ASSERT_GREATER_THAN_UINT16(colorEnergy(low), colorEnergy(mid));
    TEST_ASSERT_GREATER_THAN_UINT16(colorEnergy(mid), colorEnergy(high));

    const uint32_t dim = MATRIX_VIZ::MatrixDataVisualizationEngine::scaleColor(0x808080, 32);
    const uint32_t bright = MATRIX_VIZ::MatrixDataVisualizationEngine::scaleColor(0x808080, 192);
    TEST_ASSERT_GREATER_THAN_UINT16(colorEnergy(dim), colorEnergy(bright));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    maybePrintPreviews();
    RUN_TEST(test_non_animated_modes_are_stable_for_constant_inputs);
    RUN_TEST(test_simulated_sources_render_non_empty_frames);
    RUN_TEST(test_changing_scalar_input_changes_frame_predictably);
    RUN_TEST(test_stale_behaviors_have_distinct_frame_outputs);
    RUN_TEST(test_csi_bins_map_stably_and_shape_changes_are_visible);
    RUN_TEST(test_brightness_and_gradient_are_monotonic);
    return UNITY_END();
}
