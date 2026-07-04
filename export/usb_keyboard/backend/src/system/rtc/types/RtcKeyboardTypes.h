#pragma once

#include <Arduino.h>
#include "../RtcDefaultValues.h"

namespace RTC {

/**
 * Configuration for the direct keyboard feature exposed via /api/keyboard.
 */
struct __attribute__((packed)) KeyboardData {
    bool enabled = Defaults::Keyboard::Enabled;
};

} // namespace RTC
