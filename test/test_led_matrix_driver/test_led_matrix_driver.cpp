#include <unity.h>

#include "../../lib/matrix_driver/LedMatrix.h"

void setUp(void) {
    WS2812FX::lastInstance = nullptr;
}

void tearDown(void) {}

namespace {

void assertShownColor(const WS2812FX* strip, uint32_t logicalColor, uint8_t brightness) {
    const uint32_t expected = WS2812FX::scaleColor(logicalColor, brightness);
    for (uint32_t pixel : strip->shownPixels) {
        TEST_ASSERT_EQUAL_HEX32(expected, pixel);
    }
}

}  // namespace

void test_board_default_uses_rgb_byte_order() {
    LedMatrix matrix;
    matrix.begin(5);

    auto* strip = WS2812FX::lastInstance;
    TEST_ASSERT_NOT_NULL(strip);
    TEST_ASSERT_EQUAL_UINT8(NEO_RGB + NEO_KHZ800, strip->type);
    TEST_ASSERT_NOT_EQUAL(NEO_GRB + NEO_KHZ800, strip->type);
    TEST_ASSERT_EQUAL_UINT8(1, strip->maxSegments);
    TEST_ASSERT_EQUAL_UINT8(1, strip->maxActiveSegments);
}

void test_nonzero_brightness_change_repaints_static_logical_frame_immediately() {
    LedMatrix matrix;
    matrix.begin(5);

    auto* strip = WS2812FX::lastInstance;
    TEST_ASSERT_NOT_NULL(strip);
    TEST_ASSERT_EQUAL_UINT32(LedMatrix::NUM_LEDS, strip->pixels.size());
    TEST_ASSERT_EQUAL_UINT32(LedMatrix::NUM_LEDS, strip->shownPixels.size());
    matrix.fillScreen(0x80C040);
    matrix.show();
    assertShownColor(strip, 0x80C040, 50);

    strip->showCalls = 0;
    strip->setPixelColorCalls = 0;
    matrix.setBrightness(16);

    TEST_ASSERT_EQUAL_UINT8(16, strip->brightness);
    TEST_ASSERT_EQUAL_UINT32(LedMatrix::NUM_LEDS, strip->setPixelColorCalls);
    TEST_ASSERT_EQUAL_UINT32(1, strip->showCalls);
    assertShownColor(strip, 0x80C040, 16);

    strip->showCalls = 0;
    strip->setPixelColorCalls = 0;
    matrix.setBrightness(2);

    TEST_ASSERT_EQUAL_UINT8(2, strip->brightness);
    TEST_ASSERT_EQUAL_UINT32(LedMatrix::NUM_LEDS, strip->setPixelColorCalls);
    TEST_ASSERT_EQUAL_UINT32(1, strip->showCalls);
    assertShownColor(strip, 0x80C040, 2);

    matrix.setBrightness(80);
    assertShownColor(strip, 0x80C040, 80);
}

void test_muted_static_updates_stay_black_and_restore_latest_logical_frame() {
    LedMatrix matrix;
    matrix.begin(5);

    auto* strip = WS2812FX::lastInstance;
    TEST_ASSERT_NOT_NULL(strip);
    matrix.fillScreen(0x123456);
    matrix.show();

    strip->showCalls = 0;
    matrix.setBrightness(0);

    TEST_ASSERT_EQUAL_UINT8(0, strip->brightness);
    TEST_ASSERT_EQUAL_UINT32(1, strip->showCalls);
    for (uint32_t pixel : strip->pixels) {
        TEST_ASSERT_EQUAL_HEX32(0x000000, pixel);
    }
    for (uint32_t pixel : strip->shownPixels) {
        TEST_ASSERT_EQUAL_HEX32(0x000000, pixel);
    }

    matrix.fillScreen(0xABCDEF);
    matrix.show();
    TEST_ASSERT_EQUAL_UINT32(1, strip->showCalls);
    for (uint32_t pixel : strip->shownPixels) {
        TEST_ASSERT_EQUAL_HEX32(0x000000, pixel);
    }

    matrix.setBrightness(32);

    // A 0 -> non-zero transition intentionally stays black until the owner has
    // drained any newer visible command queued behind brightness.
    TEST_ASSERT_EQUAL_UINT32(1, strip->showCalls);
    for (uint32_t pixel : strip->shownPixels) {
        TEST_ASSERT_EQUAL_HEX32(0x000000, pixel);
    }

    // A coalesced thermal/settings update can change the non-zero cap again
    // before the pending owner renders. It must not reveal the pre-mute frame.
    matrix.setBrightness(16);
    TEST_ASSERT_EQUAL_UINT32(1, strip->showCalls);
    for (uint32_t pixel : strip->shownPixels) {
        TEST_ASSERT_EQUAL_HEX32(0x000000, pixel);
    }

    matrix.restoreOutputIfPending();
    TEST_ASSERT_EQUAL_UINT32(2, strip->showCalls);
    assertShownColor(strip, 0xABCDEF, 16);
}

