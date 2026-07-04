#include "UsbBootPolicy.h"

namespace SYSTEM::UsbBootPolicy {

bool airMouseRequiresUsb(const RTC::AirMouseData& config) {
    // AirMouse BLE HID was removed; if any AirMouse path is enabled, it owns
    // the USB-backed runtime stack through these feature toggles.
    return config.movementEnabled ||
           config.clickEnabled ||
           config.jiggler.mode != RTC::MouseJigglerMode::JIGGLER_OFF;
}

bool shouldStartUsbOnBoot(const RTC::ConfigStore& config) {
    return config.usbTerminal.enabled ||
           config.keyboard.enabled ||
           config.macros.enabled ||
           airMouseRequiresUsb(config.airMouse);
}

bool shouldStartUsbOnBoot() {
    return shouldStartUsbOnBoot(RTC::getConfig());
}

bool shouldStartKeyboardServiceOnBoot(const RTC::ConfigStore& config) {
    return shouldStartUsbOnBoot(config);
}

bool shouldStartKeyboardServiceOnBoot() {
    return shouldStartUsbOnBoot();
}

bool shouldStartAirMouseServiceOnBoot(const RTC::ConfigStore& config) {
    return airMouseRequiresUsb(config.airMouse);
}

bool shouldStartAirMouseServiceOnBoot() {
    return shouldStartAirMouseServiceOnBoot(RTC::getConfig());
}

bool shouldStartUsbTerminalServiceOnBoot(const RTC::ConfigStore& config) {
    return config.usbTerminal.enabled;
}

bool shouldStartUsbTerminalServiceOnBoot() {
    return shouldStartUsbTerminalServiceOnBoot(RTC::getConfig());
}

} // namespace SYSTEM::UsbBootPolicy
