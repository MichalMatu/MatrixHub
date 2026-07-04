#include <unity.h>

#include "../../lib/matrix_service/effects/MatrixFxEngine3D.cpp"

namespace {

MATRIX_FX::MatrixFxConfig makeConfig(uint8_t mode) {
    MATRIX_FX::MatrixFxConfig config;
    config.mode = mode;
    config.speedMs = 900;
    config.color1 = 0x1030FF;
    config.color2 = 0xFF3050;
    config.color3 = 0x20FF80;
    config.provider = MATRIX_FX::ReactiveProvider::Imu;
    config.reactivityGain = 120;
    return config;
}

uint32_t countLitPixels(const uint32_t* frame) {
    uint32_t count = 0;
    for (uint8_t i = 0; i < MATRIX_FX::kMatrixFxPixelCount; ++i) {
        if ((frame[i] & 0x00FFFFFFu) != 0) {
            count++;
        }
    }
    return count;
}

uint32_t countChangedPixels(const uint32_t* a, const uint32_t* b) {
    uint32_t count = 0;
    for (uint8_t i = 0; i < MATRIX_FX::kMatrixFxPixelCount; ++i) {
        if ((a[i] & 0x00FFFFFFu) != (b[i] & 0x00FFFFFFu)) {
            count++;
        }
    }
    return count;
}

uint32_t countPixelsAboveBrightness(const uint32_t* frame, uint8_t threshold);

uint8_t brightness(uint32_t color) {
    const uint8_t r = static_cast<uint8_t>((color >> 16) & 0xFF);
    const uint8_t g = static_cast<uint8_t>((color >> 8) & 0xFF);
    const uint8_t b = static_cast<uint8_t>(color & 0xFF);
    return static_cast<uint8_t>((static_cast<uint16_t>(r) + g + b) / 3);
}

uint32_t countPixelsAboveBrightness(const uint32_t* frame, uint8_t threshold) {
    uint32_t count = 0;
    for (uint8_t i = 0; i < MATRIX_FX::kMatrixFxPixelCount; ++i) {
        if (brightness(frame[i]) > threshold) {
            count++;
        }
    }
    return count;
}

uint32_t countUniqueColors(const uint32_t* frame) {
    uint32_t unique = 0;
    uint32_t seen[MATRIX_FX::kMatrixFxPixelCount]{};
    for (uint8_t i = 0; i < MATRIX_FX::kMatrixFxPixelCount; ++i) {
        const uint32_t color = frame[i] & 0x00FFFFFFu;
        bool exists = false;
        for (uint8_t j = 0; j < unique; ++j) {
            if (seen[j] == color) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            seen[unique++] = color;
        }
    }
    return unique;
}

struct TemporalAudit {
    float avgBrightnessDelta = 0.0f;
    float hardPixelRate = 0.0f;
    float onOffRate = 0.0f;
    uint8_t minLit = MATRIX_FX::kMatrixFxPixelCount;
};

TemporalAudit measureTemporalSmoothness(uint8_t mode) {
    MATRIX_FX::MatrixFxEngine3D engine;
    auto config = makeConfig(mode);
    config.speedMs = 1000;
    config.reactivityGain = 95;
    engine.configure(config);

    MATRIX_FX::MatrixFxInput input;
    input.imuValid = true;
    input.ax = -0.72f;
    input.ay = 0.58f;
    input.az = 0.38f;
    input.motionEnergy = 0.45f;
    engine.setInput(input);

    uint32_t current[MATRIX_FX::kMatrixFxPixelCount]{};
    uint32_t previous[MATRIX_FX::kMatrixFxPixelCount]{};
    for (uint8_t warmup = 0; warmup < 8; ++warmup) {
        TEST_ASSERT_TRUE(engine.render(1000 + warmup * 33, current, MATRIX_FX::kMatrixFxPixelCount));
    }

    TemporalAudit metrics;
    uint32_t totalBrightnessDelta = 0;
    uint32_t hardPixels = 0;
    uint32_t onOffTransitions = 0;
    constexpr uint8_t frameCount = 48;

    for (uint8_t frame = 0; frame < frameCount; ++frame) {
        TEST_ASSERT_TRUE(engine.render(1000 + (frame + 8) * 33, current, MATRIX_FX::kMatrixFxPixelCount));
        metrics.minLit = std::min<uint8_t>(metrics.minLit, static_cast<uint8_t>(countLitPixels(current)));

        if (frame > 0) {
            for (uint8_t pixel = 0; pixel < MATRIX_FX::kMatrixFxPixelCount; ++pixel) {
                const uint8_t previousBrightness = brightness(previous[pixel]);
                const uint8_t currentBrightness = brightness(current[pixel]);
                const uint8_t delta = previousBrightness > currentBrightness
                    ? previousBrightness - currentBrightness
                    : currentBrightness - previousBrightness;
                totalBrightnessDelta += delta;
                if (delta > 70) {
                    hardPixels++;
                }
                if ((previousBrightness <= 10 && currentBrightness >= 42) ||
                    (previousBrightness >= 42 && currentBrightness <= 10)) {
                    onOffTransitions++;
                }
            }
        }

        for (uint8_t pixel = 0; pixel < MATRIX_FX::kMatrixFxPixelCount; ++pixel) {
            previous[pixel] = current[pixel];
        }
    }

    const float sampleCount = static_cast<float>((frameCount - 1) * MATRIX_FX::kMatrixFxPixelCount);
    metrics.avgBrightnessDelta = static_cast<float>(totalBrightnessDelta) / sampleCount;
    metrics.hardPixelRate = static_cast<float>(hardPixels) / sampleCount;
    metrics.onOffRate = static_cast<float>(onOffTransitions) / sampleCount;
    return metrics;
}

void renderSettledFrame(uint8_t mode,
                        MATRIX_FX::MatrixFxConfig config,
                        const MATRIX_FX::MatrixFxInput& input,
                        uint32_t* frame) {
    config.mode = mode;
    MATRIX_FX::MatrixFxEngine3D engine;
    engine.configure(config);
    engine.setInput(input);
    for (uint8_t step = 0; step < 8; ++step) {
        TEST_ASSERT_TRUE(engine.render(1000 + step * 33, frame, MATRIX_FX::kMatrixFxPixelCount));
    }
}

float weightedY(const uint32_t* frame) {
    float sum = 0.0f;
    float weight = 0.0f;
    for (uint8_t y = 0; y < MATRIX_FX::kMatrixFxHeight; ++y) {
        for (uint8_t x = 0; x < MATRIX_FX::kMatrixFxWidth; ++x) {
            const float b = static_cast<float>(brightness(frame[y * MATRIX_FX::kMatrixFxWidth + x]));
            sum += static_cast<float>(y) * b;
            weight += b;
        }
    }
    return weight > 0.0f ? sum / weight : 0.0f;
}

}  // namespace