void test_legacy_effect_is_paused_while_muted_and_restarted_at_new_cap() {
    LedMatrix matrix;
    matrix.begin(5);

    auto* strip = WS2812FX::lastInstance;
    TEST_ASSERT_NOT_NULL(strip);

    matrix.start();
    TEST_ASSERT_TRUE(strip->running);
    TEST_ASSERT_EQUAL_UINT32(1, strip->startCalls);

    matrix.setBrightness(0);
    matrix.service();
    TEST_ASSERT_EQUAL_UINT32(0, strip->serviceCalls);

    matrix.setBrightness(24);
    TEST_ASSERT_EQUAL_UINT32(2, strip->startCalls);
    for (uint32_t pixel : strip->shownPixels) {
        TEST_ASSERT_EQUAL_HEX32(0x000000, pixel);
    }

    strip->showCalls = 0;
    matrix.setBrightness(12);
    TEST_ASSERT_EQUAL_UINT32(2, strip->startCalls);
    TEST_ASSERT_EQUAL_UINT32(0, strip->showCalls);

    matrix.service();
    TEST_ASSERT_EQUAL_UINT32(1, strip->serviceCalls);
}

void test_nonzero_cap_restarts_legacy_effect_from_black_transport_without_extra_latch() {
    LedMatrix matrix;
    matrix.begin(5);

    auto* strip = WS2812FX::lastInstance;
    TEST_ASSERT_NOT_NULL(strip);
    strip->pixels.assign(LedMatrix::NUM_LEDS, 0xFFFFFF);
    strip->shownPixels = strip->pixels;
    matrix.start();
    strip->showCalls = 0;
    strip->serviceCalls = 0;
    strip->startCalls = 0;
    strip->triggerCalls = 0;
    strip->setPixelColorCalls = 0;

    matrix.setBrightness(16);

    TEST_ASSERT_EQUAL_UINT32(LedMatrix::NUM_LEDS, strip->setPixelColorCalls);
    TEST_ASSERT_EQUAL_UINT32(1, strip->startCalls);
    TEST_ASSERT_EQUAL_UINT32(1, strip->triggerCalls);
    TEST_ASSERT_EQUAL_UINT32(1, strip->serviceCalls);
    TEST_ASSERT_EQUAL_UINT32(1, strip->showCalls);
    for (uint32_t pixel : strip->shownPixels) {
        TEST_ASSERT_EQUAL_HEX32(0x000000, pixel);
    }
}

void test_nonzero_cap_latches_black_if_legacy_effect_cannot_render_forced_frame() {
    LedMatrix matrix;
    matrix.begin(5);

    auto* strip = WS2812FX::lastInstance;
    TEST_ASSERT_NOT_NULL(strip);
    strip->pixels.assign(LedMatrix::NUM_LEDS, 0xFFFFFF);
    strip->shownPixels = strip->pixels;
    strip->serviceRenderedFrame = false;
    matrix.start();
    strip->showCalls = 0;

    matrix.setBrightness(16);

    TEST_ASSERT_EQUAL_UINT32(1, strip->serviceCalls);
    TEST_ASSERT_EQUAL_UINT32(1, strip->showCalls);
    for (uint32_t pixel : strip->shownPixels) {
        TEST_ASSERT_EQUAL_HEX32(0x000000, pixel);
    }
}

