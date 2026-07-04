#include <unity.h>

#include "../../src/matrix/MatrixEffectModes.h"

void test_supported_matrix_effect_mode_accepts_visible_modes() {
    TEST_ASSERT_TRUE(MATRIX::isSupportedMatrixEffectMode(0));
    TEST_ASSERT_TRUE(MATRIX::isSupportedMatrixEffectMode(65));
    TEST_ASSERT_TRUE(MATRIX::isSupportedMatrixEffectMode(69));
}

void test_supported_matrix_effect_mode_rejects_out_of_range_modes() {
    TEST_ASSERT_FALSE(MATRIX::isSupportedMatrixEffectMode(70));
    TEST_ASSERT_FALSE(MATRIX::isSupportedMatrixEffectMode(79));
    TEST_ASSERT_FALSE(MATRIX::isSupportedMatrixEffectMode(255));
}

void test_normalize_matrix_effect_mode_falls_back_to_static() {
    TEST_ASSERT_EQUAL_UINT8(UI::MATRIX::DEFAULT_EFFECT_MODE, MATRIX::normalizeMatrixEffectMode(70));
    TEST_ASSERT_EQUAL_UINT8(UI::MATRIX::DEFAULT_EFFECT_MODE, MATRIX::normalizeMatrixEffectMode(255));
    TEST_ASSERT_EQUAL_UINT8(69, MATRIX::normalizeMatrixEffectMode(69));
}

void test_normalize_matrix_effect_mode_for_native_engine() {
    TEST_ASSERT_EQUAL_UINT8(14, MATRIX::normalizeMatrixEffectModeForEngine(14, 1));
    TEST_ASSERT_EQUAL_UINT8(0, MATRIX::normalizeMatrixEffectModeForEngine(15, 1));
    TEST_ASSERT_EQUAL_UINT8(0, MATRIX::normalizeMatrixEffectModeForEngine(69, 1));
    TEST_ASSERT_EQUAL_UINT8(69, MATRIX::normalizeMatrixEffectModeForEngine(69, 0));
}

void test_normalize_matrix_effect_reactivity_fields() {
    TEST_ASSERT_EQUAL_UINT8(0, MATRIX::normalizeMatrixEffectEngine(9));
    TEST_ASSERT_EQUAL_UINT8(1, MATRIX::normalizeMatrixEffectEngine(1));
    TEST_ASSERT_EQUAL_UINT8(0, MATRIX::normalizeMatrixEffectReactivityProvider(9));
    TEST_ASSERT_EQUAL_UINT8(1, MATRIX::normalizeMatrixEffectReactivityProvider(1));
    TEST_ASSERT_EQUAL_UINT8(200, MATRIX::normalizeMatrixEffectReactivityGain(255));
    TEST_ASSERT_EQUAL_UINT8(125, MATRIX::normalizeMatrixEffectReactivityGain(125));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_supported_matrix_effect_mode_accepts_visible_modes);
    RUN_TEST(test_supported_matrix_effect_mode_rejects_out_of_range_modes);
    RUN_TEST(test_normalize_matrix_effect_mode_falls_back_to_static);
    RUN_TEST(test_normalize_matrix_effect_mode_for_native_engine);
    RUN_TEST(test_normalize_matrix_effect_reactivity_fields);
    return UNITY_END();
}
