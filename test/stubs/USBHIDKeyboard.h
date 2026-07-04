#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>

inline std::string g_usb_hid_keyboard_last_print;

class USBHIDKeyboard {
public:
    void begin() {}
    void end() {}
    size_t print(const char* s) {
        g_usb_hid_keyboard_last_print = s ? s : "";
        return g_usb_hid_keyboard_last_print.size();
    }
    size_t println(const char* s) {
        g_usb_hid_keyboard_last_print = s ? std::string(s) + "\n" : "\n";
        return g_usb_hid_keyboard_last_print.size();
    }
    size_t write(uint8_t k) { return 1; }
    size_t press(uint8_t k) { return 1; }
    size_t release(uint8_t k) { return 1; }
    void releaseAll() {}
};

class USBHIDConsumerControl {
public:
    void begin() {}
    void end() {}
    void press(uint16_t k) {}
    void release() {}
};

class USBHIDSystemControl {
public:
    void begin() {}
    void end() {}
    void press(uint8_t k) {}
    void release() {}
};

// Key definitions
#define KEY_LEFT_CTRL   0x80
#define KEY_LEFT_SHIFT  0x81
#define KEY_LEFT_ALT    0x82
#define KEY_LEFT_GUI    0x83
#define KEY_RIGHT_CTRL  0x84
#define KEY_RIGHT_SHIFT 0x85
#define KEY_RIGHT_ALT   0x86
#define KEY_RIGHT_GUI   0x87

// FreeRTOS Stubs for Native Test
#include <freertos/semphr.h>
