#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <stdint.h>
#include <stddef.h>
#endif
#include "../RtcDefaultValues.h"

namespace RTC {

/**
 * AirMouse no longer supports a selectable BLE HID transport. Keep one retained
 * byte in the struct so RTC layout/schema stays stable while cleanup removes the
 * dead config path from API/JSON/UI.
 */
struct AirMouseTransportLayoutCompat {
    static constexpr uint8_t ReservedUsbValue = Defaults::AirMouse::ReservedTransport;
};

/**
 * Click source: accelerometer tap vs physical BOOT button
 */
enum class ClickSource : uint8_t {
    SENSOR = 0,   // Accelerometer tap detection
    BUTTON = 1    // Physical BOOT button (GPIO0)
};

/**
 * Configurable click action for each gesture
 */
enum class ClickAction : uint8_t {
    NONE = 0,
    LEFT_CLICK = 1,
    RIGHT_CLICK = 2,
    MIDDLE_CLICK = 3,
    RUN_SCRIPT = 4      // Trigger a DuckyScript macro
};

/**
 * Mouse Jiggler Mode
 */
enum class MouseJigglerMode : uint8_t {
    JIGGLER_OFF = 0,
    JIGGLER_STEALTH = 1,    // 1px micro-movements
    JIGGLER_ACTIVE = 2,     // Visible random movements
    JIGGLER_HUMAN = 3,      // Bezier curve natural movement
    JIGGLER_KEYBOARD = 4    // Keyboard F13 key press
};

/**
 * Mouse Jiggler Settings
 */
struct __attribute__((packed)) JigglerData {
    MouseJigglerMode mode = static_cast<MouseJigglerMode>(Defaults::AirMouse::Jiggler::Mode);
    uint32_t interval = Defaults::AirMouse::Jiggler::IntervalS;
    uint16_t moveDistance = Defaults::AirMouse::Jiggler::MoveDistance;
    bool randomInterval = Defaults::AirMouse::Jiggler::RandomInterval;
};

/**
 * Air Mouse settings (RTC-persisted)
 * All timing/filter params configurable at runtime
 */
struct __attribute__((packed)) AirMouseData {
    bool movementEnabled = Defaults::AirMouse::MovementEnabled;
    bool clickEnabled = Defaults::AirMouse::ClickEnabled;
    uint8_t reservedTransport = AirMouseTransportLayoutCompat::ReservedUsbValue;
    
    // Cursor sensitivity
    float sensitivityX = Defaults::AirMouse::SensitivityX;
    float sensitivityY = Defaults::AirMouse::SensitivityY;
    float deadzone = Defaults::AirMouse::Deadzone;
    
    // Acceleration (faster movement = more cursor distance)
    bool accelerationEnabled = Defaults::AirMouse::AccelerationEnabled;
    float accelerationFactor = Defaults::AirMouse::AccelerationFactor;
    
    // Tap detection
    float tapThresholdG = Defaults::AirMouse::TapThresholdG;
    uint16_t clickDebounceMs = Defaults::AirMouse::ClickDebounceMs;
    uint16_t doubleClickWindowMs = Defaults::AirMouse::DoubleClickWindowMs;
    
    // Click source & configurable actions
    ClickSource clickSource = static_cast<ClickSource>(Defaults::AirMouse::ClickSource);
    ClickAction singleClickAction = static_cast<ClickAction>(Defaults::AirMouse::SingleClickAction);
    ClickAction doubleClickAction = static_cast<ClickAction>(Defaults::AirMouse::DoubleClickAction);
    ClickAction tripleClickAction = static_cast<ClickAction>(Defaults::AirMouse::TripleClickAction);
    
    // Script filenames for RUN_SCRIPT actions (one per gesture)
    char singleClickScript[64] = {0};
    char doubleClickScript[64] = {0};
    char tripleClickScript[64] = {0};
    
    // 1-Euro filter (runtime tunable)
    float euroMinCutoff = Defaults::AirMouse::EuroMinCutoff;
    float euroBeta = Defaults::AirMouse::EuroBeta;
    float euroDCutoff = Defaults::AirMouse::EuroDCutoff;

    // Mouse Jiggler
    JigglerData jiggler;
};

} // namespace RTC
