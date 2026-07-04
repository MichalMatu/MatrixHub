#include "KeyboardActuator.h"

namespace AIRMOUSE {

KeyboardActuator::KeyboardActuator(KEYBOARD::KeyboardService& keyboard) : _keyboard(keyboard) {}

void KeyboardActuator::sendKey(uint8_t key) {
    _keyboard.press(key); // KeyboardService::press handles basic key press/release (write)
}

void KeyboardActuator::releaseAll() {
    _keyboard.releaseAll();
}

} // namespace AIRMOUSE
