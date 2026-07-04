#pragma once

#include <Arduino.h>
#include <atomic>
#ifdef ARDUINO
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"
#include "USBHIDSystemControl.h"
#else
#include "../../test/stubs/USBHIDKeyboard.h"
#endif

namespace KEYBOARD {

/**
 * @class KeyboardService
 * @brief Provides text input and macro capabilities via USB HID
 */
class KeyboardService {
public:
    /**
     * @brief Initialize USB Keyboard
     * @return true if successful
     */
    KeyboardService();
    ~KeyboardService();

    /**
     * @brief Initialize USB Keyboard
     * @return true if successful
     */
    bool begin();

    /**
     * @brief Stop/Release resources
     */
    void stop();

    /**
     * @brief Type a string of text
     */
    void type(const char* text);
    
    /**
     * @brief Type a string of text + Enter
     */
    void typeLn(const char* text);

    /**
     * @brief Press and release a single key
     */
    /**
     * @brief Press and release a single key (Tap)
     */
    void press(uint8_t key);

    /**
     * @brief Press and hold a key
     */
    void pressKey(uint8_t key);

    /**
     * @brief Release a specific key
     */
    void releaseKey(uint8_t key);

    /**
     * @brief Press and release a consumer control key (Media)
     */
    void pressConsumer(uint16_t usage);

    /**
     * @brief Send a system control command (Power Off, Sleep, Wake)
     */
    void pressSystem(uint8_t usage);

    /**
     * @brief Press multiple keys simultaneously (e.g. Modifiers + Key), then release all.
     */
    void pressCombo(const uint8_t* keys, size_t count);

    /**
     * @brief Release all pressed keys
     */
    void releaseAll();

    /**
     * @brief Check if KeyboardService is initialized
     */
    bool isInitialized() const { return _initialized; }

private:
    USBHIDKeyboard _keyboard;
    USBHIDConsumerControl _consumer;
    USBHIDSystemControl _system;
    std::atomic<bool> _initialized{false};
    SemaphoreHandle_t _mutex = nullptr; // Protects HID reports
};

} // namespace KEYBOARD
