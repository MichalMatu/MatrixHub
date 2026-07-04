#pragma once

#include <stdint.h>

namespace AIRMOUSE {

// Lightweight interfaces used to decouple jiggler/logic from concrete services.

/**
 * @brief Interface for mouse movement
 */
class IMouseMover {
public:
    virtual void move(int x, int y) = 0;
    virtual ~IMouseMover() = default;
};

/**
 * @brief Interface for keyboard actions
 */
class IKeyboardSender {
public:
    virtual void sendKey(uint8_t key) = 0;
    virtual void releaseAll() = 0;
    virtual ~IKeyboardSender() = default;
};

/**
 * @brief Interface for time provider
 */
class ITimeProvider {
public:
    virtual uint32_t getMillis() = 0;
    virtual int getRandomRange(int min, int max) = 0;
    virtual ~ITimeProvider() = default;
};

} // namespace AIRMOUSE
