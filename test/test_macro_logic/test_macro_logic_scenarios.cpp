#include "test_macro_logic_shared.h"

void test_use_case_gaming_afk() {
    MACROS::MacroEngine engine;
    configureEngine(engine);

    MACROS::MacroCommand cmd{};
    MACROS::MacroCommand lastCmd{};
    uint32_t delay = 0;

    cmd.type = MACROS::CommandType::PRESS_KEY;
    cmd.key = 0x1A;
    engine.processCommand(cmd, lastCmd, delay);

    TEST_ASSERT_EQUAL(1, mockKeyboard.pressKeyCount);
    TEST_ASSERT_EQUAL(0x1A, mockKeyboard.lastPressedKey);

    cmd.type = MACROS::CommandType::DELAY;
    cmd.numericData = 5000;
    engine.processCommand(cmd, lastCmd, delay);

    cmd.type = MACROS::CommandType::RELEASE_KEY;
    cmd.key = 0x1A;
    engine.processCommand(cmd, lastCmd, delay);

    TEST_ASSERT_EQUAL(1, mockKeyboard.releaseKeyCount);
    TEST_ASSERT_EQUAL(0x1A, mockKeyboard.lastReleasedKey);
}

void test_use_case_login_sequence() {
    MACROS::MacroEngine engine;
    configureEngine(engine);

    MACROS::MacroCommand cmd{};
    MACROS::MacroCommand lastCmd{};
    uint32_t delay = 0;

    cmd.type = MACROS::CommandType::STRING;
    cmd.textData = "user";
    engine.processCommand(cmd, lastCmd, delay);
    TEST_ASSERT_EQUAL_STRING("TYPE:user", mockKeyboard.lastTypeString.c_str());

    cmd.type = MACROS::CommandType::KEY;
    cmd.key = KEY_TAB;
    engine.processCommand(cmd, lastCmd, delay);
    TEST_ASSERT_EQUAL(KEY_TAB, mockKeyboard.lastPressedKey);
}

void test_psram_exhaustion_safety() {
    MACROS::MacroEngine engine;
    configureEngine(engine);

    MACROS::MacroCommand cmd{};
    MACROS::MacroCommand lastCmd{};
    uint32_t delay = 0;

    cmd.type = MACROS::CommandType::STRING;
    for (int i = 0; i < 1000; i++) {
        cmd.textData += "X";
    }

    engine.processCommand(cmd, lastCmd, delay);

    TEST_ASSERT_EQUAL(1000, mockKeyboard.lastTypeString.length() - 5);
}
