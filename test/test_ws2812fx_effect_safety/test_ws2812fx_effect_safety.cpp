#include <unity.h>

#include "../../lib/WS2812FX/EffectSafety.h"
#include "../../lib/WS2812FX/EffectRuntimeState.h"

void setUp(void) {}
void tearDown(void) {}

void test_select_visible_color_falls_back_for_missing_or_black_palettes() {
    const uint32_t allBlack[] = {0, 0, 0};

    TEST_ASSERT_EQUAL_HEX32(
        0x123456,
        WS2812FX_DETAIL::selectVisibleColor(nullptr, 3, 0, 0x123456));
    TEST_ASSERT_EQUAL_HEX32(
        0x123456,
        WS2812FX_DETAIL::selectVisibleColor(allBlack, 3, 255, 0x123456));
    TEST_ASSERT_EQUAL_HEX32(
        0x000000,
        WS2812FX_DETAIL::selectVisibleColor(allBlack, 3, 255, 0x000000));
    TEST_ASSERT_EQUAL_HEX32(
        0x123456,
        WS2812FX_DETAIL::selectVisibleColor(allBlack, 0, 0, 0x123456));
}

void test_select_visible_color_skips_black_entries_and_wraps_selection() {
    const uint32_t colors[] = {0xAA0000, 0, 0x0000CC};

    TEST_ASSERT_EQUAL_HEX32(
        0xAA0000,
        WS2812FX_DETAIL::selectVisibleColor(colors, 3, 0, 0x123456));
    TEST_ASSERT_EQUAL_HEX32(
        0x0000CC,
        WS2812FX_DETAIL::selectVisibleColor(colors, 3, 1, 0x123456));
    TEST_ASSERT_EQUAL_HEX32(
        0xAA0000,
        WS2812FX_DETAIL::selectVisibleColor(colors, 3, 2, 0x123456));
    TEST_ASSERT_EQUAL_HEX32(
        0x0000CC,
        WS2812FX_DETAIL::selectVisibleColor(colors, 3, 255, 0x123456));
}

void test_full_frame_blink_and_strobe_delays_enforce_a_safe_flash_period() {
    TEST_ASSERT_EQUAL_UINT16(250, WS2812FX_DETAIL::safeBlinkPhaseDelay(50));
    TEST_ASSERT_EQUAL_UINT16(250, WS2812FX_DETAIL::safeBlinkPhaseDelay(500));
    TEST_ASSERT_EQUAL_UINT16(750, WS2812FX_DETAIL::safeBlinkPhaseDelay(1500));

    TEST_ASSERT_EQUAL_UINT16(80, WS2812FX_DETAIL::safeStrobePhaseDelay(50, true));
    TEST_ASSERT_EQUAL_UINT16(420, WS2812FX_DETAIL::safeStrobePhaseDelay(50, false));
    TEST_ASSERT_EQUAL_UINT16(1920, WS2812FX_DETAIL::safeStrobePhaseDelay(2000, false));
}

void test_multi_strobe_is_a_bounded_double_pulse_without_a_high_frequency_burst() {
    uint32_t minimumCycleMs = 0;
    uint32_t requestedCycleMs = 0;
    for (uint8_t phase = 0; phase < 4; ++phase) {
        minimumCycleMs += WS2812FX_DETAIL::safeMultiStrobePhaseDelay(50, phase);
        requestedCycleMs += WS2812FX_DETAIL::safeMultiStrobePhaseDelay(2000, phase);
    }

    TEST_ASSERT_EQUAL_UINT32(1000, minimumCycleMs);
    TEST_ASSERT_EQUAL_UINT32(2000, requestedCycleMs);
    TEST_ASSERT_EQUAL_UINT16(80, WS2812FX_DETAIL::safeMultiStrobePhaseDelay(50, 0));
    TEST_ASSERT_EQUAL_UINT16(170, WS2812FX_DETAIL::safeMultiStrobePhaseDelay(50, 1));
    TEST_ASSERT_EQUAL_UINT16(80, WS2812FX_DETAIL::safeMultiStrobePhaseDelay(50, 2));
    TEST_ASSERT_EQUAL_UINT16(670, WS2812FX_DETAIL::safeMultiStrobePhaseDelay(50, 3));
}

void test_frame_deadline_is_inclusive_and_safe_across_millis_rollover() {
    TEST_ASSERT_FALSE(WS2812FX_DETAIL::isFrameDue(99, 100));
    TEST_ASSERT_TRUE(WS2812FX_DETAIL::isFrameDue(100, 100));
    TEST_ASSERT_TRUE(WS2812FX_DETAIL::isFrameDue(101, 100));

    TEST_ASSERT_FALSE(WS2812FX_DETAIL::isFrameDue(UINT32_MAX, 2));
    TEST_ASSERT_FALSE(WS2812FX_DETAIL::isFrameDue(0, 2));
    TEST_ASSERT_FALSE(WS2812FX_DETAIL::isFrameDue(1, 2));
    TEST_ASSERT_TRUE(WS2812FX_DETAIL::isFrameDue(2, 2));
    TEST_ASSERT_TRUE(WS2812FX_DETAIL::isFrameDue(3, 2));
}

