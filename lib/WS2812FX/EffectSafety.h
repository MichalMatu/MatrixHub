#pragma once

#include <cstddef>
#include <cstdint>

namespace WS2812FX_DETAIL {

constexpr uint16_t kMinimumFlashPeriodMs = 500;
constexpr uint16_t kFlashPulseMs = 80;
constexpr uint16_t kMultiFlashCycleMs = 1000;
constexpr uint16_t kMultiFlashGapMs = 170;
// A localized effect frame can turn several pixels on and off at once. Keeping
// the frame cadence at or below three updates per second is a conservative
// product rule that does not depend on viewing distance or illuminated area.
constexpr uint16_t kMinimumLocalizedFlashFrameMs = 334;

constexpr uint16_t safeFlashPeriod(uint16_t requestedPeriodMs) {
    return requestedPeriodMs < kMinimumFlashPeriodMs
        ? kMinimumFlashPeriodMs
        : requestedPeriodMs;
}

constexpr uint16_t safeBlinkPhaseDelay(uint16_t requestedPeriodMs) {
    return static_cast<uint16_t>(safeFlashPeriod(requestedPeriodMs) / 2U);
}

constexpr uint16_t safeStrobePhaseDelay(uint16_t requestedPeriodMs, bool litFrame) {
    const uint16_t period = safeFlashPeriod(requestedPeriodMs);
    return litFrame ? kFlashPulseMs : static_cast<uint16_t>(period - kFlashPulseMs);
}

constexpr uint16_t safeMultiStrobePhaseDelay(uint16_t requestedPeriodMs, uint8_t phase) {
    const uint16_t cycle = requestedPeriodMs < kMultiFlashCycleMs
        ? kMultiFlashCycleMs
        : requestedPeriodMs;
    switch (phase & 0x03U) {
        case 0:
        case 2:
            return kFlashPulseMs;
        case 1:
            return kMultiFlashGapMs;
        default:
            return static_cast<uint16_t>(
                cycle - (2U * kFlashPulseMs) - kMultiFlashGapMs);
    }
}

constexpr uint16_t safeLocalizedFlashFrameDelay(uint16_t effectSpeedMs) {
    // Keep the legacy WS2812FX speed curve so large saved values do not turn a
    // sparkle into a minute-long static frame, then apply the safety floor.
    const uint16_t requestedPeriodMs = static_cast<uint16_t>(effectSpeedMs / 32U);
    return requestedPeriodMs < kMinimumLocalizedFlashFrameMs
        ? kMinimumLocalizedFlashFrameMs
        : requestedPeriodMs;
}

constexpr bool isDoublePulseLitPhase(uint8_t phase) {
    return (phase & 0x03U) == 0U || (phase & 0x03U) == 2U;
}

constexpr bool isFrameDue(uint32_t nowMs, uint32_t deadlineMs) {
    return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}

constexpr uint32_t immediateFrameDeadline(uint32_t nowMs) {
    return nowMs;
}

constexpr uint32_t selectVisibleColor(
    const uint32_t* colors,
    size_t colorCount,
    uint8_t selection,
    uint32_t fallback) {
    if (colors == nullptr || colorCount == 0) {
        return fallback;
    }

    size_t visibleCount = 0;
    for (size_t index = 0; index < colorCount; ++index) {
        if (colors[index] != 0) {
            ++visibleCount;
        }
    }
    if (visibleCount == 0) {
        return fallback;
    }

    size_t target = static_cast<size_t>(selection) % visibleCount;
    for (size_t index = 0; index < colorCount; ++index) {
        if (colors[index] == 0) {
            continue;
        }
        if (target == 0) {
            return colors[index];
        }
        --target;
    }

    return fallback;
}

}  // namespace WS2812FX_DETAIL
