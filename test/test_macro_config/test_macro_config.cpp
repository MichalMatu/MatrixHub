/**
 * @file test_macro_config.cpp
 * @brief Unit tests for Macro Configuration Persistence (JSON/RTC)
 */

#include <unity.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "../../src/config/json/MacroConfigJson.h"
#include "../../src/config/json/MacroConfigJson.cpp" // Include impl for linking
#include "../../src/system/rtc/RtcConfig.h"
#include "../../src/config/App.h"

// ============================================================================
// MOCKS / STUBS
// ============================================================================

// 1. Mock RTC Storage
// We don't want to rely on hardware RTC memory or link the complex RtcConfig.cpp
namespace RTC {
    ConfigStore mockStore;

    ConfigStore& getMutableConfig() {
        return mockStore;
    }

    const ConfigStore& getConfig() {
        return mockStore;
    }

    void withConfig(const std::function<void(const ConfigStore&)>& reader) {
        reader(mockStore);
    }

    bool updateConfig(const std::function<void(ConfigStore&)>& updater) {
        updater(mockStore);
        return true;
    }
}

// 2. Stub Logging
namespace LOG {
    void Logging::log(esp_log_level_t level, const char* tag, const char* format, ...) {
        // No-op
    }
}

// We need to link RtcConfig storage if not already linked by environment
// In PlatformIO tests, usually we explicitly include what we need or rely on library dependencies.
// Since RtcConfig.cpp contains the storage definition, we might need it.
// However, RtcConfig.h declares them extern.
// Let's try to rely on the build system. If linker fails, we include RtcConfig.cpp.

// Namespace shortcuts
using namespace CONFIG;
using namespace CONFIG::JSON;

void setUp(void) {
    // Reset RTC macros to defaults before each test
    auto& cfg = RTC::getMutableConfig().macros;
    cfg.enabled = false;
    cfg.bootDelay = 5000;
    memset(cfg.bootScript, 0, sizeof(cfg.bootScript));
}

void tearDown(void) {
}

void test_json_load_macros() {
    // 1. Prepare JSON
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    obj[Keys::kEnabled] = true;
    obj[Keys::kBootDelay] = 1234;
    obj[Keys::kBootScript] = "startup.txt";

    // 2. Load
    loadMacros(obj);

    // 3. Verify RTC
    const auto& cfg = RTC::getConfig().macros;
    TEST_ASSERT_TRUE(cfg.enabled);
    TEST_ASSERT_EQUAL_UINT32(1234, cfg.bootDelay);
    TEST_ASSERT_EQUAL_STRING("startup.txt", cfg.bootScript);
}

void test_json_load_macros_partial() {
    // Test loading only partial keys does not wipe others
    
    // Setup initial state
    auto& cfg = RTC::getMutableConfig().macros;
    cfg.enabled = true;
    cfg.bootDelay = 9999;
    strcpy(cfg.bootScript, "keep_me.txt");

    // JSON has only bootDelay
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    obj[Keys::kBootDelay] = 1000;

    loadMacros(obj);

    TEST_ASSERT_TRUE(cfg.enabled); // Should stay true
    TEST_ASSERT_EQUAL_UINT32(1000, cfg.bootDelay); // Should update
    TEST_ASSERT_EQUAL_STRING("keep_me.txt", cfg.bootScript); // Should stay
}

void test_json_save_macros() {
    // 1. Setup RTC
    auto& cfg = RTC::getMutableConfig().macros;
    cfg.enabled = true;
    cfg.bootDelay = 5555;
    strcpy(cfg.bootScript, "save_test.txt");

    // 2. Save
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    saveMacros(obj);

    // 3. Verify JSON
    TEST_ASSERT_TRUE(obj[Keys::kEnabled].as<bool>());
    TEST_ASSERT_EQUAL(5555, obj[Keys::kBootDelay].as<uint32_t>());
    TEST_ASSERT_EQUAL_STRING("save_test.txt", obj[Keys::kBootScript].as<const char*>());
}

void test_json_boot_script_sanitize_path() {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    obj[Keys::kBootScript] = "/scripts/foo.txt";

    loadMacros(obj);

    const auto& cfg = RTC::getConfig().macros;
    TEST_ASSERT_EQUAL_STRING("foo.txt", cfg.bootScript);
}

void test_json_boot_script_sanitize_traversal() {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    obj[Keys::kBootScript] = "../foo.txt";

    loadMacros(obj);

    const auto& cfg = RTC::getConfig().macros;
    TEST_ASSERT_EQUAL_STRING("foo.txt", cfg.bootScript);
}

void test_json_boot_script_empty() {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    obj[Keys::kBootScript] = "";

    loadMacros(obj);

    const auto& cfg = RTC::getConfig().macros;
    TEST_ASSERT_EQUAL_STRING("", cfg.bootScript);
}

#ifndef NATIVE_BUILD
// Dummy for Arduino framework
void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_json_load_macros);
    RUN_TEST(test_json_load_macros_partial);
    RUN_TEST(test_json_save_macros);
    RUN_TEST(test_json_boot_script_sanitize_path);
    RUN_TEST(test_json_boot_script_sanitize_traversal);
    RUN_TEST(test_json_boot_script_empty);
    UNITY_END();
}

void loop() {}
#else
int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_json_load_macros);
    RUN_TEST(test_json_load_macros_partial);
    RUN_TEST(test_json_save_macros);
    RUN_TEST(test_json_boot_script_sanitize_path);
    RUN_TEST(test_json_boot_script_sanitize_traversal);
    RUN_TEST(test_json_boot_script_empty);
    return UNITY_END();
}
#endif
