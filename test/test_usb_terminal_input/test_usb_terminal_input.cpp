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

void setUp(void) {
    g_usb_hid_keyboard_last_print.clear();
}

void tearDown(void) {}

void test_normal_command_keeps_existing_redirection() {
    KEYBOARD::KeyboardService keyboard;
    TEST_ASSERT_TRUE(keyboard.begin());

    USB_TERMINAL::TerminalInput input(&keyboard);
    const char* cmd = "ls -la";
    TEST_ASSERT_TRUE(input.sendCommand("/dev/ttyUSB0", cmd, strlen(cmd)));
    TEST_ASSERT_EQUAL_STRING("ls -la > /dev/ttyUSB0 2>&1\n", g_usb_hid_keyboard_last_print.c_str());
}

void test_pwd_command_is_not_wrapped() {
    KEYBOARD::KeyboardService keyboard;
    TEST_ASSERT_TRUE(keyboard.begin());

    USB_TERMINAL::TerminalInput input(&keyboard);
    const char* cmd = "pwd";
    TEST_ASSERT_TRUE(input.sendCommand("/dev/ttyUSB0", cmd, strlen(cmd)));
    TEST_ASSERT_EQUAL_STRING("pwd > /dev/ttyUSB0 2>&1\n", g_usb_hid_keyboard_last_print.c_str());
}

void test_standalone_cd_command_adds_pwd_wrapper() {
    KEYBOARD::KeyboardService keyboard;
    TEST_ASSERT_TRUE(keyboard.begin());

    USB_TERMINAL::TerminalInput input(&keyboard);
    const char* cmd = "cd /tmp";
    TEST_ASSERT_TRUE(input.sendCommand("/dev/ttyUSB0", cmd, strlen(cmd)));
    TEST_ASSERT_EQUAL_STRING("{ cd /tmp; pwd; } > /dev/ttyUSB0 2>&1\n", g_usb_hid_keyboard_last_print.c_str());
}

void test_cd_dash_adds_pwd_wrapper() {
    KEYBOARD::KeyboardService keyboard;
    TEST_ASSERT_TRUE(keyboard.begin());

    USB_TERMINAL::TerminalInput input(&keyboard);
    const char* cmd = "cd -";
    TEST_ASSERT_TRUE(input.sendCommand("/dev/ttyUSB0", cmd, strlen(cmd)));
    TEST_ASSERT_EQUAL_STRING("{ cd -; pwd; } > /dev/ttyUSB0 2>&1\n", g_usb_hid_keyboard_last_print.c_str());
}

void test_cd_with_shell_operator_is_not_wrapped() {
    KEYBOARD::KeyboardService keyboard;
    TEST_ASSERT_TRUE(keyboard.begin());

    USB_TERMINAL::TerminalInput input(&keyboard);
    const char* cmd = "cd /tmp && ls";
    TEST_ASSERT_TRUE(input.sendCommand("/dev/ttyUSB0", cmd, strlen(cmd)));
    TEST_ASSERT_EQUAL_STRING("cd /tmp && ls > /dev/ttyUSB0 2>&1\n", g_usb_hid_keyboard_last_print.c_str());
}

void test_auto_target_port_adds_probe_before_redirection() {
    KEYBOARD::KeyboardService keyboard;
    TEST_ASSERT_TRUE(keyboard.begin());

    USB_TERMINAL::TerminalInput input(&keyboard);
    const char* cmd = "uname -a";
    TEST_ASSERT_TRUE(input.sendCommand("auto", cmd, strlen(cmd)));

    const char* expected =
        "p=\"$(for p in /dev/serial/by-id/*Espressif* /dev/serial/by-id/*ESP* "
        "/dev/serial/by-id/*esp* /dev/serial/by-id/*Waveshare* "
        "/dev/serial/by-id/*USB_JTAG* /dev/ttyACM* /dev/ttyUSB* "
        "/dev/cu.usbmodem* /dev/tty.usbmodem* /dev/cu.usbserial* /dev/tty.usbserial* "
        "/dev/serial/by-id/*; do [ -e \"$p\" ] && { printf '%s\\n' \"$p\"; break; }; done)\"; "
        "[ -n \"$p\" ] && uname -a > \"$p\" 2>&1\n";
    TEST_ASSERT_EQUAL_STRING(expected, g_usb_hid_keyboard_last_print.c_str());
}

void test_auto_target_port_wraps_standalone_cd_with_pwd() {
    KEYBOARD::KeyboardService keyboard;
    TEST_ASSERT_TRUE(keyboard.begin());

    USB_TERMINAL::TerminalInput input(&keyboard);
    const char* cmd = "cd /tmp";
    TEST_ASSERT_TRUE(input.sendCommand("AUTO", cmd, strlen(cmd)));

    const char* expected =
        "p=\"$(for p in /dev/serial/by-id/*Espressif* /dev/serial/by-id/*ESP* "
        "/dev/serial/by-id/*esp* /dev/serial/by-id/*Waveshare* "
        "/dev/serial/by-id/*USB_JTAG* /dev/ttyACM* /dev/ttyUSB* "
        "/dev/cu.usbmodem* /dev/tty.usbmodem* /dev/cu.usbserial* /dev/tty.usbserial* "
        "/dev/serial/by-id/*; do [ -e \"$p\" ] && { printf '%s\\n' \"$p\"; break; }; done)\"; "
        "[ -n \"$p\" ] && { cd /tmp; pwd; } > \"$p\" 2>&1\n";
    TEST_ASSERT_EQUAL_STRING(expected, g_usb_hid_keyboard_last_print.c_str());
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_normal_command_keeps_existing_redirection);
    RUN_TEST(test_pwd_command_is_not_wrapped);
    RUN_TEST(test_standalone_cd_command_adds_pwd_wrapper);
    RUN_TEST(test_cd_dash_adds_pwd_wrapper);
    RUN_TEST(test_cd_with_shell_operator_is_not_wrapped);
    RUN_TEST(test_auto_target_port_adds_probe_before_redirection);
    RUN_TEST(test_auto_target_port_wraps_standalone_cd_with_pwd);
    return UNITY_END();
}
