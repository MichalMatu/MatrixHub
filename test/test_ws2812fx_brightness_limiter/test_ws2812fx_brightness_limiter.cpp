#include <unity.h>

#include "../../lib/WS2812FX/BrightnessLimiter.h"

void setUp(void) {}
void tearDown(void) {}

void test_fireworks_spread_never_exceeds_low_brightness_transport_ceiling() {
    for (uint16_t ceiling = 0; ceiling <= 32; ceiling++) {
        for (uint16_t previous = 0; previous <= ceiling; previous++) {
            for (uint16_t current = 0; current <= ceiling; current++) {
                for (uint16_t next = 0; next <= ceiling; next++) {
                    const uint8_t result = WS2812FX_DETAIL::spreadTransportChannel(
                        static_cast<uint8_t>(previous),
                        static_cast<uint8_t>(current),
                        static_cast<uint8_t>(next),
                        static_cast<uint8_t>(ceiling));
                    TEST_ASSERT_LESS_OR_EQUAL_UINT8(ceiling, result);
                }
            }
        }
    }
}
void test_fireworks_spread_clamps_neighbor_sum_at_representative_caps() {
    TEST_ASSERT_EQUAL_UINT8(0, WS2812FX_DETAIL::spreadTransportChannel(0, 0, 0, 0));
    TEST_ASSERT_EQUAL_UINT8(2, WS2812FX_DETAIL::spreadTransportChannel(2, 2, 2, 2));
    TEST_ASSERT_EQUAL_UINT8(16, WS2812FX_DETAIL::spreadTransportChannel(16, 16, 16, 16));
    TEST_ASSERT_EQUAL_UINT8(255, WS2812FX_DETAIL::spreadTransportChannel(255, 255, 255, 255));
    TEST_ASSERT_EQUAL_UINT8(6, WS2812FX_DETAIL::spreadTransportChannel(8, 2, 8, 16));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_fireworks_spread_never_exceeds_low_brightness_transport_ceiling);
    RUN_TEST(test_fireworks_spread_clamps_neighbor_sum_at_representative_caps);
    return UNITY_END();
}
