#pragma once

#include <stdint.h>

inline int8_t g_usb_hid_mouse_last_x = 0;
inline int8_t g_usb_hid_mouse_last_y = 0;
inline uint8_t g_usb_hid_mouse_last_click = 0;

class USBHIDMouse {
public:
    void begin() {}
    void end() {}

    void move(int8_t x, int8_t y) {
        g_usb_hid_mouse_last_x = x;
        g_usb_hid_mouse_last_y = y;
    }

    void click(uint8_t button) {
        g_usb_hid_mouse_last_click = button;
        _buttons = button;
    }

    void press(uint8_t button) {
        _buttons |= button;
    }

    void release(uint8_t button) {
        _buttons &= static_cast<uint8_t>(~button);
    }

    bool isPressed(uint8_t button) const {
        return (_buttons & button) != 0;
    }

private:
    uint8_t _buttons = 0;
};

#ifndef MOUSE_LEFT
#define MOUSE_LEFT 1
#endif
#ifndef MOUSE_RIGHT
#define MOUSE_RIGHT 2
#endif
#ifndef MOUSE_MIDDLE
#define MOUSE_MIDDLE 4
#endif
