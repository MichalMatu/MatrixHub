#pragma once

#include <cstdint>
#include <USBHIDMouse.h>
#include "../interfaces/IAirMouseInterfaces.h"

namespace AIRMOUSE {

/**
 * @brief Thin wrapper around USBHIDMouse.
 *
 * Encapsulates HID readiness checks, subpixel accumulation, and clamps movement to report size.
 */
class MouseActuator : public IMouseMover {
public:
    MouseActuator(USBHIDMouse& mouse);

    // IMouseMover implementation
    void move(int x, int y) override;
    void movePrecise(float x, float y);
    void resetAccum();

    // Clicks
    void clickLeft();
    void clickRight();
    
    // For specific USB commands if needed (reusing library constants)
    void click(uint8_t button);

private:
    USBHIDMouse& _mouse;
    float _accumX = 0.0f;
    float _accumY = 0.0f;
    
    // HID reports are int8_t deltas.
    int8_t constrainByte(int val);
};

} // namespace AIRMOUSE
