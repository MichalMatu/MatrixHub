#pragma once

#include <Arduino.h>
#include "../RtcDefaultValues.h"

namespace RTC {

/**
 * @brief Macro Configuration Data (stored in RTC SLOW Memory)
 */
struct __attribute__((packed)) MacroData {
    bool enabled = Defaults::Macro::Enabled;
    uint32_t bootDelay = Defaults::Macro::BootDelayMs;
    char bootScript[64] = {0};      // Fixed size buffer for script filename (LittleFS limit usually 31-63 chars)
    uint8_t _reserved[4] = {0};     // Future padding
};

} // namespace RTC
