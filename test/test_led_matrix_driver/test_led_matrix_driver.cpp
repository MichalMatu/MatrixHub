#include <unity.h>

#include "../../lib/matrix_driver/LedMatrix.h"

void setUp(void) {
    WS2812FX::lastInstance = nullptr;
}

void tearDown(void) {}

void test_zero_brightness_commits_black_driver_frame_and_keeps_output_muted() {
    LedMatrix matrix;
    matrix.begin(5);

    auto* strip = WS2812FX::lastInstance;
    TEST_ASSERT_NOT_NULL(strip);
    TEST_ASSERT_EQUAL_UINT32(LedMatrix::NUM_LEDS, strip->pixels.size());
    TEST_ASSERT_EQUAL_UINT32(LedMatrix::NUM_LEDS, strip->shownPixels.size());
    matrix.fillScreen(0x123456);
    matrix.show();
    for (uint32_t pixel : strip->shownPixels) {
        TEST_ASSERT_EQUAL_HEX32(0x123456, pixel);
    }

    strip->showCalls = 0;
    matrix.setBrightness(0);

    TEST_ASSERT_EQUAL_UINT8(1, strip->brightness);
    TEST_ASSERT_EQUAL_UINT32(1, strip->showCalls);
    for (uint32_t pixel : strip->pixels) {
        TEST_ASSERT_EQUAL_HEX32(0x000000, pixel);
    }
    for (uint32_t pixel : strip->shownPixels) {
        TEST_ASSERT_EQUAL_HEX32(0x000000, pixel);
    }

    matrix.fillScreen(0xABCDEF);
    matrix.show();
    for (uint32_t pixel : strip->shownPixels) {
        TEST_ASSERT_EQUAL_HEX32(0x000000, pixel);
    }

    matrix.start();
    TEST_ASSERT_TRUE(strip->running);
    matrix.service();
    TEST_ASSERT_EQUAL_UINT32(0, strip->serviceCalls);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_zero_brightness_commits_black_driver_frame_and_keeps_output_muted);
    return UNITY_END();
}
