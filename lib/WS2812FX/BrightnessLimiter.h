#pragma once

#include <cstdint>

namespace WS2812FX_DETAIL {

constexpr uint8_t spreadTransportChannel(
    uint8_t previous,
    uint8_t current,
    uint8_t next,
    uint8_t channelCeiling) {
    const uint16_t spread = static_cast<uint16_t>(previous >> 2U) +
                            static_cast<uint16_t>(current) +
                            static_cast<uint16_t>(next >> 2U);
    return spread > channelCeiling
        ? channelCeiling
        : static_cast<uint8_t>(spread);
}

}  // namespace WS2812FX_DETAIL