void test_pause_effect_relinquishes_ownership_without_latching_an_intermediate_black_frame() {
    LedMatrix matrix;
    matrix.begin(5);

    auto* strip = WS2812FX::lastInstance;
    TEST_ASSERT_NOT_NULL(strip);
    matrix.fillScreen(0x123456);
    matrix.show();
    const auto shownBeforeStop = strip->shownPixels;
    strip->showCalls = 0;
    strip->stopCalls = 0;
    strip->pauseCalls = 0;

    matrix.start();
    matrix.pauseEffect();

    TEST_ASSERT_FALSE(strip->running);
    TEST_ASSERT_EQUAL_UINT32(1, strip->pauseCalls);
    TEST_ASSERT_EQUAL_UINT32(0, strip->stopCalls);
    TEST_ASSERT_EQUAL_UINT32(0, strip->showCalls);
    TEST_ASSERT_EQUAL_UINT32(shownBeforeStop.size(), strip->shownPixels.size());
    for (size_t index = 0; index < shownBeforeStop.size(); ++index) {
        TEST_ASSERT_EQUAL_HEX32(shownBeforeStop[index], strip->shownPixels[index]);
    }
}

void test_brightness_after_pause_keeps_held_effect_frame_until_new_owner_latches() {
    LedMatrix matrix;
    matrix.begin(5);

    auto* strip = WS2812FX::lastInstance;
    TEST_ASSERT_NOT_NULL(strip);
    matrix.start();
    strip->pixels.assign(LedMatrix::NUM_LEDS, 0x112233);
    strip->show();
    const auto heldEffectFrame = strip->shownPixels;
    const uint8_t heldBrightness = strip->brightness;

    strip->showCalls = 0;
    strip->setPixelColorCalls = 0;
    matrix.pauseEffect();
    matrix.setBrightness(16);

    TEST_ASSERT_EQUAL_UINT32(1, strip->showCalls);
    TEST_ASSERT_EQUAL_UINT32(0, strip->setPixelColorCalls);
    TEST_ASSERT_EQUAL_UINT32(heldEffectFrame.size(), strip->shownPixels.size());
    for (size_t index = 0; index < heldEffectFrame.size(); ++index) {
        TEST_ASSERT_EQUAL_HEX32(
            WS2812FX::rescaleColor(heldEffectFrame[index], heldBrightness, 16),
            strip->shownPixels[index]);
    }

    matrix.fillScreen(0xA0B0C0);
    matrix.show();
    strip->showCalls = 0;
    strip->setPixelColorCalls = 0;
    matrix.setBrightness(8);

    TEST_ASSERT_EQUAL_UINT32(1, strip->showCalls);
    TEST_ASSERT_EQUAL_UINT32(LedMatrix::NUM_LEDS, strip->setPixelColorCalls);
    assertShownColor(strip, 0xA0B0C0, 8);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_board_default_uses_rgb_byte_order);
    RUN_TEST(test_nonzero_brightness_change_repaints_static_logical_frame_immediately);
    RUN_TEST(test_muted_static_updates_stay_black_and_restore_latest_logical_frame);
    RUN_TEST(test_legacy_effect_is_paused_while_muted_and_restarted_at_new_cap);
    RUN_TEST(test_nonzero_cap_restarts_legacy_effect_from_black_transport_without_extra_latch);
    RUN_TEST(test_nonzero_cap_latches_black_if_legacy_effect_cannot_render_forced_frame);
    RUN_TEST(test_pause_effect_relinquishes_ownership_without_latching_an_intermediate_black_frame);
    RUN_TEST(test_brightness_after_pause_keeps_held_effect_frame_until_new_owner_latches);
    return UNITY_END();
}
