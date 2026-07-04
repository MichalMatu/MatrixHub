#include <unity.h>

#include "../../src/system/usb/UsbBootPolicy.h"
#include "../../src/system/usb/UsbBootPolicy.cpp"

namespace RTC {
const ConfigStore& getConfig() {
	static ConfigStore config{};
	return config;
}
} // namespace RTC

void setUp(void) {}
void tearDown(void) {}

void test_usb_boot_policy_returns_false_when_all_usb_features_are_off() {
    RTC::ConfigStore config{};
    config.keyboard.enabled = false;
    config.macros.enabled = false;
    config.usbTerminal.enabled = false;
    config.airMouse.movementEnabled = false;
    config.airMouse.clickEnabled = false;
    config.airMouse.jiggler.mode = RTC::MouseJigglerMode::JIGGLER_OFF;

    TEST_ASSERT_FALSE(SYSTEM::UsbBootPolicy::shouldStartUsbOnBoot(config));
    TEST_ASSERT_FALSE(SYSTEM::UsbBootPolicy::shouldStartAirMouseServiceOnBoot(config));
    TEST_ASSERT_FALSE(SYSTEM::UsbBootPolicy::shouldStartUsbTerminalServiceOnBoot(config));
}

void test_usb_boot_policy_starts_for_keyboard_feature() {
    RTC::ConfigStore config{};
    config.keyboard.enabled = true;
    config.macros.enabled = false;
    config.usbTerminal.enabled = false;
    config.airMouse.movementEnabled = false;
    config.airMouse.clickEnabled = false;
    config.airMouse.jiggler.mode = RTC::MouseJigglerMode::JIGGLER_OFF;

    TEST_ASSERT_TRUE(SYSTEM::UsbBootPolicy::shouldStartUsbOnBoot(config));
    TEST_ASSERT_TRUE(SYSTEM::UsbBootPolicy::shouldStartKeyboardServiceOnBoot(config));
    TEST_ASSERT_FALSE(SYSTEM::UsbBootPolicy::shouldStartAirMouseServiceOnBoot(config));
}

void test_usb_boot_policy_starts_for_macros_or_usb_terminal() {
    RTC::ConfigStore macrosConfig{};
    macrosConfig.keyboard.enabled = false;
    macrosConfig.macros.enabled = true;
    macrosConfig.usbTerminal.enabled = false;
    macrosConfig.airMouse.movementEnabled = false;
    macrosConfig.airMouse.clickEnabled = false;
    macrosConfig.airMouse.jiggler.mode = RTC::MouseJigglerMode::JIGGLER_OFF;

    TEST_ASSERT_TRUE(SYSTEM::UsbBootPolicy::shouldStartUsbOnBoot(macrosConfig));
    TEST_ASSERT_TRUE(SYSTEM::UsbBootPolicy::shouldStartKeyboardServiceOnBoot(macrosConfig));
    TEST_ASSERT_FALSE(SYSTEM::UsbBootPolicy::shouldStartUsbTerminalServiceOnBoot(macrosConfig));

    RTC::ConfigStore terminalConfig{};
    terminalConfig.keyboard.enabled = false;
    terminalConfig.macros.enabled = false;
    terminalConfig.usbTerminal.enabled = true;
    terminalConfig.airMouse.movementEnabled = false;
    terminalConfig.airMouse.clickEnabled = false;
    terminalConfig.airMouse.jiggler.mode = RTC::MouseJigglerMode::JIGGLER_OFF;

    TEST_ASSERT_TRUE(SYSTEM::UsbBootPolicy::shouldStartUsbOnBoot(terminalConfig));
    TEST_ASSERT_TRUE(SYSTEM::UsbBootPolicy::shouldStartKeyboardServiceOnBoot(terminalConfig));
    TEST_ASSERT_TRUE(SYSTEM::UsbBootPolicy::shouldStartUsbTerminalServiceOnBoot(terminalConfig));
}

void test_usb_boot_policy_starts_for_any_airmouse_usb_usage() {
    RTC::ConfigStore movementConfig{};
    movementConfig.keyboard.enabled = false;
    movementConfig.macros.enabled = false;
    movementConfig.usbTerminal.enabled = false;
    movementConfig.airMouse.movementEnabled = true;
    movementConfig.airMouse.clickEnabled = false;
    movementConfig.airMouse.jiggler.mode = RTC::MouseJigglerMode::JIGGLER_OFF;

    TEST_ASSERT_TRUE(SYSTEM::UsbBootPolicy::shouldStartUsbOnBoot(movementConfig));
    TEST_ASSERT_TRUE(SYSTEM::UsbBootPolicy::shouldStartAirMouseServiceOnBoot(movementConfig));

    RTC::ConfigStore clickConfig{};
    clickConfig.keyboard.enabled = false;
    clickConfig.macros.enabled = false;
    clickConfig.usbTerminal.enabled = false;
    clickConfig.airMouse.movementEnabled = false;
    clickConfig.airMouse.clickEnabled = true;
    clickConfig.airMouse.jiggler.mode = RTC::MouseJigglerMode::JIGGLER_OFF;

    TEST_ASSERT_TRUE(SYSTEM::UsbBootPolicy::shouldStartUsbOnBoot(clickConfig));
    TEST_ASSERT_TRUE(SYSTEM::UsbBootPolicy::shouldStartAirMouseServiceOnBoot(clickConfig));

    RTC::ConfigStore jigglerConfig{};
    jigglerConfig.keyboard.enabled = false;
    jigglerConfig.macros.enabled = false;
    jigglerConfig.usbTerminal.enabled = false;
    jigglerConfig.airMouse.movementEnabled = false;
    jigglerConfig.airMouse.clickEnabled = false;
    jigglerConfig.airMouse.jiggler.mode = RTC::MouseJigglerMode::JIGGLER_KEYBOARD;

    TEST_ASSERT_TRUE(SYSTEM::UsbBootPolicy::shouldStartUsbOnBoot(jigglerConfig));
    TEST_ASSERT_TRUE(SYSTEM::UsbBootPolicy::shouldStartAirMouseServiceOnBoot(jigglerConfig));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_usb_boot_policy_returns_false_when_all_usb_features_are_off);
    RUN_TEST(test_usb_boot_policy_starts_for_keyboard_feature);
    RUN_TEST(test_usb_boot_policy_starts_for_macros_or_usb_terminal);
    RUN_TEST(test_usb_boot_policy_starts_for_any_airmouse_usb_usage);
    return UNITY_END();
}
