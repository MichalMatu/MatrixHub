#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace BLE {

struct TpData {
    float temperature;
    float humidity;
    uint8_t battery; // 0-100%
    bool valid;
};

struct BleConfig {
    bool enabled = false;
};

} // namespace BLE
