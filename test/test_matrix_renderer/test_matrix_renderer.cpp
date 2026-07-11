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

void test_runtime_zero_mutes_legacy_effect_and_restore_resumes_it() {
    MatrixRenderer renderer;
    renderer.begin(5);
    renderer.showEffect(11, 1000, 0x123456, 0x234567, 0x345678);
    TEST_ASSERT_TRUE(renderer.isActive());
    matrix().resetCounters();

    renderer.setBrightness(0);

    TEST_ASSERT_EQUAL_UINT8(0, matrix().lastBrightness);
    TEST_ASSERT_FALSE(renderer.isActive());
    TEST_ASSERT_EQUAL_UINT32(0, matrix().stopCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().fillCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().showCalls);

    matrix().resetCounters();
    renderer.loop();

    TEST_ASSERT_EQUAL_UINT32(0, matrix().serviceCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().drawBitmapCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().drawStringCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().showCalls);

    renderer.setBrightness(32);
    TEST_ASSERT_TRUE(renderer.isActive());
    renderer.loop();

    TEST_ASSERT_EQUAL_UINT32(1, matrix().serviceCalls);
}

void test_runtime_zero_mutes_native_3d_effect_and_restore_resumes_it() {
    MatrixRenderer renderer;
    renderer.begin(5);
    renderer.showNative3DEffect(2, 900, 0x123456, 0x234567, 0x345678, 1, 125);
    TEST_ASSERT_TRUE(renderer.isActive());
    matrix().resetCounters();

    renderer.setBrightness(0);

    TEST_ASSERT_FALSE(renderer.isActive());
    TEST_ASSERT_EQUAL_UINT32(0, matrix().fillCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().showCalls);

    matrix().resetCounters();
    renderer.loop();
    TEST_ASSERT_EQUAL_UINT32(0, matrix().drawBitmapCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().showCalls);

    renderer.setBrightness(32);
    TEST_STUBS::ARDUINO::millisValue = 1000;
    renderer.loop();
    TEST_ASSERT_TRUE(renderer.isActive());
    TEST_ASSERT_EQUAL_UINT32(1, matrix().drawBitmapCalls);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().showCalls);
}

void test_runtime_zero_mutes_data_visualization_and_restore_resumes_it() {
    MatrixRenderer renderer;
    renderer.begin(5);
    MATRIX::MatrixDataVisualizationConfig config;
    config.enabled = true;
    renderer.showDataVisualization(config);
    TEST_ASSERT_TRUE(renderer.isActive());
    matrix().resetCounters();

    renderer.setBrightness(0);

    TEST_ASSERT_FALSE(renderer.isActive());
    TEST_ASSERT_EQUAL_UINT32(0, matrix().fillCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().showCalls);

    matrix().resetCounters();
    renderer.loop();
    TEST_ASSERT_EQUAL_UINT32(0, matrix().drawBitmapCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().showCalls);

    renderer.setBrightness(32);
    TEST_STUBS::ARDUINO::millisValue = 1000;
    renderer.loop();
    TEST_ASSERT_TRUE(renderer.isActive());
    TEST_ASSERT_EQUAL_UINT32(1, matrix().drawBitmapCalls);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().showCalls);
}

void test_runtime_zero_mutes_scrolling_text_and_restore_resumes_it() {
    MatrixRenderer renderer;
    renderer.begin(5);
    renderer.showText("SHUTDOWN", 0xFFFFFF);
    TEST_ASSERT_TRUE(renderer.isActive());
    matrix().resetCounters();

    renderer.setBrightness(0);

    TEST_ASSERT_FALSE(renderer.isActive());
    TEST_ASSERT_EQUAL_UINT32(0, matrix().fillCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().showCalls);

    matrix().resetCounters();
    TEST_STUBS::ARDUINO::millisValue = 1000;
    renderer.loop();
    TEST_ASSERT_EQUAL_UINT32(0, matrix().drawStringCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().showCalls);

    renderer.setBrightness(32);
    renderer.loop();
    TEST_ASSERT_TRUE(renderer.isActive());
    TEST_ASSERT_EQUAL_UINT32(1, matrix().drawStringCalls);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().showCalls);
}

void test_runtime_zero_does_not_destroy_static_content_state() {
    MatrixRenderer renderer;
    renderer.begin(5);
    renderer.showSolid(0xABCDEF);
    matrix().resetCounters();

    renderer.setBrightness(0);

    TEST_ASSERT_EQUAL_UINT32(0, matrix().fillCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().showCalls);

    renderer.setBrightness(32);
    renderer.loop();
    TEST_ASSERT_EQUAL_UINT32(1, matrix().restoreOutputCalls);
}

void test_runtime_zero_does_not_destroy_static_icon_state() {
    MatrixRenderer renderer;
    renderer.begin(5);
    renderer.showIcon(IconType::ALARM_CRITICAL);
    matrix().resetCounters();

    renderer.setBrightness(0);

    TEST_ASSERT_EQUAL_UINT32(0, matrix().fillCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().showCalls);

    renderer.setBrightness(32);
    renderer.loop();
    TEST_ASSERT_EQUAL_UINT32(1, matrix().restoreOutputCalls);
}

void test_rotation_defers_static_icon_repaint_until_renderer_loop() {
    MatrixRenderer renderer;
    renderer.begin(5);
    renderer.showIcon(IconType::ALARM_WARNING);
    matrix().resetCounters();

    renderer.setRotation(1);

    TEST_ASSERT_EQUAL_UINT8(1, matrix().lastRotation);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().fillCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().drawBitmapCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().showCalls);

    renderer.loop();

    TEST_ASSERT_EQUAL_UINT32(1, matrix().fillCalls);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().drawBitmapCalls);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().showCalls);
}

void test_terminal_blackout_still_discards_renderer_modes() {
    MatrixRenderer renderer;
    renderer.begin(5);
    renderer.showEffect(11, 1000, 0x123456, 0x234567, 0x345678);
    matrix().resetCounters();

    renderer.blackoutForShutdown();

    TEST_ASSERT_EQUAL_UINT8(UI::MATRIX::BRIGHTNESS_DEFAULT, matrix().lastBrightness);
    TEST_ASSERT_FALSE(renderer.isActive());
    TEST_ASSERT_EQUAL_UINT32(1, matrix().stopCalls);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().fillCalls);
    TEST_ASSERT_EQUAL_HEX32(0x000000, matrix().lastFillColor);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().showCalls);

    matrix().resetCounters();
    renderer.setBrightness(32);
    renderer.loop();
    TEST_ASSERT_FALSE(renderer.isActive());
    TEST_ASSERT_EQUAL_UINT32(0, matrix().serviceCalls);
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
    RUN_TEST(test_runtime_zero_mutes_legacy_effect_and_restore_resumes_it);
    RUN_TEST(test_runtime_zero_mutes_native_3d_effect_and_restore_resumes_it);
    RUN_TEST(test_runtime_zero_mutes_data_visualization_and_restore_resumes_it);
    RUN_TEST(test_runtime_zero_mutes_scrolling_text_and_restore_resumes_it);
    RUN_TEST(test_runtime_zero_does_not_destroy_static_content_state);
    RUN_TEST(test_runtime_zero_does_not_destroy_static_icon_state);
    RUN_TEST(test_rotation_defers_static_icon_repaint_until_renderer_loop);
    RUN_TEST(test_terminal_blackout_still_discards_renderer_modes);
    return UNITY_END();
}
