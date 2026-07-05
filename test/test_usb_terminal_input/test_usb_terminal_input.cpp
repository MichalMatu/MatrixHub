#include <unity.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

#include "freertos/task.h"
#include "keyboard/KeyboardService.h"
#include "usb_terminal/input/TerminalInput.h"
#include "../../src/keyboard/KeyboardService.cpp"
#include "../../src/usb_terminal/input/TerminalInput.cpp"
#include "../stubs/USBHIDKeyboard.h"

namespace LOG {
    Settings Logging::_settings;
    void Logging::log(esp_log_level_t level, const char *tag, const char *fmt, ...) {
        (void)level;
        va_list args;
        va_start(args, fmt);
        printf("[%s] ", tag);
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
    }
}

namespace {

constexpr const char* kAutoPortProbeExpected =
    "p=\"$(for p in /dev/serial/by-id/*Espressif* /dev/serial/by-id/*ESP* "
    "/dev/serial/by-id/*esp* /dev/serial/by-id/*Waveshare* "
    "/dev/serial/by-id/*USB_JTAG* /dev/ttyACM* /dev/ttyUSB* "
    "/dev/cu.usbmodem* /dev/tty.usbmodem* /dev/cu.usbserial* /dev/tty.usbserial* "
    "/dev/serial/by-id/*; do [ -e \"$p\" ] && { printf '%s\\n' \"$p\"; break; }; done)\"; "
    "[ -n \"$p\" ] && ";

}

void setUp(void) {
    g_usb_hid_keyboard_last_print.clear();
}

void tearDown(void) {}

void test_normal_command_wraps_entire_command_group() {
    KEYBOARD::KeyboardService keyboard;
    TEST_ASSERT_TRUE(keyboard.begin());

    USB_TERMINAL::TerminalInput input(&keyboard);
    const char* cmd = "ls -la";
    TEST_ASSERT_TRUE(input.sendCommand("/dev/ttyUSB0", cmd, strlen(cmd)));
    TEST_ASSERT_EQUAL_STRING(
        "{ printf '[MatrixHub] target port: %s\\n' '/dev/ttyUSB0';\n"
        "ls -la\n"
        "} > '/dev/ttyUSB0' 2>&1\n",
        g_usb_hid_keyboard_last_print.c_str());
}

void test_pwd_command_wraps_entire_command_group() {
    KEYBOARD::KeyboardService keyboard;
    TEST_ASSERT_TRUE(keyboard.begin());

    USB_TERMINAL::TerminalInput input(&keyboard);
    const char* cmd = "pwd";
    TEST_ASSERT_TRUE(input.sendCommand("/dev/ttyUSB0", cmd, strlen(cmd)));
    TEST_ASSERT_EQUAL_STRING(
        "{ printf '[MatrixHub] target port: %s\\n' '/dev/ttyUSB0';\n"
        "pwd\n"
        "} > '/dev/ttyUSB0' 2>&1\n",
        g_usb_hid_keyboard_last_print.c_str());
}

void test_standalone_cd_command_adds_pwd_wrapper() {
    KEYBOARD::KeyboardService keyboard;
    TEST_ASSERT_TRUE(keyboard.begin());

    USB_TERMINAL::TerminalInput input(&keyboard);
    const char* cmd = "cd /tmp";
    TEST_ASSERT_TRUE(input.sendCommand("/dev/ttyUSB0", cmd, strlen(cmd)));
    TEST_ASSERT_EQUAL_STRING(
        "{ printf '[MatrixHub] target port: %s\\n' '/dev/ttyUSB0';\n"
        "cd /tmp\n"
        "pwd\n"
        "} > '/dev/ttyUSB0' 2>&1\n",
        g_usb_hid_keyboard_last_print.c_str());
}

void test_cd_dash_adds_pwd_wrapper() {
    KEYBOARD::KeyboardService keyboard;
    TEST_ASSERT_TRUE(keyboard.begin());

    USB_TERMINAL::TerminalInput input(&keyboard);
    const char* cmd = "cd -";
    TEST_ASSERT_TRUE(input.sendCommand("/dev/ttyUSB0", cmd, strlen(cmd)));
    TEST_ASSERT_EQUAL_STRING(
        "{ printf '[MatrixHub] target port: %s\\n' '/dev/ttyUSB0';\n"
        "cd -\n"
        "pwd\n"
        "} > '/dev/ttyUSB0' 2>&1\n",
        g_usb_hid_keyboard_last_print.c_str());
}

void test_cd_with_shell_operator_wraps_entire_command_group() {
    KEYBOARD::KeyboardService keyboard;
    TEST_ASSERT_TRUE(keyboard.begin());

    USB_TERMINAL::TerminalInput input(&keyboard);
    const char* cmd = "cd /tmp && ls";
    TEST_ASSERT_TRUE(input.sendCommand("/dev/ttyUSB0", cmd, strlen(cmd)));
    TEST_ASSERT_EQUAL_STRING(
        "{ printf '[MatrixHub] target port: %s\\n' '/dev/ttyUSB0';\n"
        "cd /tmp && ls\n"
        "} > '/dev/ttyUSB0' 2>&1\n",
        g_usb_hid_keyboard_last_print.c_str());
}

