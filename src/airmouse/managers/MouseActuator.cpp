#include "MouseActuator.h"
#include <Arduino.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "../../config/System.h"
#include "../../system/logging/Logging.h"
#include "class/hid/hid_device.h" // For tud_hid_ready()

#undef min
#undef max
#undef abs

#undef LOG_TAG
#define LOG_TAG "MouseAct"

namespace AIRMOUSE {

MouseActuator::MouseActuator(USBHIDMouse& mouse) : _mouse(mouse) {}

void MouseActuator::move(int x, int y) {
    movePrecise(static_cast<float>(x), static_cast<float>(y));
}

void MouseActuator::movePrecise(float x, float y) {
    // Skip if USB HID is not ready (avoids blocking).
    if (!tud_hid_ready()) return;

    // Accumulate subpixel remainders.
    _accumX += x;
    _accumY += y;

    // Clamp accumulated tail per axis to avoid runaway backlog.
    const float maxBacklog = (float)CONFIG::AIR_MOUSE::HID::MAX_BACKLOG;
    _accumX = std::clamp(_accumX, -maxBacklog, maxBacklog);
    _accumY = std::clamp(_accumY, -maxBacklog, maxBacklog);

    const int32_t maxDelta = CONFIG::AIR_MOUSE::HID::REPORT_DELTA_MAX;
    uint8_t reports = 0;

    while (reports < CONFIG::AIR_MOUSE::HID::MAX_REPORTS_PER_TICK) {
        int32_t outX = (int32_t)lroundf(_accumX);
        int32_t outY = (int32_t)lroundf(_accumY);
        if (outX == 0 && outY == 0) break;

        int32_t sendX = outX;
        int32_t sendY = outY;

        const int32_t maxAbs = std::max(std::abs(outX), std::abs(outY));
        if (maxAbs > maxDelta) {
            const float scale = (float)maxDelta / (float)maxAbs;
            sendX = (int32_t)lroundf(outX * scale);
            sendY = (int32_t)lroundf(outY * scale);
        }

        const int8_t hidX = constrainByte(sendX);
        const int8_t hidY = constrainByte(sendY);
        if (hidX == 0 && hidY == 0) break;

        _mouse.move(hidX, hidY);

        // Remove only what was actually sent.
        _accumX -= hidX;
        _accumY -= hidY;

        reports++;
    }
}

void MouseActuator::resetAccum() {
    _accumX = 0.0f;
    _accumY = 0.0f;
}

int8_t MouseActuator::constrainByte(int val) {
    if (val < CONFIG::AIR_MOUSE::HID::REPORT_DELTA_MIN) {
        return static_cast<int8_t>(CONFIG::AIR_MOUSE::HID::REPORT_DELTA_MIN);
    }
    if (val > CONFIG::AIR_MOUSE::HID::REPORT_DELTA_MAX) {
        return static_cast<int8_t>(CONFIG::AIR_MOUSE::HID::REPORT_DELTA_MAX);
    }
    return (int8_t)val;
}

void MouseActuator::clickLeft() {
    if (tud_hid_ready()) _mouse.click(MOUSE_LEFT);
}

void MouseActuator::clickRight() {
    if (tud_hid_ready()) _mouse.click(MOUSE_RIGHT);
}

void MouseActuator::click(uint8_t button) {
    if (tud_hid_ready()) _mouse.click(button);
}

} // namespace AIRMOUSE
