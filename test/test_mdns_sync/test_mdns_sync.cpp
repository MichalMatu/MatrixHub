#include <unity.h>

#include <atomic>
#include <cstdarg>
#include <string_view>

#define ESP32SVELTEKIT_USE_REAL_IMPL
#define private public
#define protected public
#include "../../lib/framework/core/ESP32SvelteKit.cpp"
#undef private
#undef protected

WiFiClass WiFi;
FS LittleFS;

namespace LOG {

Settings Logging::_settings{ESP_LOG_INFO};

void Logging::begin(const Settings& settings) { _settings = settings; }
void Logging::setLevel(esp_log_level_t level) { _settings.level = level; }
Settings Logging::settings() { return _settings; }
bool Logging::isEnabled(esp_log_level_t level) { return level <= _settings.level; }
void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    (void)level;
    (void)tag;
    (void)fmt;
}
void Logging::clearBuffer() {}
const char* Logging::levelToString(esp_log_level_t level) {
    (void)level;
    return "INFO";
}
esp_log_level_t Logging::stringToLevel(std::string_view name, esp_log_level_t fallback) {
    (void)name;
    return fallback;
}
void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {
    (void)taskName;
    (void)stackSize;
}
void Logging::logSection(const char* title) {
    (void)title;
}
void Logging::suppressNoisyModules() {}

}  // namespace LOG

namespace SYSTEM::HEALTH {

HttpServerHealthSnapshot HttpServerHealthTracker::_snapshot{};
uint32_t HttpServerHealthTracker::_recentCloseWindowStartMs = 0;
uint8_t HttpServerHealthTracker::_recentCloseBurstCount = 0;

void HttpServerHealthTracker::reset() {
    _snapshot = {};
    _recentCloseWindowStartMs = 0;
    _recentCloseBurstCount = 0;
}

void HttpServerHealthTracker::recordOpen() {
    ++_snapshot.openCount;
}

void HttpServerHealthTracker::recordClose() {
    ++_snapshot.closeCount;
}

void HttpServerHealthTracker::recordWsOpen() {
    ++_snapshot.wsOpenCount;
    ++_snapshot.wsActiveClients;
    if (_snapshot.wsActiveClients > _snapshot.wsPeakClients) {
        _snapshot.wsPeakClients = _snapshot.wsActiveClients;
    }
}

void HttpServerHealthTracker::recordWsClose() {
    ++_snapshot.wsCloseCount;
    if (_snapshot.wsActiveClients > 0) {
        --_snapshot.wsActiveClients;
    }
}

void HttpServerHealthTracker::recordWsForcedRemoval(int fd) {
    (void)fd;
    ++_snapshot.wsForcedRemovals;
}

void HttpServerHealthTracker::recordWsQueueDrop(size_t payloadLen) {
    ++_snapshot.wsQueueDrops;
    _snapshot.lastWsQueueDropPayload = static_cast<uint32_t>(payloadLen);
}

void HttpServerHealthTracker::recordWsHeapFallback(size_t payloadLen) {
    ++_snapshot.wsHeapFallbacks;
    _snapshot.lastWsHeapFallbackPayload = static_cast<uint32_t>(payloadLen);
}

HttpServerHealthSnapshot HttpServerHealthTracker::getSnapshot() {
    return _snapshot;
}

}  // namespace SYSTEM::HEALTH

namespace {
void resetTestState() {
    TEST_STUBS::ARDUINO::millisValue = 0;
    TEST_STUBS::WIFI::reset();
    TEST_STUBS::MDNS::reset();
    TEST_STUBS::MDNS_NATIVE::reset();
    TEST_STUBS::NETIF::reset();
}

void assertAction(const std::pair<String, mdns_event_actions_t>& action,
                  const char* ifKey,
                  mdns_event_actions_t expectedFlags) {
    TEST_ASSERT_EQUAL_STRING(ifKey, action.first.c_str());
    TEST_ASSERT_EQUAL_INT(expectedFlags, action.second);
}

}  // namespace

void setUp(void) {
    resetTestState();
}

void tearDown(void) {}

void test_syncMDNS_starts_http_service_for_sta_interface() {
    PsychicHttpServer server;
    ESP32SvelteKit framework(&server, 45);
    framework._wifiSettingsService.hostname = "matrixhub";
    TEST_STUBS::WIFI::connected = true;
    TEST_STUBS::WIFI::status = WL_CONNECTED;
    TEST_STUBS::WIFI::mode = WIFI_STA;

    framework.requestMDNSSync();
    framework.syncMDNS();

    TEST_ASSERT_EQUAL_INT(1, TEST_STUBS::MDNS::beginCalls);
    TEST_ASSERT_EQUAL_STRING("matrixhub", TEST_STUBS::MDNS::lastHostname.c_str());
    TEST_ASSERT_EQUAL_STRING("MatrixHub", TEST_STUBS::MDNS::lastInstanceName.c_str());
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(TEST_STUBS::MDNS::services.size()));
    TEST_ASSERT_EQUAL_STRING("http:tcp:80", TEST_STUBS::MDNS::services[0].c_str());
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(TEST_STUBS::MDNS::txtItems.size()));
    TEST_ASSERT_EQUAL_STRING("http:tcp:Firmware Version:2.0.0",
                             TEST_STUBS::MDNS::txtItems[0].c_str());
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(TEST_STUBS::MDNS_NATIVE::registeredIfKeys.size()));
    TEST_ASSERT_EQUAL_STRING("WIFI_STA_DEF", TEST_STUBS::MDNS_NATIVE::registeredIfKeys[0].c_str());
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(TEST_STUBS::MDNS_NATIVE::actions.size()));
    assertAction(TEST_STUBS::MDNS_NATIVE::actions[0], "WIFI_STA_DEF",
                 MDNS_EVENT_ENABLE_IP4 | MDNS_EVENT_ANNOUNCE_IP4);
    TEST_ASSERT_TRUE(framework._mdnsStarted);
    TEST_ASSERT_FALSE(framework._mdnsSyncPending.load(std::memory_order_acquire));
}

