#include <unity.h>
#include <cstdarg>
#include <cstdio>
#include <freertos/task.h>
#include "keyboard/KeyboardService.h"
#include "../../src/keyboard/KeyboardService.cpp"

// Mock Logging implementation
namespace LOG {
    Settings Logging::_settings;
    void Logging::log(esp_log_level_t level, const char *tag, const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        printf("[%s] ", tag);
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
    }
}
// ... stubs if needed, but included via build flags or local stubs
// USBHIDKeyboard stub handles the underlying calls

void setUp(void) {
    // reset singletons or global state if any
}

void tearDown(void) {
    // clean up
}

void test_keyboard_instantiation() {
    KEYBOARD::KeyboardService ks;
    TEST_ASSERT_FALSE(ks.isInitialized());
}

void test_keyboard_initialization() {
    KEYBOARD::KeyboardService ks;
    bool result = ks.begin();
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(ks.isInitialized());
    
    // transform to singleton behavior test if needed, but now it's instance
    // verifying double begin returns true
    TEST_ASSERT_TRUE(ks.begin());
}

void test_keyboard_stop() {
    KEYBOARD::KeyboardService ks;
    ks.begin();
    TEST_ASSERT_TRUE(ks.isInitialized());
    
    ks.stop();
    TEST_ASSERT_FALSE(ks.isInitialized());
}

void test_keyboard_typing() {
    KEYBOARD::KeyboardService ks;
    ks.begin();
    
    // These calls should not crash with stubbed USB
    ks.type("Hello");
    ks.typeLn("World");
    ks.press('A');
    ks.releaseAll();
    
    TEST_ASSERT_TRUE(ks.isInitialized());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_keyboard_instantiation);
    RUN_TEST(test_keyboard_initialization);
    RUN_TEST(test_keyboard_stop);
    RUN_TEST(test_keyboard_typing);
    UNITY_END();

    return 0;
}
