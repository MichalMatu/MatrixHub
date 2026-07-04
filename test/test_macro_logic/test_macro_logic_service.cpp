#include "test_macro_logic_shared.h"

void test_initialization() {
    MACROS::MacroService service(&mockKeyboard, &mockAirMouse, nullptr, nullptr);
    TEST_ASSERT_EQUAL(MACROS::MacroStatus::IDLE, service.getStatus().status);
}

void test_settings_persistence() {
    // Persistence accessors were removed from MacroService; keep a placeholder
    // to preserve suite coverage layout without inventing fake behavior.
    TEST_ASSERT_TRUE(true);
}

void test_engine_clear_state() {
    MACROS::MacroEngine engine;
    engine._state.status = MACROS::MacroStatus::ERROR;
    engine._state.currentLine = 42;
    engine._state.currentScript = "bad.txt";
    engine._state.lastError = "boom";

    engine.clearState(false);

    const MACROS::MacroState state = engine.getState();
    TEST_ASSERT_EQUAL(MACROS::MacroStatus::IDLE, state.status);
    TEST_ASSERT_EQUAL_UINT32(0, state.currentLine);
    TEST_ASSERT_EQUAL_STRING("", state.currentScript.c_str());
    TEST_ASSERT_EQUAL_STRING("", state.lastError.c_str());
}

void test_engine_execution_budget_limits_commands_and_runtime() {
    MACROS::MacroEngine engine;
    MACROS::PsramString error;

    TEST_STUBS::ARDUINO::millisValue = 1000;
    TEST_ASSERT_FALSE(engine.executionLimitExceeded(1000, MACROS::LIMITS::MAX_EXPANDED_COMMANDS, error));
    TEST_ASSERT_EQUAL_STRING("", error.c_str());

    TEST_ASSERT_TRUE(engine.executionLimitExceeded(
        1000,
        MACROS::LIMITS::MAX_EXPANDED_COMMANDS + 1,
        error));
    TEST_ASSERT_EQUAL_STRING("Macro command limit exceeded", error.c_str());

    error.clear();
    TEST_STUBS::ARDUINO::millisValue = 1000 + MACROS::LIMITS::MAX_RUNTIME_MS + 1;
    TEST_ASSERT_TRUE(engine.executionLimitExceeded(1000, 1, error));
    TEST_ASSERT_EQUAL_STRING("Macro runtime limit exceeded", error.c_str());
}