void setUp(void) {}
void tearDown(void) {}

void test_engine_rejects_render_before_configure() {
    MATRIX_FX::MatrixFxEngine3D engine;
    uint32_t frame[MATRIX_FX::kMatrixFxPixelCount]{};

    TEST_ASSERT_FALSE(engine.render(100, frame, MATRIX_FX::kMatrixFxPixelCount));
}

void test_engine_renders_all_native_modes_to_nonblank_frames() {
    for (uint8_t mode = 0; mode <= MATRIX_FX::kNative3DModeMax; ++mode) {
        MATRIX_FX::MatrixFxEngine3D engine;
        uint32_t frame[MATRIX_FX::kMatrixFxPixelCount]{};

        engine.configure(makeConfig(mode));

        TEST_ASSERT_TRUE(engine.render(1000 + mode * 40, frame, MATRIX_FX::kMatrixFxPixelCount));
        TEST_ASSERT_GREATER_THAN_UINT32(0, countLitPixels(frame));
    }
}

bool isClassicIridescentMode(uint8_t mode) {
    return mode == static_cast<uint8_t>(MATRIX_FX::Native3DMode::CenterRipple);
}

void test_render_audit_all_native_modes_are_varied() {
    MATRIX_FX::MatrixFxInput input;
    input.imuValid = true;
    input.ax = 0.42f;
    input.ay = -0.28f;
    input.az = 0.85f;
    input.motionEnergy = 0.25f;

    for (uint8_t mode = 0; mode <= MATRIX_FX::kNative3DModeMax; ++mode) {
        auto cool = makeConfig(mode);
        cool.color1 = 0x0030FF;
        cool.color2 = 0x00FF90;
        cool.color3 = 0xB0F0FF;

        auto warm = makeConfig(mode);
        warm.color1 = 0x400020;
        warm.color2 = 0xFF5030;
        warm.color3 = 0xFFD060;

        uint32_t coolFrame[MATRIX_FX::kMatrixFxPixelCount]{};
        uint32_t warmFrame[MATRIX_FX::kMatrixFxPixelCount]{};
        renderSettledFrame(mode, cool, input, coolFrame);
        renderSettledFrame(mode, warm, input, warmFrame);

        TEST_ASSERT_GREATER_THAN_UINT32(4, countLitPixels(coolFrame));
        TEST_ASSERT_GREATER_THAN_UINT32(3, countUniqueColors(coolFrame));
        if (!isClassicIridescentMode(mode)) {
            TEST_ASSERT_GREATER_THAN_UINT32(4, countChangedPixels(coolFrame, warmFrame));
        }
    }
}

