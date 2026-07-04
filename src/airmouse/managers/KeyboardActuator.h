#pragma once

#include "../../keyboard/KeyboardService.h"
#include "../interfaces/IAirMouseInterfaces.h"

namespace AIRMOUSE {

/**
 * @brief Adapter to use KeyboardService via the IKeyboardSender interface.
 */
class KeyboardActuator : public IKeyboardSender {
public:
    explicit KeyboardActuator(KEYBOARD::KeyboardService& keyboard);

    // IKeyboardSender implementation
    void sendKey(uint8_t key) override;
    void releaseAll() override;

private:
    KEYBOARD::KeyboardService& _keyboard;
};

} // namespace AIRMOUSE
