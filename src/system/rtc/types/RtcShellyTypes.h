#pragma once

#include <Arduino.h>
#include "../../../shelly/ShellyTypes.h"

namespace RTC {

/** Maximum number of Shelly devices */
constexpr uint8_t kMaxShellyDevices = 4;

/**
 * Full Shelly device storage used by runtime/PSRAM config store.
 */
struct __attribute__((packed)) ShellyData {
    SHELLY::ShellyDevice devices[kMaxShellyDevices];
    uint8_t deviceCount = 0;
};

/**
 * Retained summary stored in RTC config.
 */
struct __attribute__((packed)) ShellySummaryData {
    uint8_t deviceCount = 0;
    uint8_t enabledCount = 0;
};

} // namespace RTC