void test_syncMDNS_starts_https_service_for_ap_sta_mode() {
    PsychicHttpServer server;
    ESP32SvelteKit framework(&server, 45);
    framework._wifiSettingsService.hostname = "matrixhub";
    framework.setMDNSAppName("MatrixHub");
    framework._ssl_cert = "cert";
    framework._ssl_key = "key";
    TEST_STUBS::WIFI::connected = true;
    TEST_STUBS::WIFI::status = WL_CONNECTED;
    TEST_STUBS::WIFI::mode = WIFI_AP_STA;

    framework.requestMDNSSync();
    framework.syncMDNS();

    TEST_ASSERT_EQUAL_INT(1, TEST_STUBS::MDNS::beginCalls);
    TEST_ASSERT_EQUAL_STRING("MatrixHub", TEST_STUBS::MDNS::lastInstanceName.c_str());
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(TEST_STUBS::MDNS::services.size()));
    TEST_ASSERT_EQUAL_STRING("https:tcp:443", TEST_STUBS::MDNS::services[0].c_str());
    TEST_ASSERT_EQUAL_INT(2, static_cast<int>(TEST_STUBS::MDNS_NATIVE::registeredIfKeys.size()));
    TEST_ASSERT_EQUAL_STRING("WIFI_STA_DEF", TEST_STUBS::MDNS_NATIVE::registeredIfKeys[0].c_str());
    TEST_ASSERT_EQUAL_STRING("WIFI_AP_DEF", TEST_STUBS::MDNS_NATIVE::registeredIfKeys[1].c_str());
    TEST_ASSERT_EQUAL_INT(2, static_cast<int>(TEST_STUBS::MDNS_NATIVE::actions.size()));
    assertAction(TEST_STUBS::MDNS_NATIVE::actions[0], "WIFI_STA_DEF",
                 MDNS_EVENT_ENABLE_IP4 | MDNS_EVENT_ANNOUNCE_IP4);
    assertAction(TEST_STUBS::MDNS_NATIVE::actions[1], "WIFI_AP_DEF",
                 MDNS_EVENT_ENABLE_IP4 | MDNS_EVENT_ANNOUNCE_IP4);
}

void test_syncMDNS_stops_running_instance_when_no_interface_is_active() {
    PsychicHttpServer server;
    ESP32SvelteKit framework(&server, 45);
    framework._mdnsStarted = true;
    TEST_STUBS::WIFI::connected = false;
    TEST_STUBS::WIFI::status = WL_DISCONNECTED;
    TEST_STUBS::WIFI::mode = WIFI_MODE_NULL;

    framework.requestMDNSSync();
    framework.syncMDNS();

    TEST_ASSERT_EQUAL_INT(1, TEST_STUBS::MDNS::endCalls);
    TEST_ASSERT_EQUAL_INT(0, TEST_STUBS::MDNS::beginCalls);
    TEST_ASSERT_FALSE(framework._mdnsStarted);
    TEST_ASSERT_FALSE(framework._mdnsSyncPending.load(std::memory_order_acquire));
}

void test_syncMDNS_leaves_request_pending_until_retry_interval_after_begin_failure() {
    PsychicHttpServer server;
    ESP32SvelteKit framework(&server, 45);
    framework._wifiSettingsService.hostname = "matrixhub";
    TEST_STUBS::WIFI::connected = true;
    TEST_STUBS::WIFI::status = WL_CONNECTED;
    TEST_STUBS::WIFI::mode = WIFI_STA;
    TEST_STUBS::MDNS::beginResult = false;

    framework.requestMDNSSync();
    framework.syncMDNS();

    TEST_ASSERT_EQUAL_INT(1, TEST_STUBS::MDNS::beginCalls);
    TEST_ASSERT_TRUE(framework._mdnsSyncPending.load(std::memory_order_acquire));
    TEST_ASSERT_FALSE(framework._mdnsStarted);

    TEST_STUBS::MDNS::beginResult = true;
    TEST_STUBS::ARDUINO::millisValue = 500;
    framework.syncMDNS();
    TEST_ASSERT_EQUAL_INT(1, TEST_STUBS::MDNS::beginCalls);

    TEST_STUBS::ARDUINO::millisValue = 1001;
    framework.syncMDNS();
    TEST_ASSERT_EQUAL_INT(2, TEST_STUBS::MDNS::beginCalls);
    TEST_ASSERT_TRUE(framework._mdnsStarted);
    TEST_ASSERT_FALSE(framework._mdnsSyncPending.load(std::memory_order_acquire));
}

