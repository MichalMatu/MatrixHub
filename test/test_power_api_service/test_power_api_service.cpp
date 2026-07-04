#include <unity.h>

#include <ArduinoJson.h>
#include <string>

#include "../../test/stubs/PsychicHttpServer.h"
#include "../../test/stubs/PsychicRequest.h"

#include "../../src/system/power/PowerManager.h"
#include "../../src/system/power/PowerSettingsService.h"
#include "../../src/system/thermal/ThermalMonitor.h"
#include "../../src/system/logging/Logging.h"
#include "../../src/system/rtc/types/RtcRuntimeTypes.h"

std::string g_responseBuffer;
std::string g_responseType;
std::string g_responseHeaderField;
std::string g_responseHeaderValue;

extern "C" {
#include "esp_http_server.h"

esp_err_t httpd_resp_set_type(httpd_req_t* r, const char* type) {
    (void)r;
    g_responseType = type ? type : "";
    return ESP_OK;
}

esp_err_t httpd_resp_set_hdr(httpd_req_t* r, const char* field, const char* value) {
    (void)r;
    g_responseHeaderField = field ? field : "";
    g_responseHeaderValue = value ? value : "";
    return ESP_OK;
}

esp_err_t httpd_resp_send_chunk(httpd_req_t* r, const char* buf, ssize_t len) {
    (void)r;
    if (buf != nullptr && len > 0) {
        g_responseBuffer.append(buf, static_cast<size_t>(len));
    }
    return ESP_OK;
}

esp_err_t httpd_unregister_uri_handler(httpd_handle_t handle, const char* uri, httpd_method_t method) {
    (void)handle;
    (void)uri;
    (void)method;
    return ESP_OK;
}
}

namespace LOG {
Settings Logging::_settings{};

void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    (void)level;
    (void)tag;
    (void)fmt;
}

void Logging::logSection(const char* title) {
    (void)title;
}

void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {
    (void)taskName;
    (void)stackSize;
}
}  // namespace LOG

namespace RTC {
RtcRuntimeStats runtimeStats{};

bool markMaintenanceSleepPending(uint32_t nowMs, float thermalShutdownTemp) {
    (void)nowMs;
    (void)thermalShutdownTemp;
    return true;
}
}  // namespace RTC

namespace POWER {
namespace {
uint32_t g_notifyActivityCount = 0;
}

WakeReason PowerManager::wakeReason() {
    return WakeReason::Timer;
}

esp_sleep_wakeup_cause_t PowerManager::getWakeupCauseRaw() {
    return ESP_SLEEP_WAKEUP_TIMER;
}

uint64_t PowerManager::getGpioWakeupMask() {
    return 0x01;
}

uint64_t PowerManager::getExt1WakeupMask() {
    return 0x02;
}

bool PowerManager::isSleepRequested() {
    return false;
}

InactivityConfig PowerManager::inactivityConfig() {
    return InactivityConfig{600000, 120000, true};
}

RTC::PowerData PowerManager::powerConfig() {
    RTC::PowerData config{};
    config.sleepEnabled = true;
    config.inactivityTimeoutMs = 600000;
    config.graceAfterBootMs = 120000;
    config.wakeTimerEnabled = true;
    config.wakeButtonEnabled = true;
    config.wakeTouchEnabled = false;
    config.wakeIntervalMs = 300000;
    config.timerWakeAwakeMs = 60000;
    config.buttonWakeAwakeMs = 120000;
    config.wakeTouchGpio = 4;
    config.wakeTouchThreshold = 2000;
    return config;
}

uint32_t PowerManager::wakeIntervalMs() {
    return 300000;
}

uint32_t PowerManager::wakeAwakeWindowMs() {
    return 60000;
}

uint32_t PowerManager::wakeAwakeEtaMs() {
    return 45000;
}

uint32_t PowerManager::lastActivityMs() {
    return 1234;
}

uint32_t PowerManager::sleepEtaMs() {
    return 0;
}

void PowerManager::notifyActivity(const char* source) {
    (void)source;
    g_notifyActivityCount++;
}

void PowerManager::setWakeInterval(uint32_t intervalMs) {
    (void)intervalMs;
}

void PowerManager::requestSleep(const char* reason, uint32_t delayMs) {
    (void)reason;
    (void)delayMs;
}

void PowerSettingsService::readState(RTC::PowerData& settings, JsonObject& root) {
    root["sleep_enabled"] = settings.sleepEnabled;
    root["inactivity_timeout_ms"] = settings.inactivityTimeoutMs;
    root["grace_after_boot_ms"] = settings.graceAfterBootMs;
    root["wake_timer_enabled"] = settings.wakeTimerEnabled;
    root["wake_button_enabled"] = settings.wakeButtonEnabled;
    root["wake_touch_enabled"] = settings.wakeTouchEnabled;
    root["wake_interval_ms"] = settings.wakeIntervalMs;
    root["timer_wake_awake_ms"] = settings.timerWakeAwakeMs;
    root["button_wake_awake_ms"] = settings.buttonWakeAwakeMs;
    root["wake_touch_gpio"] = settings.wakeTouchGpio;
    root["wake_touch_threshold"] = settings.wakeTouchThreshold;
}

StateHandlerResult PowerSettingsService::validateStateUpdate(JsonObject& jsonObject) {
    (void)jsonObject;
    return StateHandlerResult::success();
}

StateUpdateResult PowerSettingsService::updateState(
    JsonObject& jsonObject,
    RTC::PowerData& settings,
    std::string_view originId) {
    (void)jsonObject;
    (void)settings;
    (void)originId;
    return StateUpdateResult::UNCHANGED;
}
}  // namespace POWER