void test_reset_deadline_is_immediately_due_at_every_uptime_boundary() {
    const uint32_t resetTimes[] = {
        0,
        static_cast<uint32_t>(INT32_MAX) - 1U,
        static_cast<uint32_t>(INT32_MAX) + 1U,
        UINT32_MAX,
    };

    for (uint32_t now : resetTimes) {
        const uint32_t deadline = WS2812FX_DETAIL::immediateFrameDeadline(now);
        TEST_ASSERT_EQUAL_UINT32(now, deadline);
        TEST_ASSERT_TRUE(WS2812FX_DETAIL::isFrameDue(now, deadline));
    }
}

void test_effect_runtime_initializers_are_per_instance_and_repeatable() {
    WS2812FXEffectRuntime first;
    WS2812FXEffectRuntime second;

    WS2812FX_DETAIL::initializeComets(first, 64);
    WS2812FX_DETAIL::initializePopcorn(first);
    WS2812FX_DETAIL::initializeOscillators(first, 64);
    WS2812FX_DETAIL::initializeComets(second, 64);
    WS2812FX_DETAIL::initializePopcorn(second);
    WS2812FX_DETAIL::initializeOscillators(second, 64);

    for (size_t index = 0; index < 6; ++index) {
        TEST_ASSERT_EQUAL_UINT16(64, first.comets[index]);
        TEST_ASSERT_EQUAL_UINT16(first.comets[index], second.comets[index]);
    }
    for (size_t index = 0; index < 5; ++index) {
        TEST_ASSERT_FLOAT_WITHIN(0.0001f, -1.0f, first.popcorn[index].position);
        TEST_ASSERT_EQUAL_FLOAT(first.popcorn[index].position, second.popcorn[index].position);
        TEST_ASSERT_EQUAL_HEX32(0, first.popcorn[index].color);
    }
    TEST_ASSERT_EQUAL_UINT8(16, first.oscillators[0].size);
    TEST_ASSERT_EQUAL_UINT16(0, first.oscillators[0].pos);
    TEST_ASSERT_EQUAL_INT8(1, first.oscillators[0].speed);
    TEST_ASSERT_EQUAL_UINT8(16, first.oscillators[1].size);
    TEST_ASSERT_EQUAL_UINT16(63, first.oscillators[1].pos);
    TEST_ASSERT_EQUAL_INT8(-2, first.oscillators[1].speed);

    first.comets[0] = 1;
    first.popcorn[0].position = 42.0f;
    first.oscillators[0].pos = 9;
    TEST_ASSERT_EQUAL_UINT16(64, second.comets[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -1.0f, second.popcorn[0].position);
    TEST_ASSERT_EQUAL_UINT16(0, second.oscillators[0].pos);
}

void test_active_segment_lookup_returns_runtime_slot_not_segment_id() {
    const uint8_t activeSegments[] = {5, 2, 9};

    TEST_ASSERT_EQUAL_UINT64(
        0,
        WS2812FX_DETAIL::findActiveSegmentSlot(activeSegments, 3, 5));
    TEST_ASSERT_EQUAL_UINT64(
        1,
        WS2812FX_DETAIL::findActiveSegmentSlot(activeSegments, 3, 2));
    TEST_ASSERT_EQUAL_UINT64(
        2,
        WS2812FX_DETAIL::findActiveSegmentSlot(activeSegments, 3, 9));
    TEST_ASSERT_EQUAL_UINT64(
        WS2812FX_DETAIL::kNoRuntimeSlot,
        WS2812FX_DETAIL::findActiveSegmentSlot(activeSegments, 3, 7));
    TEST_ASSERT_EQUAL_UINT64(
        WS2812FX_DETAIL::kNoRuntimeSlot,
        WS2812FX_DETAIL::findActiveSegmentSlot(nullptr, 3, 5));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_select_visible_color_falls_back_for_missing_or_black_palettes);
    RUN_TEST(test_select_visible_color_skips_black_entries_and_wraps_selection);
    RUN_TEST(test_full_frame_blink_and_strobe_delays_enforce_a_safe_flash_period);
    RUN_TEST(test_multi_strobe_is_a_bounded_double_pulse_without_a_high_frequency_burst);
    RUN_TEST(test_frame_deadline_is_inclusive_and_safe_across_millis_rollover);
    RUN_TEST(test_reset_deadline_is_immediately_due_at_every_uptime_boundary);
    RUN_TEST(test_effect_runtime_initializers_are_per_instance_and_repeatable);
    RUN_TEST(test_active_segment_lookup_returns_runtime_slot_not_segment_id);
    return UNITY_END();
}
