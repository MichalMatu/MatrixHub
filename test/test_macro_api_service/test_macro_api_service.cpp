/**
 * @file test_macro_api_service.cpp
 * @brief Integration-like tests for MacroApiService settings validation
 */

#include <unity.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <string>

#include "../../src/system/logging/Logging.h"
#include "../../src/system/rtc/RtcConfig.h"
#include "../../test/stubs/PsychicRequest.h"
#include "../../test/stubs/PsychicHttpServer.h"
#include "../../test/stubs/freertos/semphr.h"

// ----------------- Stubs -----------------
// Minimal HTTP server stubs
extern "C" {
    #include "esp_http_server.h"
    esp_err_t httpd_resp_set_type(httpd_req_t *r, const char *type) { return ESP_OK; }
    esp_err_t httpd_resp_set_hdr(httpd_req_t *r, const char *field, const char *value) { return ESP_OK; }
    esp_err_t httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len) { return ESP_OK; }
}

// Logging stub
namespace LOG {
    Settings Logging::_settings;
    void Logging::log(esp_log_level_t level, const char *tag, const char *fmt, ...) {}
    void Logging::logSection(const char* title) {}
    void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {}
}

// RTC mock
namespace RTC {
    ConfigStore mockStore;

    ConfigStore& getMutableConfig() { return mockStore; }
    const ConfigStore& getConfig() { return mockStore; }
    void withConfig(const std::function<void(const ConfigStore&)>& reader) { reader(mockStore); }
    SemaphoreHandle_t getLock() {
        static SemaphoreHandle_t lock = xSemaphoreCreateMutex();
        return lock;
    }
    bool updateConfig(const std::function<void(ConfigStore&)>& updater) {
        updater(mockStore);
        return true;
    }
    void markValid() {}
}

// CONFIG mock
namespace CONFIG {
    bool save(FS& fs) { return true; }
}

// LittleFS global
FS LittleFS;

// Bring real implementations needed by MacroApiService
#include "../../src/system/utils/json/JsonResponseWriter.cpp"
#include "../../src/config/json/MacroConfigJson.cpp"
#include "../../src/macros/MacroSettingsService.cpp"

#include "../../src/api/macros/MacroApiService.cpp"

update_handler_id_t StateUpdateHandlerInfo::currentUpdatedHandlerId = 0;
hook_handler_id_t StateHookHandlerInfo::currentHookHandlerId = 0;

// MacroService stubs to satisfy linker in native tests
namespace {
    bool stubScriptExists = false;
    bool stubSaveResult = true;
    bool stubDeleteResult = true;
    bool stubStartResult = true;
    bool stubStopCalled = false;
    std::string stubSavedFilename;
    std::string stubSavedContent;
    std::string stubDeletedFilename;
    std::string stubStartedFilename;
    std::string stubContent;

    MACROS::MacroService* fakeMacroService() {
        return reinterpret_cast<MACROS::MacroService*>(0x1);
    }
}

namespace MACROS {
    PsramVector<PsramString> MacroService::listScripts() { return {}; }
    bool MacroService::deleteScript(const char* filename) {
        stubDeletedFilename = filename ? filename : "";
        return stubDeleteResult;
    }
    bool MacroService::saveScript(const char* filename, const char* content) {
        stubSavedFilename = filename ? filename : "";
        stubSavedContent = content ? content : "";
        return stubSaveResult;
    }
    PsramString MacroService::getScriptContent(const char* filename) {
        (void)filename;
        return PsramString(stubContent.c_str(), PsramAllocator<char>());
    }
    bool MacroService::scriptExists(const char* filename) {
        (void)filename;
        return stubScriptExists;
    }
    bool MacroService::startScript(const char* filename) {
        stubStartedFilename = filename ? filename : "";
        return stubStartResult;
    }
    void MacroService::stopScript() { stubStopCalled = true; }
    MacroState MacroService::getStatus() { return MacroState(); }
    void MacroService::applySettings() {}
}

// PowerManager stub (BaseApiService wrapper dependency)
namespace POWER {
    void PowerManager::notifyActivity(const char *source) { (void)source; }
}

using namespace MACROS;

void setUp(void) {
    auto& cfg = RTC::getMutableConfig().macros;
    cfg.enabled = true;
    cfg.bootDelay = 5000;
    memset(cfg.bootScript, 0, sizeof(cfg.bootScript));
    stubScriptExists = false;
    stubSaveResult = true;
    stubDeleteResult = true;
    stubStartResult = true;
    stubStopCalled = false;
    stubSavedFilename.clear();
    stubSavedContent.clear();
    stubDeletedFilename.clear();
    stubStartedFilename.clear();
    stubContent.clear();
}

void tearDown(void) {}

static void setJsonBody(PsychicRequest& req, JsonDocument& doc) {
    std::string body;
    serializeJson(doc, body);
    req.setContentType("application/json");
    req.setBody(body);
}