void test_render_audit_all_native_modes_react_to_imu_input() {
    MATRIX_FX::MatrixFxInput idle;

    MATRIX_FX::MatrixFxInput tilted;
    tilted.imuValid = true;
    tilted.ax = -0.72f;
    tilted.ay = 0.58f;
    tilted.az = 0.38f;
    tilted.gx = 90.0f;
    tilted.gy = -55.0f;
    tilted.gz = 40.0f;
    tilted.motionEnergy = 0.45f;

    for (uint8_t mode = 0; mode <= MATRIX_FX::kNative3DModeMax; ++mode) {
        const auto config = makeConfig(mode);
        uint32_t idleFrame[MATRIX_FX::kMatrixFxPixelCount]{};
        uint32_t tiltedFrame[MATRIX_FX::kMatrixFxPixelCount]{};
        renderSettledFrame(mode, config, idle, idleFrame);
        renderSettledFrame(mode, config, tilted, tiltedFrame);

        TEST_ASSERT_GREATER_THAN_UINT32(0, countChangedPixels(idleFrame, tiltedFrame));
    }
}

void test_render_audit_all_native_modes_have_smooth_temporal_transitions() {
    for (uint8_t mode = 0; mode <= MATRIX_FX::kNative3DModeMax; ++mode) {
        const TemporalAudit metrics = measureTemporalSmoothness(mode);
        TEST_ASSERT_GREATER_THAN_UINT32(0, metrics.minLit);
        TEST_ASSERT_TRUE_MESSAGE(metrics.avgBrightnessDelta <= 34.0f, "effect should not jump too much between frames");
        TEST_ASSERT_TRUE_MESSAGE(metrics.hardPixelRate <= 0.10f, "effect should not have many hard pixel jumps");
        TEST_ASSERT_TRUE_MESSAGE(metrics.onOffRate <= 0.035f, "effect should not flicker pixels on/off");
    }
}

void test_engine_clamps_config_and_keeps_small_runtime_state() {
    MATRIX_FX::MatrixFxEngine3D engine;
    uint32_t frame[MATRIX_FX::kMatrixFxPixelCount]{};
    auto config = makeConfig(255);
    config.provider = static_cast<MATRIX_FX::ReactiveProvider>(255);
    config.reactivityGain = 255;

    engine.configure(config);

    TEST_ASSERT_TRUE(engine.render(1000, frame, MATRIX_FX::kMatrixFxPixelCount));
    TEST_ASSERT_GREATER_THAN_UINT32(0, countLitPixels(frame));
    TEST_ASSERT_LESS_THAN_UINT32(512, static_cast<uint32_t>(sizeof(MATRIX_FX::MatrixFxEngine3D)));
}

