#include <unity.h>

#include "../../lib/matrix_service/effects/MatrixFxEngine3D.cpp"
#include "../../lib/matrix_service/visualization/MatrixDataVisualizationEngine.cpp"
#include "../../lib/matrix_service/renderer/MatrixRenderer.cpp"

void IconDrawer::draw(LedMatrix* matrix, IconType icon, const uint32_t* customBitmap) {
    (void)icon;
    if (matrix) {
        matrix->drawBitmap(customBitmap);
    }
}

namespace {

LedMatrix& matrix() {
    TEST_ASSERT_NOT_NULL(LedMatrix::lastInstance);
    return *LedMatrix::lastInstance;
}

}  // namespace

void setUp(void) {
    TEST_STUBS::ARDUINO::millisValue = 0;
    LedMatrix::lastInstance = nullptr;
}

void tearDown(void) {}

void test_show_effect_normalizes_unsupported_mode() {
    MatrixRenderer renderer;
    renderer.begin(5);
    matrix().resetCounters();

    renderer.showEffect(70, 750, 0x010203, 0x040506, 0x070809);

    TEST_ASSERT_EQUAL_UINT8(UI::MATRIX::DEFAULT_EFFECT_MODE, matrix().lastMode);
    TEST_ASSERT_EQUAL_UINT16(750, matrix().lastSpeed);
    TEST_ASSERT_EQUAL_HEX32(0x010203, matrix().lastColor1);
    TEST_ASSERT_EQUAL_HEX32(0x040506, matrix().lastColor2);
    TEST_ASSERT_EQUAL_HEX32(0x070809, matrix().lastColor3);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().startCalls);
}

void test_show_text_same_text_new_color_repaints() {
    MatrixRenderer renderer;
    renderer.begin(5);
    matrix().resetCounters();

    renderer.showText("AB", 0x112233);
    renderer.loop();
    matrix().resetCounters();

    renderer.showText("AB", 0x445566);
    renderer.loop();

    TEST_ASSERT_EQUAL_UINT32(1, matrix().drawStringCalls);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().showCalls);
    TEST_ASSERT_EQUAL_HEX32(0x445566, matrix().lastDrawColor);
}

void test_show_text_same_text_same_color_is_deduplicated() {
    MatrixRenderer renderer;
    renderer.begin(5);
    matrix().resetCounters();

    renderer.showText("AB", 0x112233);
    renderer.loop();
    matrix().resetCounters();

    renderer.showText("AB", 0x112233);
    renderer.loop();

    TEST_ASSERT_EQUAL_UINT32(0, matrix().drawStringCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().showCalls);
}

void test_show_text_after_effect_renders_immediately() {
    MatrixRenderer renderer;
    renderer.begin(5);
    matrix().resetCounters();

    renderer.showEffect(11, 1000, 0x123456, 0x000000, 0x000000);
    matrix().resetCounters();

    renderer.showText("HI", 0xABCDEF);
    renderer.loop();

    TEST_ASSERT_EQUAL_UINT32(1, matrix().showCalls);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().drawStringCalls);
    TEST_ASSERT_EQUAL_HEX32(0xABCDEF, matrix().lastDrawColor);
}

void test_legacy_effect_service_is_not_double_throttled_by_renderer() {
    MatrixRenderer renderer;
    renderer.begin(5);
    matrix().resetCounters();

    renderer.showEffect(11, 1000, 0x123456, 0x234567, 0x345678);
    TEST_ASSERT_EQUAL_UINT16(1000, matrix().lastSpeed);

    TEST_STUBS::ARDUINO::millisValue = 1000;
    renderer.loop();
    TEST_STUBS::ARDUINO::millisValue = 1010;
    renderer.loop();
    TEST_STUBS::ARDUINO::millisValue = 1020;
    renderer.loop();

    TEST_ASSERT_EQUAL_UINT32(3, matrix().serviceCalls);
}

void test_native_3d_effect_renders_bitmap_without_starting_legacy_fx() {
    MatrixRenderer renderer;
    renderer.begin(5);
    matrix().resetCounters();

    renderer.showNative3DEffect(2, 900, 0x123456, 0x234567, 0x345678, 1, 125);

    TEST_ASSERT_TRUE(renderer.isActive());
    TEST_ASSERT_EQUAL_UINT32(0, matrix().startCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().serviceCalls);

    TEST_STUBS::ARDUINO::millisValue = 1000;
    renderer.loop();

    TEST_ASSERT_EQUAL_UINT32(1, matrix().drawBitmapCalls);
    TEST_ASSERT_FALSE(matrix().lastBitmapWasNull);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().showCalls);
}

void test_data_visualization_renders_bitmap_without_starting_legacy_fx() {
    MatrixRenderer renderer;
    renderer.begin(5);
    matrix().resetCounters();

    MATRIX::MatrixDataVisualizationConfig config;
    config.enabled = true;
    config.mode = static_cast<uint8_t>(MATRIX::MatrixDataVizMode::Gauge);
    config.minValue = 0.0f;
    config.maxValue = 100.0f;
    renderer.showDataVisualization(config);

    MATRIX::MatrixDataVisualizationInput input;
    input.valid = true;
    input.value = 50.0f;
    input.timestampMs = 1;
    renderer.setDataVisualizationInput(input);
    renderer.loop();

    TEST_ASSERT_TRUE(renderer.isActive());
    TEST_ASSERT_EQUAL_UINT32(0, matrix().startCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().serviceCalls);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().drawBitmapCalls);
    TEST_ASSERT_FALSE(matrix().lastBitmapWasNull);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().showCalls);
}

void test_brightness_zero_blacks_out_and_blocks_effect_rendering() {
    MatrixRenderer renderer;
    renderer.begin(5);
    matrix().resetCounters();

    renderer.setBrightness(0);

    TEST_ASSERT_EQUAL_UINT8(0, matrix().lastBrightness);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().fillCalls);
    TEST_ASSERT_EQUAL_HEX32(0x000000, matrix().lastFillColor);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().showCalls);

    matrix().resetCounters();

    renderer.showEffect(11, 1000, 0x123456, 0x234567, 0x345678);
    renderer.loop();

    TEST_ASSERT_EQUAL_UINT32(0, matrix().startCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().serviceCalls);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().fillCalls);
    TEST_ASSERT_EQUAL_HEX32(0x000000, matrix().lastFillColor);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().showCalls);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_show_effect_normalizes_unsupported_mode);
    RUN_TEST(test_show_text_same_text_new_color_repaints);
    RUN_TEST(test_show_text_same_text_same_color_is_deduplicated);
    RUN_TEST(test_show_text_after_effect_renders_immediately);
    RUN_TEST(test_legacy_effect_service_is_not_double_throttled_by_renderer);
    RUN_TEST(test_native_3d_effect_renders_bitmap_without_starting_legacy_fx);
    RUN_TEST(test_data_visualization_renders_bitmap_without_starting_legacy_fx);
    RUN_TEST(test_brightness_zero_blacks_out_and_blocks_effect_rendering);
    return UNITY_END();
}
