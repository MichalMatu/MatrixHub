/**
 * @file test_macro_repository.cpp
 * @brief Unit tests for MacroRepository listScripts normalization and filtering
 */

#include <unity.h>
#include <LittleFS.h>

#include "../../src/system/logging/Logging.h"
#include "../../src/macros/persistence/MacroRepository.cpp"

// LittleFS global
FS LittleFS;

extern "C" size_t heap_caps_get_free_size(uint32_t caps) {
    (void)caps;
    return 1024 * 1024;
}

// Logging stub
namespace LOG {
    Settings Logging::_settings;
    void Logging::log(esp_log_level_t level, const char *tag, const char *fmt, ...) {}
    void Logging::logSection(const char* title) {}
    void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {}
}

using namespace MACROS;

void setUp(void) {
    fsStubReset();
}

void tearDown(void) {}

void test_list_scripts_skips_tmp() {
    fsStubSetDirEntries("/scripts", {
        {"/scripts/a.txt", false},
        {"/scripts/b.tmp", false}
    });

    auto files = MacroRepository::listScripts();
    TEST_ASSERT_EQUAL(1, files.size());
    TEST_ASSERT_EQUAL_STRING("a.txt", files[0].c_str());
}

void test_list_scripts_normalizes_path() {
    fsStubSetDirEntries("/scripts", {
        {"/scripts/foo.txt", false}
    });

    auto files = MacroRepository::listScripts();
    TEST_ASSERT_EQUAL(1, files.size());
    TEST_ASSERT_EQUAL_STRING("foo.txt", files[0].c_str());
}

void test_script_exists_uses_scripts_directory() {
    fsStubSetFileExists("/scripts/exists.txt", true);

    TEST_ASSERT_TRUE(MacroRepository::scriptExists("exists.txt"));
    TEST_ASSERT_FALSE(MacroRepository::scriptExists("missing.txt"));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_list_scripts_skips_tmp);
    RUN_TEST(test_list_scripts_normalizes_path);
    RUN_TEST(test_script_exists_uses_scripts_directory);
    return UNITY_END();
}
