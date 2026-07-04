#include <unity.h>

#include <LittleFS.h>
#include <string>

#include "../../test/stubs/PsychicHttpServer.h"
#include "../../test/stubs/PsychicJson.h"
#include "../../test/stubs/PsychicRequest.h"
#include "../../test/stubs/WiFi.h"
#include "../../test/stubs/WiFiUdp.h"
#include "../../test/stubs/freertos/semphr.h"
#include "../../test/stubs/freertos/task.h"

#include "../../src/sensors/SensorLoggingTask.h"
#include "../../src/system/power/PowerManager.h"
#define private public
#include "../../src/udp/UdpPusher.h"
#undef private

#include "../../src/system/logging/Logging.h"
#include "../../src/system/rtc/RtcConfig.h"

extern "C" {
#include "esp_http_server.h"
esp_err_t httpd_resp_set_type(httpd_req_t* r, const char* type) {
    (void)r;
    (void)type;
    return ESP_OK;
}
esp_err_t httpd_resp_send_chunk(httpd_req_t* r, const char* buf, ssize_t len) {
    (void)r;
    (void)buf;
    (void)len;
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

const char* Logging::levelToString(esp_log_level_t level) {
    (void)level;
    return "info";
}

esp_log_level_t Logging::stringToLevel(std::string_view name, esp_log_level_t fallback) {
    (void)name;
    return fallback;
}

}  // namespace LOG

namespace RTC {

ConfigStore mockStore{};
ConfigStore* store = &mockStore;
RtcRuntimeStats runtimeStats{};

const ConfigStore& getConfig() {
    return mockStore;
}

ConfigStore getConfigSafeCopy() {
    return mockStore;
}

ConfigStore& getMutableConfig() {
    return mockStore;
}

void withConfig(const std::function<void(const ConfigStore&)>& reader) {
    reader(mockStore);
}

SemaphoreHandle_t getLock() {
    static SemaphoreHandle_t lock = xSemaphoreCreateMutex();
    return lock;
}

bool updateConfig(const std::function<void(ConfigStore&)>& updater) {
    updater(mockStore);
    return true;
}

bool updateConfig(const std::function<void(ConfigStore&)>& updater, TickType_t timeoutTicks) {
    (void)timeoutTicks;
    updater(mockStore);
    return true;
}

void markValid() {}

}  // namespace RTC

namespace CONFIG {

bool save(FS& fs) {
    (void)fs;
    return true;
}

namespace JSON {

void deserializeUdpPusher(JsonObject& obj, RTC::UdpPusherData& data) {
    if (obj["enabled"].is<bool>()) {
        data.enabled = obj["enabled"].as<bool>();
    }
    if (obj["host"].is<const char*>()) {
        strncpy(data.host, obj["host"].as<const char*>(), sizeof(data.host));
        data.host[sizeof(data.host) - 1] = '\0';
    }
    if (obj["port"].is<uint16_t>()) {
        data.port = obj["port"].as<uint16_t>();
    }
}

}  // namespace JSON
}  // namespace CONFIG

namespace POWER {

void PowerManager::notifyActivity(const char* source) {
    (void)source;
}

}  // namespace POWER

WiFiClass WiFi;
FS LittleFS;

namespace {

SensorSnapshot gSnapshot{};

void configureValidUdp() {
    RTC::mockStore = RTC::ConfigStore{};
    RTC::mockStore.udpPusher.enabled = true;
    strncpy(RTC::mockStore.udpPusher.host, "telegraf.local", sizeof(RTC::mockStore.udpPusher.host));
    RTC::mockStore.udpPusher.host[sizeof(RTC::mockStore.udpPusher.host) - 1] = '\0';
    RTC::mockStore.udpPusher.port = 8094;
    RTC::mockStore.udpPusher.format = RTC::UdpFormat::Json;
    RTC::mockStore.udpPusher.intervalMs = 1000;
}

void resetStubs() {
    static uint32_t nextNowMs = 0;
    nextNowMs += 60000;
    TEST_STUBS::ARDUINO::millisValue = nextNowMs;
    TEST_STUBS::FREERTOS::resetTaskCreateStub();
    TEST_STUBS::FREERTOS::resetSemaphoreStubs();
    TEST_STUBS::WIFI::reset();
    TEST_STUBS::WIFIUDP::reset();
    RTC::runtimeStats = {};
    configureValidUdp();
    gSnapshot = SensorSnapshot{};
    gSnapshot.timestamp_ms = 123456;
    gSnapshot.co2 = 678;
    gSnapshot.temp = 24.5f;
    gSnapshot.humid = 45.2f;
    gSnapshot.seq = 7;
}

PsychicRequest makeJsonRequest(const std::string& body = "{}") {
    PsychicRequest request;
    request.setContentType("application/json");
    request.setBody(body);
    return request;
}

}  // namespace

SensorSnapshot SensorLoggingTask::getLastGoodSnapshot() {
    return gSnapshot;
}

#include "../../src/udp/UdpPacketFormatter.cpp"
#define private public
#include "../../src/udp/UdpPusher.cpp"
#undef private
#include "../../src/udp/UdpSettingsService.cpp"
#include "../../src/api/udp/UdpApiService.cpp"

update_handler_id_t StateUpdateHandlerInfo::currentUpdatedHandlerId = 0;
hook_handler_id_t StateHookHandlerInfo::currentHookHandlerId = 0;

void setUp(void) {
    resetStubs();
}

void tearDown(void) {}

void test_udp_test_endpoint_returns_sent_for_successful_sync_push() {
    TEST_STUBS::FREERTOS::taskCreateResult = pdFAIL;
    PsychicHttpServer server;
    SecurityManager securityManager;
    UDPPUSH::UdpPusher pusher;
    API::UdpApiService api(&server, &securityManager, nullptr, &pusher, nullptr);
    api.begin();
    PsychicRequest request = makeJsonRequest();

    const esp_err_t err = server.invoke("/api/udp/test", HTTP_POST, &request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(200, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"status\":\"sent\"") != std::string::npos);
}

void test_udp_test_endpoint_returns_queued_when_worker_accepts_request() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    UDPPUSH::UdpPusher pusher;
    API::UdpApiService api(&server, &securityManager, nullptr, &pusher, nullptr);
    api.begin();
    PsychicRequest request = makeJsonRequest();

    const esp_err_t err = server.invoke("/api/udp/test", HTTP_POST, &request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(200, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"status\":\"queued\"") != std::string::npos);
}

