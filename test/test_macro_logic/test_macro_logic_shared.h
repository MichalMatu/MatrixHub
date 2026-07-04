#pragma once

#include <unity.h>
#include <Arduino.h>

#define TEST_STUBS_SKIP_XTASKCREATESTATICPINNEDTOCORE 1

// Defines for keys used by macro tests.
#define KEY_LEFT_CTRL   0x80
#define KEY_LEFT_SHIFT  0x81
#define KEY_LEFT_ALT    0x82
#define KEY_LEFT_GUI    0x83
#define KEY_RETURN      0xB0
#define KEY_TAB         0xB3

namespace KEYBOARD {
class KeyboardService {
public:
    int pressKeyCount = 0;
    int releaseKeyCount = 0;
    int releaseAllCount = 0;
    uint16_t lastPressedKey = 0;
    uint16_t lastReleasedKey = 0;
    std::string lastTypeString;
    int comboCount = 0;
    uint16_t systemKey = 0;
    uint16_t consumerKey = 0;

    void type(const char* s) { lastTypeString = "TYPE:" + std::string(s); }
    void typeLn(const char* s) { lastTypeString = "TYPELN:" + std::string(s); }
    void press(uint16_t k) { lastPressedKey = k; }
    void pressKey(uint16_t k) {
        pressKeyCount++;
        lastPressedKey = k;
    }
    void releaseKey(uint16_t k) {
        releaseKeyCount++;
        lastReleasedKey = k;
    }
    void releaseAll() { releaseAllCount++; }
    void pressCombo(uint8_t* keys, size_t count) {
        (void)keys;
        (void)count;
        comboCount++;
    }
    void pressSystem(uint16_t k) { systemKey = k; }
    void pressConsumer(uint16_t k) { consumerKey = k; }
    bool isInitialized() { return true; }

    void resetStats() {
        pressKeyCount = 0;
        releaseKeyCount = 0;
        releaseAllCount = 0;
        lastPressedKey = 0;
        lastReleasedKey = 0;
        lastTypeString.clear();
        comboCount = 0;
        systemKey = 0;
        consumerKey = 0;
    }
};
}  // namespace KEYBOARD

namespace AIRMOUSE {
class AirMouseService {
public:
    int moveX = 0;
    int moveY = 0;
    int clickBtn = 0;

    void move(int x, int y) {
        moveX = x;
        moveY = y;
    }
    void click(uint8_t b) { clickBtn = b; }
    void begin(KEYBOARD::KeyboardService* k) { (void)k; }

    void reset() {
        moveX = 0;
        moveY = 0;
        clickBtn = 0;
    }
};
}  // namespace AIRMOUSE

#include "../../src/system/logging/Logging.h"
#include "../../src/system/rtc/RtcConfig.h"
#include "../../src/config/System.h"

#define private public
#define protected public
#include "../../src/macros/MacroService.h"
#undef protected
#undef private

extern KEYBOARD::KeyboardService mockKeyboard;
extern AIRMOUSE::AirMouseService mockAirMouse;

void configureEngine(MACROS::MacroEngine& engine, bool withAirMouse = false);

namespace RTC {
void resetMockStore();
}