void test_imu_input_changes_projected_frame() {
    const auto config = makeConfig(static_cast<uint8_t>(MATRIX_FX::Native3DMode::DepthTunnel));
    uint32_t idleFrame[MATRIX_FX::kMatrixFxPixelCount]{};
    uint32_t reactiveFrame[MATRIX_FX::kMatrixFxPixelCount]{};

    MATRIX_FX::MatrixFxEngine3D idle;
    idle.configure(config);
    TEST_ASSERT_TRUE(idle.render(1000, idleFrame, MATRIX_FX::kMatrixFxPixelCount));

    MATRIX_FX::MatrixFxEngine3D reactive;
    reactive.configure(config);
    MATRIX_FX::MatrixFxInput input;
    input.imuValid = true;
    input.ax = 0.85f;
    input.ay = -0.65f;
    input.az = 0.40f;
    input.gx = 120.0f;
    input.gy = -80.0f;
    input.gz = 35.0f;
    reactive.setInput(input);
    TEST_ASSERT_TRUE(reactive.render(1000, reactiveFrame, MATRIX_FX::kMatrixFxPixelCount));

    TEST_ASSERT_GREATER_THAN_UINT32(0, countChangedPixels(idleFrame, reactiveFrame));
}

void test_iridescent_ripple_reacts_to_tilted_center_and_hue() {
    const auto config = makeConfig(static_cast<uint8_t>(MATRIX_FX::Native3DMode::CenterRipple));
    uint32_t idleFrame[MATRIX_FX::kMatrixFxPixelCount]{};
    uint32_t tiltedFrame[MATRIX_FX::kMatrixFxPixelCount]{};

    MATRIX_FX::MatrixFxEngine3D idle;
    idle.configure(config);
    TEST_ASSERT_TRUE(idle.render(1000, idleFrame, MATRIX_FX::kMatrixFxPixelCount));

    MATRIX_FX::MatrixFxEngine3D tilted;
    tilted.configure(config);
    MATRIX_FX::MatrixFxInput input;
    input.imuValid = true;
    input.ax = 0.80f;
    input.ay = -0.55f;
    input.motionEnergy = 0.40f;
    tilted.setInput(input);
    TEST_ASSERT_TRUE(tilted.render(1000, tiltedFrame, MATRIX_FX::kMatrixFxPixelCount));

    TEST_ASSERT_GREATER_THAN_UINT32(4, countChangedPixels(idleFrame, tiltedFrame));
}

void test_liquid_wave_reacts_to_imu_tilt() {
    const auto config = makeConfig(static_cast<uint8_t>(MATRIX_FX::Native3DMode::LiquidWave));
    uint32_t idleFrame[MATRIX_FX::kMatrixFxPixelCount]{};
    uint32_t tiltedFrame[MATRIX_FX::kMatrixFxPixelCount]{};

    MATRIX_FX::MatrixFxEngine3D idle;
    idle.configure(config);
    TEST_ASSERT_TRUE(idle.render(1000, idleFrame, MATRIX_FX::kMatrixFxPixelCount));

    MATRIX_FX::MatrixFxEngine3D tilted;
    tilted.configure(config);
    MATRIX_FX::MatrixFxInput input;
    input.imuValid = true;
    input.ax = -0.70f;
    input.ay = 0.65f;
    tilted.setInput(input);
    TEST_ASSERT_TRUE(tilted.render(1000, tiltedFrame, MATRIX_FX::kMatrixFxPixelCount));

    TEST_ASSERT_GREATER_THAN_UINT32(8, countChangedPixels(idleFrame, tiltedFrame));
}