void test_save_settings_boot_script_missing() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    MacroSettingsService settings(&LittleFS, nullptr);
    MacroApiService api(&server, &securityManager, nullptr, nullptr, &settings);
    api.begin();

    PsychicRequest req;
    JsonDocument doc;
    doc["boot_script"] = "missing.txt";
    std::string body;
    serializeJson(doc, body);
    req.setBody(body);

    esp_err_t err = server.invoke("/api/macros/settings", HTTP_POST, &req);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(400, req.lastStatusCode);
    TEST_ASSERT_TRUE(req.lastResponseBody.find("boot_script_not_found") != std::string::npos);
}

void test_upload_rejects_path_traversal_filename() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    MacroApiService api(&server, &securityManager, nullptr, fakeMacroService(), nullptr);
    api.begin();

    PsychicRequest req;
    JsonDocument doc;
    doc["filename"] = "../evil.txt";
    doc["content"] = "REM safe";
    setJsonBody(req, doc);

    esp_err_t err = server.invoke("/api/macros", HTTP_POST, &req);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(400, req.lastStatusCode);
    TEST_ASSERT_TRUE(req.lastResponseBody.find("input/invalid_filename") != std::string::npos);
    TEST_ASSERT_EQUAL_STRING("", stubSavedFilename.c_str());
}

void test_upload_rejects_missing_content() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    MacroApiService api(&server, &securityManager, nullptr, fakeMacroService(), nullptr);
    api.begin();

    PsychicRequest req;
    JsonDocument doc;
    doc["filename"] = "safe.txt";
    setJsonBody(req, doc);

    esp_err_t err = server.invoke("/api/macros", HTTP_POST, &req);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(400, req.lastStatusCode);
    TEST_ASSERT_TRUE(req.lastResponseBody.find("input/missing_content") != std::string::npos);
}

void test_upload_rejects_oversize_script_payload() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    MacroApiService api(&server, &securityManager, nullptr, fakeMacroService(), nullptr);
    api.begin();

    std::string content(MACROS::LIMITS::MAX_SCRIPT_SIZE_BYTES + 1, 'A');
    PsychicRequest req;
    JsonDocument doc;
    doc["filename"] = "large.txt";
    doc["content"] = content;
    setJsonBody(req, doc);

    esp_err_t err = server.invoke("/api/macros", HTTP_POST, &req);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_TRUE(req.lastStatusCode == 400 || req.lastStatusCode == 413);
    TEST_ASSERT_TRUE(
        req.lastResponseBody.find("input/script_too_large") != std::string::npos ||
        req.lastResponseBody.find("input/payload_too_large") != std::string::npos);
    TEST_ASSERT_EQUAL_STRING("", stubSavedFilename.c_str());
}

void test_run_missing_script_returns_not_found() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    MacroApiService api(&server, &securityManager, nullptr, fakeMacroService(), nullptr);
    api.begin();

    PsychicRequest req;
    JsonDocument doc;
    doc["name"] = "missing.txt";
    setJsonBody(req, doc);

    esp_err_t err = server.invoke("/api/macros/run", HTTP_POST, &req);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(404, req.lastStatusCode);
    TEST_ASSERT_TRUE(req.lastResponseBody.find("input/script_not_found") != std::string::npos);
    TEST_ASSERT_EQUAL_STRING("", stubStartedFilename.c_str());
}

void test_run_rejects_invalid_filename_without_sanitizing() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    MacroApiService api(&server, &securityManager, nullptr, fakeMacroService(), nullptr);
    api.begin();

    PsychicRequest req;
    JsonDocument doc;
    doc["name"] = "../safe.txt";
    setJsonBody(req, doc);

    esp_err_t err = server.invoke("/api/macros/run", HTTP_POST, &req);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(400, req.lastStatusCode);
    TEST_ASSERT_TRUE(req.lastResponseBody.find("input/invalid_filename") != std::string::npos);
    TEST_ASSERT_EQUAL_STRING("", stubStartedFilename.c_str());
}

void test_get_content_allows_empty_existing_script() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    MacroApiService api(&server, &securityManager, nullptr, fakeMacroService(), nullptr);
    api.begin();

    stubScriptExists = true;
    stubContent = "";

    PsychicRequest req;
    req.setParam("name", "empty.txt");

    esp_err_t err = server.invoke("/api/macros/content", HTTP_GET, &req);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_STRING("", req._lastReply.c_str());
    TEST_ASSERT_EQUAL(0, req.lastStatusCode);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_save_settings_boot_script_missing);
    RUN_TEST(test_upload_rejects_path_traversal_filename);
    RUN_TEST(test_upload_rejects_missing_content);
    RUN_TEST(test_upload_rejects_oversize_script_payload);
    RUN_TEST(test_run_missing_script_returns_not_found);
    RUN_TEST(test_run_rejects_invalid_filename_without_sanitizing);
    RUN_TEST(test_get_content_allows_empty_existing_script);
    return UNITY_END();
}