void test_udp_test_endpoint_returns_wifi_disconnected_when_wifi_is_down() {
    TEST_STUBS::FREERTOS::taskCreateResult = pdFAIL;
    TEST_STUBS::WIFI::status = 0;
    TEST_STUBS::WIFI::connected = false;
    PsychicHttpServer server;
    SecurityManager securityManager;
    UDPPUSH::UdpPusher pusher;
    API::UdpApiService api(&server, &securityManager, nullptr, &pusher, nullptr);
    api.begin();
    PsychicRequest request = makeJsonRequest();

    const esp_err_t err = server.invoke("/api/udp/test", HTTP_POST, &request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(503, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"status\":\"wifi_disconnected\"") != std::string::npos);
}

void test_udp_test_endpoint_returns_send_failed_when_begin_packet_fails() {
    TEST_STUBS::FREERTOS::taskCreateResult = pdFAIL;
    TEST_STUBS::WIFIUDP::beginPacketResult = false;
    PsychicHttpServer server;
    SecurityManager securityManager;
    UDPPUSH::UdpPusher pusher;
    API::UdpApiService api(&server, &securityManager, nullptr, &pusher, nullptr);
    api.begin();
    PsychicRequest request = makeJsonRequest();

    const esp_err_t err = server.invoke("/api/udp/test", HTTP_POST, &request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(502, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"status\":\"send_failed\"") != std::string::npos);
}

