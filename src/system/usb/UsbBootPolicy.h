#pragma once

#include "../rtc/RtcConfig.h"

namespace SYSTEM::UsbBootPolicy {

bool airMouseRequiresUsb(const RTC::AirMouseData& config);
bool shouldStartUsbOnBoot(const RTC::ConfigStore& config);
bool shouldStartUsbOnBoot();

bool shouldStartKeyboardServiceOnBoot(const RTC::ConfigStore& config);
bool shouldStartKeyboardServiceOnBoot();

bool shouldStartAirMouseServiceOnBoot(const RTC::ConfigStore& config);
bool shouldStartAirMouseServiceOnBoot();

bool shouldStartUsbTerminalServiceOnBoot(const RTC::ConfigStore& config);
bool shouldStartUsbTerminalServiceOnBoot();

} // namespace SYSTEM::UsbBootPolicy