namespace SYSTEM {
ThermalMonitor& ThermalMonitor::instance() {
    static ThermalMonitor inst;
    return inst;
}
}  // namespace SYSTEM

#include "../../src/system/utils/json/JsonResponseWriter.cpp"
#include "../../src/api/power/PowerApiService.cpp"

void setUp(void) {
    g_responseBuffer.clear();
    g_responseType.clear();
    g_responseHeaderField.clear();
    g_responseHeaderValue.clear();
    POWER::g_notifyActivityCount = 0;
    TEST_STUBS::ARDUINO::millisValue = 10000;
    TEST_ASSERT_TRUE(Utils::JsonResponseWriter::begin());
}

void tearDown(void) {}

void test_power_status_exposes_thermal_diagnostics_without_activity_side_effect() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    POWER::PowerManager powerManager;
    API::PowerApiService api(&server, &securityManager, &powerManager, nullptr);
    api.begin();
    PsychicRequest request;
    request.setUri("/rest/power/status");

    const esp_err_t err = server.invoke("/rest/power/status", HTTP_GET, &request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_UINT32(0, POWER::g_notifyActivityCount);

    JsonDocument doc;
    DeserializationError jsonError = deserializeJson(doc, g_responseBuffer);
    TEST_ASSERT_FALSE(jsonError);
    TEST_ASSERT_EQUAL_STRING("timer", doc["wake_reason"].as<const char*>());
    TEST_ASSERT_TRUE(doc["sleep_enabled"].as<bool>());
    TEST_ASSERT_TRUE(doc["wake_timer_enabled"].as<bool>());
    TEST_ASSERT_TRUE(doc["wake_button_enabled"].as<bool>());
    TEST_ASSERT_FALSE(doc["wake_touch_enabled"].as<bool>());
    TEST_ASSERT_EQUAL(300000U, doc["wake_interval_ms"].as<unsigned int>());
    TEST_ASSERT_EQUAL(60000U, doc["timer_wake_awake_ms"].as<unsigned int>());
    TEST_ASSERT_EQUAL(120000U, doc["button_wake_awake_ms"].as<unsigned int>());
    TEST_ASSERT_EQUAL(4U, doc["wake_touch_gpio"].as<unsigned int>());
    TEST_ASSERT_EQUAL(2000U, doc["wake_touch_threshold"].as<unsigned int>());
    TEST_ASSERT_EQUAL(60000U, doc["wake_awake_window_ms"].as<unsigned int>());
    TEST_ASSERT_EQUAL(45000U, doc["wake_awake_eta_ms"].as<unsigned int>());
    TEST_ASSERT_EQUAL_STRING("normal", doc["thermal_state"].as<const char*>());
    TEST_ASSERT_EQUAL(240U, doc["thermal_cpu_mhz"].as<unsigned int>());
    TEST_ASSERT_FALSE(doc["thermal_throttled"].as<bool>());
    TEST_ASSERT_EQUAL(60, doc["thermal_soft_c"].as<int>());
    TEST_ASSERT_EQUAL(68, doc["thermal_hard_c"].as<int>());
    TEST_ASSERT_EQUAL(80, doc["thermal_critical_c"].as<int>());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_power_status_exposes_thermal_diagnostics_without_activity_side_effect);
    return UNITY_END();
}