void test_liquid_wave_is_not_alias_of_iridescent_ripple() {
    auto config = makeConfig(static_cast<uint8_t>(MATRIX_FX::Native3DMode::CenterRipple));
    MATRIX_FX::MatrixFxInput input;
    input.imuValid = true;
    input.ax = 0.34f;
    input.ay = -0.42f;
    input.motionEnergy = 0.18f;

    uint32_t rippleFrame[MATRIX_FX::kMatrixFxPixelCount]{};
    renderSettledFrame(
        static_cast<uint8_t>(MATRIX_FX::Native3DMode::CenterRipple),
        config,
        input,
        rippleFrame);

    uint32_t liquidFrame[MATRIX_FX::kMatrixFxPixelCount]{};
    renderSettledFrame(
        static_cast<uint8_t>(MATRIX_FX::Native3DMode::LiquidWave),
        config,
        input,
        liquidFrame);

    TEST_ASSERT_GREATER_THAN_UINT32(24, countChangedPixels(rippleFrame, liquidFrame));
}

void test_gravity_particles_fall_opposite_accelerometer_support_vector() {
    const auto config = makeConfig(static_cast<uint8_t>(MATRIX_FX::Native3DMode::GravityParticles));
    uint32_t supportUpFrame[MATRIX_FX::kMatrixFxPixelCount]{};
    uint32_t supportDownFrame[MATRIX_FX::kMatrixFxPixelCount]{};

    MATRIX_FX::MatrixFxInput supportUp;
    supportUp.imuValid = true;
    supportUp.ay = 0.85f;

    MATRIX_FX::MatrixFxInput supportDown;
    supportDown.imuValid = true;
    supportDown.ay = -0.85f;

    MATRIX_FX::MatrixFxEngine3D supportUpEngine;
    supportUpEngine.configure(config);
    supportUpEngine.setInput(supportUp);
    for (uint8_t i = 0; i < 24; ++i) {
        TEST_ASSERT_TRUE(supportUpEngine.render(1000 + i * 33, supportUpFrame, MATRIX_FX::kMatrixFxPixelCount));
    }

    MATRIX_FX::MatrixFxEngine3D supportDownEngine;
    supportDownEngine.configure(config);
    supportDownEngine.setInput(supportDown);
    for (uint8_t i = 0; i < 24; ++i) {
        TEST_ASSERT_TRUE(supportDownEngine.render(1000 + i * 33, supportDownFrame, MATRIX_FX::kMatrixFxPixelCount));
    }

    TEST_ASSERT_LESS_THAN_FLOAT(weightedY(supportDownFrame) - 0.45f, weightedY(supportUpFrame));
}

void test_depth_tunnel_keeps_readable_wireframe_shape() {
    const auto config = makeConfig(static_cast<uint8_t>(MATRIX_FX::Native3DMode::DepthTunnel));
    uint32_t frame[MATRIX_FX::kMatrixFxPixelCount]{};
    MATRIX_FX::MatrixFxEngine3D engine;
    engine.configure(config);

    TEST_ASSERT_TRUE(engine.render(1000, frame, MATRIX_FX::kMatrixFxPixelCount));

    TEST_ASSERT_EQUAL_UINT32(MATRIX_FX::kMatrixFxPixelCount, countLitPixels(frame));
    const uint32_t brightPixels = countPixelsAboveBrightness(frame, 42);
    TEST_ASSERT_GREATER_THAN_UINT32(8, brightPixels);
    TEST_ASSERT_LESS_THAN_UINT32(MATRIX_FX::kMatrixFxPixelCount, brightPixels);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_engine_rejects_render_before_configure);
    RUN_TEST(test_engine_renders_all_native_modes_to_nonblank_frames);
    RUN_TEST(test_render_audit_all_native_modes_are_varied);
    RUN_TEST(test_render_audit_all_native_modes_react_to_imu_input);
    RUN_TEST(test_render_audit_all_native_modes_have_smooth_temporal_transitions);
    RUN_TEST(test_engine_clamps_config_and_keeps_small_runtime_state);
    RUN_TEST(test_imu_input_changes_projected_frame);
    RUN_TEST(test_iridescent_ripple_reacts_to_tilted_center_and_hue);
    RUN_TEST(test_liquid_wave_reacts_to_imu_tilt);
    RUN_TEST(test_liquid_wave_is_not_alias_of_iridescent_ripple);
    RUN_TEST(test_gravity_particles_fall_opposite_accelerometer_support_vector);
    RUN_TEST(test_depth_tunnel_keeps_readable_wireframe_shape);
    return UNITY_END();
}
