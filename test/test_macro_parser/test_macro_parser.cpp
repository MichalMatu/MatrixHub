/**
 * @file test_macro_parser.cpp
 * @brief Unit/Integration tests for MacroParser
 */

#include <unity.h>

#ifdef NATIVE_BUILD
#include <FS.h>
FS LittleFS; // Provide mock File System for the parser

#include "../../src/macros/parsing/MacroParser.cpp"
#include "../../src/notifications/telegram/commands/MacroCommands.h"

// Provide mock Logging
namespace LOG {
    Settings Logging::_settings;
    void Logging::log(esp_log_level_t level, const char *tag, const char *fmt, ...) {}
    void Logging::logSection(const char* title) {}
    void Logging::logStackHwm(const char* name, uint32_t period) {}
}

#else
#include <LittleFS.h>
#include "../../src/macros/parsing/MacroParser.cpp"
#endif

using namespace MACROS;
const char* TEST_SCRIPT_PATH = "/test_script.txt";

void setUp(void) {
    if (!LittleFS.begin(true)) {
        TEST_FAIL_MESSAGE("Failed to mount FS");
    }
}

void tearDown(void) {
    if (LittleFS.exists(TEST_SCRIPT_PATH)) {
        LittleFS.remove(TEST_SCRIPT_PATH);
    }
}

// Use memory mode for native tests to avoid FS mock complexities
void executeScript(const char* content) {
    // This is tested directly inside the test cases now
}

void test_parser_standard_commands() {
    const char* script = 
        "DELAY 500\n"
        "STRING Hello World\n"
        "ENTER\n";

    MacroParser parser;
    TEST_ASSERT_TRUE(parser.beginFromContent((const uint8_t*)script, strlen(script)));

    MacroCommand cmd;
    TEST_ASSERT_TRUE(parser.next(cmd));
    TEST_ASSERT_EQUAL(CommandType::DELAY, cmd.type);
    TEST_ASSERT_EQUAL(500, cmd.numericData);

    TEST_ASSERT_TRUE(parser.next(cmd));
    TEST_ASSERT_EQUAL(CommandType::STRING, cmd.type);
    TEST_ASSERT_EQUAL_STRING("Hello World", cmd.textData.c_str());

    TEST_ASSERT_TRUE(parser.next(cmd));
    TEST_ASSERT_EQUAL(CommandType::KEY, cmd.type);

    TEST_ASSERT_FALSE(parser.next(cmd)); // EOF
    parser.end();
}

void test_parser_missing_eof_newline() {
    const char* script = "STRING TestNoNewline"; // Lacks \n at the end

    MacroParser parser;
    TEST_ASSERT_TRUE(parser.beginFromContent((const uint8_t*)script, strlen(script)));

    MacroCommand cmd;
    TEST_ASSERT_TRUE(parser.next(cmd));
    TEST_ASSERT_EQUAL(CommandType::STRING, cmd.type);
    TEST_ASSERT_EQUAL_STRING("TestNoNewline", cmd.textData.c_str());

    TEST_ASSERT_FALSE(parser.next(cmd)); // Gracefully handles missing EOF newline
    parser.end();
}

void test_stress_overflow() {
    // Generate a massive string > 1024 bytes (MAX_LINE_LENGTH is ~1024 usually)
    std::string hugeLine = "STRING ";
    for (int i = 0; i < 2000; i++) {
        hugeLine += "A";
    }
    hugeLine += "\n";

    MacroParser parser;
    TEST_ASSERT_TRUE(parser.beginFromContent((const uint8_t*)hugeLine.c_str(), hugeLine.length()));
    MacroCommand cmd;

    // The parser should safely truncate the read to prevent buffer overflows
    while(parser.next(cmd)) {
        if (cmd.type == CommandType::STRING) {
             TEST_ASSERT_TRUE(cmd.textData.length() <= 1024);
        }
    }
    parser.end();
}

void test_parser_clamps_delay_and_repeat_limits() {
    const char* script =
        "DELAY 999999999\n"
        "DEFAULT_DELAY 999999999\n"
        "REPEAT 999999999\n"
        "REPLAY 999999999\n"
        "REPEAT -5\n"
        "REPLAY -5\n";

    MacroParser parser;
    TEST_ASSERT_TRUE(parser.beginFromContent((const uint8_t*)script, strlen(script)));

    MacroCommand cmd;
    TEST_ASSERT_TRUE(parser.next(cmd));
    TEST_ASSERT_EQUAL(CommandType::DELAY, cmd.type);
    TEST_ASSERT_EQUAL(MACROS::LIMITS::MAX_DELAY_MS, cmd.numericData);

    TEST_ASSERT_TRUE(parser.next(cmd));
    TEST_ASSERT_EQUAL(CommandType::DEFAULT_DELAY, cmd.type);
    TEST_ASSERT_EQUAL(MACROS::LIMITS::MAX_DEFAULT_DELAY_MS, cmd.numericData);

    TEST_ASSERT_TRUE(parser.next(cmd));
    TEST_ASSERT_EQUAL(CommandType::REPEAT, cmd.type);
    TEST_ASSERT_EQUAL(MACROS::LIMITS::MAX_REPEAT_COUNT, cmd.numericData);

    TEST_ASSERT_TRUE(parser.next(cmd));
    TEST_ASSERT_EQUAL(CommandType::REPEAT, cmd.type);
    TEST_ASSERT_EQUAL(MACROS::LIMITS::MAX_REPEAT_COUNT, cmd.numericData);

    TEST_ASSERT_TRUE(parser.next(cmd));
    TEST_ASSERT_EQUAL(CommandType::REPEAT, cmd.type);
    TEST_ASSERT_EQUAL(1, cmd.numericData);

    TEST_ASSERT_TRUE(parser.next(cmd));
    TEST_ASSERT_EQUAL(CommandType::REPEAT, cmd.type);
    TEST_ASSERT_EQUAL(1, cmd.numericData);

    TEST_ASSERT_FALSE(parser.next(cmd));
    parser.end();
}


int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_parser_standard_commands);
    RUN_TEST(test_parser_missing_eof_newline);
    RUN_TEST(test_stress_overflow);
    RUN_TEST(test_parser_clamps_delay_and_repeat_limits);
    return UNITY_END();
}

#if defined(ARDUINO) || defined(ESP_PLATFORM)
void setup() {
    delay(2000); // Allow USB to stabilize
    main(0, nullptr);
}
void loop() {}
#endif
