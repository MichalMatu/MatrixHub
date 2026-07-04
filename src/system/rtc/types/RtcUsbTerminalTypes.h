#pragma once

#include <Arduino.h>
#include "../RtcDefaultValues.h"

namespace RTC {

/**
 * Configuration for USB Terminal Service
 */
struct __attribute__((packed)) UsbTerminalData {
    bool enabled = Defaults::UsbTerminal::Enabled;
    uint32_t idleTimeoutMs = Defaults::UsbTerminal::IdleTimeoutMs;
    char targetPort[32] = "auto";
};

} // namespace RTC
