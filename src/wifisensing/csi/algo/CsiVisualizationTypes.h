#pragma once

#include <cstdint>

namespace WIFISENSING {
namespace CSI {

constexpr uint8_t CSI_VISUALIZATION_BIN_COUNT = 64;

struct CsiVisualizationSnapshot {
    bool valid = false;
    bool stale = true;
    uint32_t timestampMs = 0;
    uint16_t width = 0;
    float value = 0.0f;
    uint8_t bins[CSI_VISUALIZATION_BIN_COUNT] = {};
    uint8_t binCount = 0;
};

} // namespace CSI
} // namespace WIFISENSING