void test_udp_test_endpoint_returns_send_failed_when_end_packet_fails() {
    TEST_STUBS::FREERTOS::taskCreateResult = pdFAIL;
    TEST_STUBS::WIFIUDP::endPacketResult = false;
    PsychicHttpServer server;
    SecurityManager securityManager;
    UDPPUSH::UdpPusher pusher;
    API::UdpApiService api(&server, &securityManager, nullptr, &pusher, nullptr);
    api.begin();
    PsychicRequest request = makeJsonRequest();

    const esp_err_t err = server.invoke("/api/udp/test", HTTP_POST, &request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(502, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"status\":\"send_failed\"") != std::string::npos);
}

void test_udp_config_rejects_enabled_empty_host() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    UDPPUSH::UdpSettingsService settings(&LittleFS);
    UDPPUSH::UdpPusher pusher;
    API::UdpApiService api(&server, &securityManager, nullptr, &pusher, &settings);
    api.begin();
    PsychicRequest request = makeJsonRequest(
        "{\"enabled\":true,\"host\":\"\",\"port\":8094,\"format\":\"json\",\"interval_ms\":60000}");

    const esp_err_t err = server.invoke("/api/udp", HTTP_POST, &request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(400, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("input/invalid_host") != std::string::npos);
}

void test_udp_config_rejects_enabled_host_with_whitespace() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    UDPPUSH::UdpSettingsService settings(&LittleFS);
    UDPPUSH::UdpPusher pusher;
    API::UdpApiService api(&server, &securityManager, nullptr, &pusher, &settings);
    api.begin();
    PsychicRequest request = makeJsonRequest(
        "{\"enabled\":true,\"host\":\"bad host.local\",\"port\":8094,\"format\":\"json\",\"interval_ms\":60000}");

    const esp_err_t err = server.invoke("/api/udp", HTTP_POST, &request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(400, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("input/invalid_host") != std::string::npos);
}

void test_udp_config_rejects_port_zero_before_clamp() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    UDPPUSH::UdpSettingsService settings(&LittleFS);
    UDPPUSH::UdpPusher pusher;
    API::UdpApiService api(&server, &securityManager, nullptr, &pusher, &settings);
    api.begin();
    PsychicRequest request = makeJsonRequest(
        "{\"enabled\":true,\"host\":\"telegraf.local\",\"port\":0,\"format\":\"json\",\"interval_ms\":60000}");

    const esp_err_t err = server.invoke("/api/udp", HTTP_POST, &request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(400, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("input/invalid_port") != std::string::npos);
}

void test_udp_config_allows_disabled_empty_host() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    UDPPUSH::UdpSettingsService settings(&LittleFS);
    UDPPUSH::UdpPusher pusher;
    API::UdpApiService api(&server, &securityManager, nullptr, &pusher, &settings);
    api.begin();
    PsychicRequest request = makeJsonRequest(
        "{\"enabled\":false,\"host\":\"\",\"port\":8094,\"format\":\"json\",\"interval_ms\":60000}");

    const esp_err_t err = server.invoke("/api/udp", HTTP_POST, &request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(200, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"enabled\":false") != std::string::npos);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_udp_test_endpoint_returns_sent_for_successful_sync_push);
    RUN_TEST(test_udp_test_endpoint_returns_queued_when_worker_accepts_request);
    RUN_TEST(test_udp_test_endpoint_returns_wifi_disconnected_when_wifi_is_down);
    RUN_TEST(test_udp_test_endpoint_returns_send_failed_when_begin_packet_fails);
    RUN_TEST(test_udp_test_endpoint_returns_send_failed_when_end_packet_fails);
    RUN_TEST(test_udp_config_rejects_enabled_empty_host);
    RUN_TEST(test_udp_config_rejects_enabled_host_with_whitespace);
    RUN_TEST(test_udp_config_rejects_port_zero_before_clamp);
    RUN_TEST(test_udp_config_allows_disabled_empty_host);
    return UNITY_END();
}
