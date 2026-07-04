#include "test_macro_logic_shared.h"

void test_execution_gaming_commands() {
    MACROS::MacroEngine engine;
    configureEngine(engine);

    MACROS::MacroCommand cmd{};
    MACROS::MacroCommand lastCmd{};
    uint32_t delay = 0;

    cmd.type = MACROS::CommandType::PRESS_KEY;
    cmd.key = 0x04;
    engine.processCommand(cmd, lastCmd, delay);

    TEST_ASSERT_EQUAL(1, mockKeyboard.pressKeyCount);
    TEST_ASSERT_EQUAL(0x04, mockKeyboard.lastPressedKey);

    cmd.type = MACROS::CommandType::RELEASE_KEY;
    cmd.key = 0x04;
    engine.processCommand(cmd, lastCmd, delay);

    TEST_ASSERT_EQUAL(1, mockKeyboard.releaseKeyCount);
    TEST_ASSERT_EQUAL(0x04, mockKeyboard.lastReleasedKey);

    cmd.type = MACROS::CommandType::RELEASE_ALL;
    engine.processCommand(cmd, lastCmd, delay);

    TEST_ASSERT_EQUAL(1, mockKeyboard.releaseAllCount);
}

void test_execution_all_other_commands() {
    MACROS::MacroEngine engine;
    configureEngine(engine, true);

    MACROS::MacroCommand cmd{};
    MACROS::MacroCommand lastCmd{};
    uint32_t delay = 0;

    cmd.type = MACROS::CommandType::STRING;
    cmd.textData = "TestText";
    engine.processCommand(cmd, lastCmd, delay);
    TEST_ASSERT_EQUAL_STRING("TYPE:TestText", mockKeyboard.lastTypeString.c_str());

    cmd.type = MACROS::CommandType::STRINGLN;
    cmd.textData = "Line";
    engine.processCommand(cmd, lastCmd, delay);
    TEST_ASSERT_EQUAL_STRING("TYPELN:Line", mockKeyboard.lastTypeString.c_str());

    cmd.type = MACROS::CommandType::COMBO;
    cmd.modifiers = MACROS::MOD_CTRL;
    cmd.key = 'c';
    engine.processCommand(cmd, lastCmd, delay);
    TEST_ASSERT_EQUAL(1, mockKeyboard.comboCount);

    cmd.type = MACROS::CommandType::MOUSE_MOVE;
    cmd.numericData = (uint32_t)((int16_t)10 << 16 | ((int16_t)-5 & 0xFFFF));
    engine.processCommand(cmd, lastCmd, delay);
    TEST_ASSERT_EQUAL(10, mockAirMouse.moveX);
    TEST_ASSERT_EQUAL(-5, mockAirMouse.moveY);

    cmd.type = MACROS::CommandType::MOUSE_CLICK;
    cmd.key = 2;
    engine.processCommand(cmd, lastCmd, delay);
    TEST_ASSERT_EQUAL(2, mockAirMouse.clickBtn);

    cmd.type = MACROS::CommandType::CONSUMER;
    cmd.key = 0xE9;
    engine.processCommand(cmd, lastCmd, delay);
    TEST_ASSERT_EQUAL(0xE9, mockKeyboard.consumerKey);

    cmd.type = MACROS::CommandType::SYSTEM_CONTROL;
    cmd.key = 0x01;
    engine.processCommand(cmd, lastCmd, delay);
    TEST_ASSERT_EQUAL(0x01, mockKeyboard.systemKey);
}