void test_syncMDNS_skips_empty_hostname_and_clears_pending_flag() {
    PsychicHttpServer server;
    ESP32SvelteKit framework(&server, 45);
    framework._wifiSettingsService.hostname = "";
    TEST_STUBS::WIFI::connected = true;
    TEST_STUBS::WIFI::status = WL_CONNECTED;
    TEST_STUBS::WIFI::mode = WIFI_STA;

    framework.requestMDNSSync();
    framework.syncMDNS();

    TEST_ASSERT_EQUAL_INT(0, TEST_STUBS::MDNS::beginCalls);
    TEST_ASSERT_FALSE(framework._mdnsSyncPending.load(std::memory_order_acquire));
    TEST_ASSERT_FALSE(framework._mdnsStarted);
}

void test_syncMDNS_stops_running_instance_when_hostname_becomes_empty() {
    PsychicHttpServer server;
    ESP32SvelteKit framework(&server, 45);
    framework._wifiSettingsService.hostname = "";
    framework._mdnsStarted = true;
    TEST_STUBS::WIFI::connected = true;
    TEST_STUBS::WIFI::status = WL_CONNECTED;
    TEST_STUBS::WIFI::mode = WIFI_STA;

    framework.requestMDNSSync();
    framework.syncMDNS();

    TEST_ASSERT_EQUAL_INT(1, TEST_STUBS::MDNS::endCalls);
    TEST_ASSERT_EQUAL_INT(0, TEST_STUBS::MDNS::beginCalls);
    TEST_ASSERT_FALSE(framework._mdnsStarted);
    TEST_ASSERT_FALSE(framework._mdnsSyncPending.load(std::memory_order_acquire));
}

void test_hostname_change_requests_immediate_mdns_resync() {
    PsychicHttpServer server;
    ESP32SvelteKit framework(&server, 45);
    framework.begin(false);

    TEST_STUBS::MDNS::reset();
    TEST_STUBS::MDNS_NATIVE::reset();
    framework._mdnsSyncPending.store(false, std::memory_order_release);
    framework._mdnsStarted = true;
    framework._hasMdnsSyncAttempt = true;
    framework._lastMdnsSyncAttemptMs = 900;
    framework._wifiSettingsService.hostname = "renamed-device";
    framework._wifiSettingsService.notifyHostnameChanged();
    TEST_STUBS::WIFI::connected = true;
    TEST_STUBS::WIFI::status = WL_CONNECTED;
    TEST_STUBS::WIFI::mode = WIFI_STA;
    TEST_STUBS::ARDUINO::millisValue = 1000;

    framework.syncMDNS();

    TEST_ASSERT_EQUAL_INT(1, TEST_STUBS::MDNS::endCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_STUBS::MDNS::beginCalls);
    TEST_ASSERT_EQUAL_STRING("renamed-device", TEST_STUBS::MDNS::lastHostname.c_str());
    TEST_ASSERT_TRUE(framework._mdnsStarted);
    TEST_ASSERT_FALSE(framework._mdnsSyncPending.load(std::memory_order_acquire));
}

void test_registerMDNSEventHandlers_marks_sync_pending_on_wifi_events() {
    PsychicHttpServer server;
    ESP32SvelteKit framework(&server, 45);

    framework.registerMDNSEventHandlers();

    TEST_ASSERT_EQUAL_INT(4, TEST_STUBS::WIFI::onEventCalls);
    TEST_ASSERT_FALSE(framework._mdnsSyncPending.load(std::memory_order_acquire));

    TEST_STUBS::WIFI::fireEvent(ARDUINO_EVENT_WIFI_STA_GOT_IP);
    TEST_ASSERT_TRUE(framework._mdnsSyncPending.load(std::memory_order_acquire));

    framework._mdnsSyncPending.store(false, std::memory_order_release);
    TEST_STUBS::WIFI::fireEvent(ARDUINO_EVENT_WIFI_AP_STOP);
    TEST_ASSERT_TRUE(framework._mdnsSyncPending.load(std::memory_order_acquire));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_syncMDNS_starts_http_service_for_sta_interface);
    RUN_TEST(test_syncMDNS_starts_https_service_for_ap_sta_mode);
    RUN_TEST(test_syncMDNS_stops_running_instance_when_no_interface_is_active);
    RUN_TEST(test_syncMDNS_leaves_request_pending_until_retry_interval_after_begin_failure);
    RUN_TEST(test_syncMDNS_skips_empty_hostname_and_clears_pending_flag);
    RUN_TEST(test_syncMDNS_stops_running_instance_when_hostname_becomes_empty);
    RUN_TEST(test_hostname_change_requests_immediate_mdns_resync);
    RUN_TEST(test_registerMDNSEventHandlers_marks_sync_pending_on_wifi_events);
    return UNITY_END();
}
