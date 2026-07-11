#pragma once

#include <cstddef>
#include <cstdint>

struct Popcorn {
    float position;
    float velocity;
    int32_t color;
};

struct Oscillator {
    uint8_t size;
    uint16_t pos;
    int8_t speed;
};

struct WS2812FXEffectRuntime {
    uint16_t comets[6] = {};
    Popcorn popcorn[5] = {};
    float popcornCoeff = 0.0f;
    Oscillator oscillators[2] = {};
};

namespace WS2812FX_DETAIL {

constexpr size_t kNoRuntimeSlot = static_cast<size_t>(-1);

constexpr size_t findActiveSegmentSlot(
    const uint8_t* activeSegments,
    size_t activeSegmentCount,
    uint8_t segment) {
    if (activeSegments == nullptr) {
        return kNoRuntimeSlot;
    }
    for (size_t slot = 0; slot < activeSegmentCount; ++slot) {
        if (activeSegments[slot] == segment) {
            return slot;
        }
    }
    return kNoRuntimeSlot;
}

inline void initializeComets(WS2812FXEffectRuntime& state, uint16_t segmentLength) {
    for (uint16_t& comet : state.comets) {
        comet = segmentLength;
    }
}

inline void initializePopcorn(WS2812FXEffectRuntime& state) {
    for (Popcorn& kernel : state.popcorn) {
        kernel.position = -1.0f;
        kernel.velocity = 0.0f;
        kernel.color = 0;
    }
}

inline void initializeOscillators(WS2812FXEffectRuntime& state, uint16_t segmentLength) {
    uint8_t size = static_cast<uint8_t>(segmentLength / 4U);
    if (size == 0) {
        size = 1;
    }
    const uint16_t lastPixel = segmentLength > 0 ? static_cast<uint16_t>(segmentLength - 1U) : 0;
    state.oscillators[0] = {size, 0, 1};
    state.oscillators[1] = {size, lastPixel, -2};
}

}  // namespace WS2812FX_DETAIL