void test_shell_list_wraps_entire_command_group() {
    KEYBOARD::KeyboardService keyboard;
    TEST_ASSERT_TRUE(keyboard.begin());

    USB_TERMINAL::TerminalInput input(&keyboard);
    const char* cmd = "printf 'A\\n'; uname -a; pwd";
    TEST_ASSERT_TRUE(input.sendCommand("/dev/ttyUSB0", cmd, strlen(cmd)));
    TEST_ASSERT_EQUAL_STRING(
        "{ printf '[MatrixHub] target port: %s\\n' '/dev/ttyUSB0';\n"
        "printf 'A\\n'; uname -a; pwd\n"
        "} > '/dev/ttyUSB0' 2>&1\n",
        g_usb_hid_keyboard_last_print.c_str());
}

void test_invalid_target_port_is_rejected() {
    KEYBOARD::KeyboardService keyboard;
    TEST_ASSERT_TRUE(keyboard.begin());

    USB_TERMINAL::TerminalInput input(&keyboard);
    const char* cmd = "uname -a";
    TEST_ASSERT_FALSE(input.sendCommand("/dev/ttyUSB0; echo hacked", cmd, strlen(cmd)));
    TEST_ASSERT_TRUE(g_usb_hid_keyboard_last_print.empty());
}

void test_target_port_validation_normalizes_auto_and_accepts_device_paths() {
    char normalized[32] = {0};

    TEST_ASSERT_TRUE(USB_TERMINAL::copyNormalizedTargetPort(normalized, sizeof(normalized), "AUTO"));
    TEST_ASSERT_EQUAL_STRING("auto", normalized);

    TEST_ASSERT_TRUE(USB_TERMINAL::copyNormalizedTargetPort(
        normalized,
        sizeof(normalized),
        "/dev/serial/by-id/ESP32-S3_01"));
    TEST_ASSERT_EQUAL_STRING("/dev/serial/by-id/ESP32-S3_01", normalized);
}

void test_target_port_validation_rejects_shell_metacharacters() {
    char normalized[32] = {0};

    TEST_ASSERT_FALSE(USB_TERMINAL::copyNormalizedTargetPort(
        normalized,
        sizeof(normalized),
        "/dev/ttyUSB0;echo hacked"));
    TEST_ASSERT_FALSE(USB_TERMINAL::isValidTargetPortSetting("/dev/ttyUSB0 $(id)"));
}

void test_auto_target_port_adds_probe_before_redirection() {
    KEYBOARD::KeyboardService keyboard;
    TEST_ASSERT_TRUE(keyboard.begin());

    USB_TERMINAL::TerminalInput input(&keyboard);
    const char* cmd = "uname -a";
    TEST_ASSERT_TRUE(input.sendCommand("auto", cmd, strlen(cmd)));

    const std::string expected = std::string(kAutoPortProbeExpected) +
        "{ printf '[MatrixHub] target port: %s\\n' \"$p\";\n"
        "uname -a\n"
        "} > \"$p\" 2>&1\n";
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), g_usb_hid_keyboard_last_print.c_str());
}

void test_auto_target_port_wraps_standalone_cd_with_pwd() {
    KEYBOARD::KeyboardService keyboard;
    TEST_ASSERT_TRUE(keyboard.begin());

    USB_TERMINAL::TerminalInput input(&keyboard);
    const char* cmd = "cd /tmp";
    TEST_ASSERT_TRUE(input.sendCommand("AUTO", cmd, strlen(cmd)));

    const std::string expected = std::string(kAutoPortProbeExpected) +
        "{ printf '[MatrixHub] target port: %s\\n' \"$p\";\n"
        "cd /tmp\n"
        "pwd\n"
        "} > \"$p\" 2>&1\n";
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), g_usb_hid_keyboard_last_print.c_str());
}

void test_auto_target_port_wraps_shell_list() {
    KEYBOARD::KeyboardService keyboard;
    TEST_ASSERT_TRUE(keyboard.begin());

    USB_TERMINAL::TerminalInput input(&keyboard);
    const char* cmd = "printf 'A\\n'; uname -a; pwd";
    TEST_ASSERT_TRUE(input.sendCommand("auto", cmd, strlen(cmd)));

    const std::string expected = std::string(kAutoPortProbeExpected) +
        "{ printf '[MatrixHub] target port: %s\\n' \"$p\";\n"
        "printf 'A\\n'; uname -a; pwd\n"
        "} > \"$p\" 2>&1\n";
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), g_usb_hid_keyboard_last_print.c_str());
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_normal_command_wraps_entire_command_group);
    RUN_TEST(test_pwd_command_wraps_entire_command_group);
    RUN_TEST(test_standalone_cd_command_adds_pwd_wrapper);
    RUN_TEST(test_cd_dash_adds_pwd_wrapper);
    RUN_TEST(test_cd_with_shell_operator_wraps_entire_command_group);
    RUN_TEST(test_shell_list_wraps_entire_command_group);
    RUN_TEST(test_invalid_target_port_is_rejected);
    RUN_TEST(test_target_port_validation_normalizes_auto_and_accepts_device_paths);
    RUN_TEST(test_target_port_validation_rejects_shell_metacharacters);
    RUN_TEST(test_auto_target_port_adds_probe_before_redirection);
    RUN_TEST(test_auto_target_port_wraps_standalone_cd_with_pwd);
    RUN_TEST(test_auto_target_port_wraps_shell_list);
    return UNITY_END();
}
