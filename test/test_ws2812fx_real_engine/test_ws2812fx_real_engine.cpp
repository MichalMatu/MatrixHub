#include <unity.h>

#include <array>
#include <cstdint>
#include <vector>

#include <Arduino.h>
#include <freertos/task.h>

class __FlashStringHelper {};
#define F(value) reinterpret_cast<const __FlashStringHelper*>(value)

inline long map(long value, long fromLow, long fromHigh, long toLow, long toHigh) {
    if (fromHigh == fromLow) {
        return toLow;
    }
    return (value - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow;
}

inline long random(long minimum, long maximum) {
    return maximum <= minimum ? minimum : minimum + random(maximum - minimum);
}

// Select the same 70-mode table and 2 ms scheduler floor as the ESP target
// without enabling ESP32 mutex/RMT dependencies in the host process.
#define ESP8266 1
#include "../../lib/WS2812FX/WS2812FX.cpp"
#include "../../lib/WS2812FX/modes.cpp"
#include "../../lib/WS2812FX/modes_funcs.cpp"

namespace {

constexpr uint16_t kPixelCount = 64;
constexpr uint32_t kLocalizedTraceDurationMs = 5000;
constexpr uint32_t kBackground = 0x010101;

struct TraceFrame {
    uint32_t atMs = 0;
    std::array<uint32_t, kPixelCount> pixels{};
};

std::vector<TraceFrame> renderTrace(
    uint8_t mode,
    uint16_t speed,
    uint32_t durationMs,
    uint16_t segmentStart = 0,
    uint16_t segmentStop = kPixelCount - 1,
    bool reverse = false) {
    TEST_STUBS::ARDUINO::millisValue = 0;
    TEST_STUBS::FREERTOS::resetTaskCreateStub();
    Adafruit_NeoPixel::outOfBoundsWrites = 0;
    Adafruit_NeoPixel::showCalls = 0;

    WS2812FX strip(kPixelCount, 0, NEO_RGB + NEO_KHZ800, 1, 1);
    strip.init();
    strip.setBrightness(255);
    const uint32_t colors[] = {kBackground, 0x000000, 0x000000};
    strip.setSegment(0, segmentStart, segmentStop, mode, colors, speed, reverse);
    strip.start();

    std::vector<TraceFrame> frames;
    for (uint32_t nowMs = 0; nowMs < durationMs; ++nowMs) {
        TEST_STUBS::ARDUINO::millisValue = nowMs;
        if (!strip.service()) {
            continue;
        }

        TraceFrame frame;
        frame.atMs = nowMs;
        for (uint16_t pixel = 0; pixel < kPixelCount; ++pixel) {
            frame.pixels[pixel] = strip.getPixelColor(pixel) & 0x00FFFFFFU;
        }
        frames.push_back(frame);
    }
    return frames;
}

size_t countWhitePixels(const TraceFrame& frame) {
    size_t count = 0;
    for (uint32_t pixel : frame.pixels) {
        if (pixel == 0x00FFFFFFU) {
            ++count;
        }
    }
    return count;
}

std::vector<uint16_t> whitePixelIndices(const TraceFrame& frame) {
    std::vector<uint16_t> indices;
    for (uint16_t index = 0; index < frame.pixels.size(); ++index) {
        if (frame.pixels[index] == 0x00FFFFFFU) {
            indices.push_back(index);
        }
    }
    return indices;
}

size_t maximumFramesInOneSecond(const std::vector<TraceFrame>& frames) {
    size_t maximum = 0;
    for (size_t start = 0; start < frames.size(); ++start) {
        size_t end = start;
        while (end < frames.size() && frames[end].atMs < frames[start].atMs + 1000U) {
            ++end;
        }
        maximum = std::max(maximum, end - start);
    }
    return maximum;
}

size_t maximumLitFramesInOneSecond(const std::vector<TraceFrame>& frames) {
    size_t maximum = 0;
    for (size_t start = 0; start < frames.size(); ++start) {
        size_t litFrames = 0;
        for (size_t index = start;
             index < frames.size() && frames[index].atMs < frames[start].atMs + 1000U;
             ++index) {
            if (countWhitePixels(frames[index]) > 0) {
                ++litFrames;
            }
        }
        maximum = std::max(maximum, litFrames);
    }
    return maximum;
}

void assertLocalizedSparkleTrace(
    uint8_t mode,
    size_t minimumWhitePixels,
    size_t maximumWhitePixels) {
    for (uint16_t speed : {static_cast<uint16_t>(50),
                           static_cast<uint16_t>(500),
                           static_cast<uint16_t>(1000),
                           static_cast<uint16_t>(65535)}) {
        const std::vector<TraceFrame> frames = renderTrace(
            mode,
            speed,
            kLocalizedTraceDurationMs);
        TEST_ASSERT_FALSE(frames.empty());
        TEST_ASSERT_LESS_OR_EQUAL_UINT32(3, maximumFramesInOneSecond(frames));
        if (frames.size() > 1) {
            const uint32_t expectedDelay =
                WS2812FX_DETAIL::safeLocalizedFlashFrameDelay(speed);
            for (size_t index = 1; index < frames.size(); ++index) {
                TEST_ASSERT_EQUAL_UINT32(
                    expectedDelay,
                    frames[index].atMs - frames[index - 1].atMs);
            }
        }
        for (const TraceFrame& frame : frames) {
            TEST_ASSERT_GREATER_OR_EQUAL_UINT32(minimumWhitePixels, countWhitePixels(frame));
            TEST_ASSERT_LESS_OR_EQUAL_UINT32(maximumWhitePixels, countWhitePixels(frame));
        }
        TEST_ASSERT_EQUAL_UINT32(0, Adafruit_NeoPixel::outOfBoundsWrites);
        TEST_ASSERT_EQUAL_UINT32(frames.size(), Adafruit_NeoPixel::showCalls);
    }
}

void assertChaseFlashTrace(uint8_t mode) {
    for (uint16_t speed : {static_cast<uint16_t>(50),
                           static_cast<uint16_t>(500),
                           static_cast<uint16_t>(1000),
                           static_cast<uint16_t>(65535)}) {
        const uint32_t expectedCycleMs = std::max<uint32_t>(speed, 1000U);
        const std::vector<TraceFrame> frames = renderTrace(
            mode,
            speed,
            expectedCycleMs * 2U + 1U);
        TEST_ASSERT_GREATER_OR_EQUAL_UINT32(9, frames.size());

        // Each cycle is lit, dark, lit, dark and advances only after that
        // final dark frame. The minimum cycle is one second.
        TEST_ASSERT_EQUAL_UINT32(2, countWhitePixels(frames[0]));
        TEST_ASSERT_EQUAL_UINT32(0, countWhitePixels(frames[1]));
        TEST_ASSERT_EQUAL_UINT32(2, countWhitePixels(frames[2]));
        TEST_ASSERT_EQUAL_UINT32(0, countWhitePixels(frames[3]));
        TEST_ASSERT_EQUAL_UINT32(expectedCycleMs, frames[4].atMs - frames[0].atMs);
        TEST_ASSERT_EQUAL_UINT32(expectedCycleMs, frames[8].atMs - frames[4].atMs);
        TEST_ASSERT_LESS_OR_EQUAL_UINT32(2, maximumLitFramesInOneSecond(frames));

        for (size_t cycleStart : {static_cast<size_t>(0), static_cast<size_t>(4)}) {
            TEST_ASSERT_EQUAL_UINT32(80, frames[cycleStart + 1].atMs - frames[cycleStart].atMs);
            TEST_ASSERT_EQUAL_UINT32(170, frames[cycleStart + 2].atMs - frames[cycleStart + 1].atMs);
            TEST_ASSERT_EQUAL_UINT32(80, frames[cycleStart + 3].atMs - frames[cycleStart + 2].atMs);
            TEST_ASSERT_EQUAL_UINT32(
                expectedCycleMs - 330U,
                frames[cycleStart + 4].atMs - frames[cycleStart + 3].atMs);
            TEST_ASSERT_EQUAL_UINT32(0, countWhitePixels(frames[cycleStart + 1]));
            TEST_ASSERT_EQUAL_UINT32(0, countWhitePixels(frames[cycleStart + 3]));
            TEST_ASSERT_TRUE(
                whitePixelIndices(frames[cycleStart]) ==
                whitePixelIndices(frames[cycleStart + 2]));
            TEST_ASSERT_FALSE(
                whitePixelIndices(frames[cycleStart]) ==
                whitePixelIndices(frames[cycleStart + 4]));
        }

        for (const TraceFrame& frame : frames) {
            TEST_ASSERT_LESS_OR_EQUAL_UINT32(2, countWhitePixels(frame));
        }
        TEST_ASSERT_EQUAL_UINT32(0, Adafruit_NeoPixel::outOfBoundsWrites);
        TEST_ASSERT_EQUAL_UINT32(frames.size(), Adafruit_NeoPixel::showCalls);
    }
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_real_flash_sparkle_trace_is_rate_limited() {
    assertLocalizedSparkleTrace(FX_MODE_FLASH_SPARKLE, 1, 1);
}

void test_real_hyper_sparkle_trace_is_rate_limited() {
    assertLocalizedSparkleTrace(FX_MODE_HYPER_SPARKLE, 1, 8);
}

void test_real_chase_flash_trace_is_a_bounded_double_pulse() {
    assertChaseFlashTrace(FX_MODE_CHASE_FLASH);
}

void test_real_random_chase_flash_trace_is_a_bounded_double_pulse() {
    assertChaseFlashTrace(FX_MODE_CHASE_FLASH_RANDOM);
}

void test_localized_flash_modes_stay_inside_short_offset_segments() {
    for (uint8_t mode : {FX_MODE_FLASH_SPARKLE,
                         FX_MODE_HYPER_SPARKLE,
                         FX_MODE_CHASE_FLASH,
                         FX_MODE_CHASE_FLASH_RANDOM}) {
        for (const auto& segment : {std::array<uint16_t, 2>{7, 7},
                                    std::array<uint16_t, 2>{9, 10},
                                    std::array<uint16_t, 2>{17, 23}}) {
            for (bool reverse : {false, true}) {
                const std::vector<TraceFrame> frames = renderTrace(
                    mode,
                    50,
                    2001,
                    segment[0],
                    segment[1],
                    reverse);
                TEST_ASSERT_FALSE(frames.empty());
                TEST_ASSERT_EQUAL_UINT32(0, Adafruit_NeoPixel::outOfBoundsWrites);
                for (const TraceFrame& frame : frames) {
                    for (uint16_t index = 0; index < frame.pixels.size(); ++index) {
                        if (index < segment[0] || index > segment[1]) {
                            TEST_ASSERT_EQUAL_HEX32(0, frame.pixels[index]);
                        }
                    }
                }
            }
        }
    }
}

void test_host_neopixel_stub_matches_transport_order_and_brightness_contract() {
    Adafruit_NeoPixel rgb(1, 0, NEO_RGB + NEO_KHZ800);
    rgb.setBrightness(255);
    rgb.setPixelColor(0, 0x123456);
    TEST_ASSERT_EQUAL_HEX8(0x12, rgb.getPixels()[0]);
    TEST_ASSERT_EQUAL_HEX8(0x34, rgb.getPixels()[1]);
    TEST_ASSERT_EQUAL_HEX8(0x56, rgb.getPixels()[2]);
    TEST_ASSERT_EQUAL_HEX32(0x123456, rgb.getPixelColor(0));

    Adafruit_NeoPixel grb(1, 0, NEO_GRB + NEO_KHZ800);
    grb.setBrightness(255);
    grb.setPixelColor(0, 0x123456);
    TEST_ASSERT_EQUAL_HEX8(0x34, grb.getPixels()[0]);
    TEST_ASSERT_EQUAL_HEX8(0x12, grb.getPixels()[1]);
    TEST_ASSERT_EQUAL_HEX8(0x56, grb.getPixels()[2]);
    TEST_ASSERT_EQUAL_HEX32(0x123456, grb.getPixelColor(0));

    rgb.clear();
    rgb.setBrightness(16);
    rgb.setPixelColor(0, 0x804020);
    TEST_ASSERT_EQUAL_HEX8(0x08, rgb.getPixels()[0]);
    TEST_ASSERT_EQUAL_HEX8(0x04, rgb.getPixels()[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02, rgb.getPixels()[2]);
    TEST_ASSERT_EQUAL_HEX32(0x783C1E, rgb.getPixelColor(0));

    rgb.clear();
    rgb.setBrightness(255);
    rgb.setPixelColor(0, 0x804020);
    rgb.setBrightness(2);
    TEST_ASSERT_EQUAL_HEX32(0x550000, rgb.getPixelColor(0));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_real_flash_sparkle_trace_is_rate_limited);
    RUN_TEST(test_real_hyper_sparkle_trace_is_rate_limited);
    RUN_TEST(test_real_chase_flash_trace_is_a_bounded_double_pulse);
    RUN_TEST(test_real_random_chase_flash_trace_is_a_bounded_double_pulse);
    RUN_TEST(test_localized_flash_modes_stay_inside_short_offset_segments);
    RUN_TEST(test_host_neopixel_stub_matches_transport_order_and_brightness_contract);
    return UNITY_END();
}
