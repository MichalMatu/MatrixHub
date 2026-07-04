#include <unity.h>

#include <atomic>
#include <cstdarg>

#include "../../lib/ArduinoJson/src/ArduinoJson.h"
#include "../../test/stubs/esp_log.h"
#include "../../test/stubs/WiFi.h"

#define private public
#define protected public
#include "../../lib/framework/wifi/WiFiSettingsService.h"
#include "../../lib/framework/utils/SettingValue.cpp"
#include "../../lib/framework/wifi/WiFiSettingsService.cpp"
#undef private
#undef protected

#include "../../src/system/rtc/types/RtcNetworkState.h"
#include "../../src/system/health/wifi/WifiHealthTracker.h"
#include "../../src/system/logging/Logging.h"
#include "../../lib/framework/services/RestartService.h"

WiFiClass WiFi;
SemaphoreHandle_t g_fsMutex = nullptr;

update_handler_id_t StateUpdateHandlerInfo::currentUpdatedHandlerId = 0;
hook_handler_id_t StateHookHandlerInfo::currentHookHandlerId = 0;

namespace RTC {
RtcNetworkState networkState;
}

namespace SYSTEM::HEALTH {
const WiFiHealth& WifiHealthTracker::getHealth() {
    static WiFiHealth health{};
    return health;
}
}

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
}

namespace {
bool s_apStartShouldSucceed = true;
int s_apStartCalls = 0;
int s_apStopCalls = 0;
}

APSettingsService::APSettingsService(PsychicHttpServer* server,
                                     FS* fs,
                                     SecurityManager* securityManager)
    : _server(server),
      _securityManager(securityManager),
      _httpEndpoint(APSettings::read,
                    APSettings::update,
                    this,
                    server,
                    AP_SETTINGS_SERVICE_PATH,
                    securityManager,
                    AuthenticationPredicates::IS_ADMIN,
                    APSettings::validate),
      _fsPersistence(APSettings::read, APSettings::update, this, fs, AP_SETTINGS_FILE),
      _dnsServer(nullptr) {
}

bool APSettingsService::startAccessPoint() {
    ++s_apStartCalls;
    _apStarted = s_apStartShouldSucceed;
    if (_apStarted) {
        WiFi.mode(WIFI_AP);
    }
    return s_apStartShouldSucceed;
}

void APSettingsService::stopAccessPoint() {
    if (_apStarted) {
        ++s_apStopCalls;
    }
    _apStarted = false;
    WiFi.softAPdisconnect(true);
}

namespace {

wifi_settings_t configuredNetwork() {
    wifi_settings_t network{};
    network.ssid = "matrixhub";
    network.password = "secret";
    network.staticIPConfig = false;
    network.localIP = INADDR_NONE;
    network.gatewayIP = INADDR_NONE;
    network.subnetMask = INADDR_NONE;
    network.dnsIP1 = INADDR_NONE;
    network.dnsIP2 = INADDR_NONE;
    return network;
}

void configureStationService(WiFiSettingsService& service) {
    service._state = WiFiSettings{};
    service._state.hostname = "device";
    service._state.mode = WiFiOperatingMode::Station;
    service._state.wifiSettings.push_back(configuredNetwork());
}

void configureApService(WiFiSettingsService& service) {
    service._state = WiFiSettings{};
    service._state.hostname = "device";
    service._state.mode = WiFiOperatingMode::AccessPoint;
}

void configureOffService(WiFiSettingsService& service) {
    service._state = WiFiSettings{};
    service._state.hostname = "device";
    service._state.mode = WiFiOperatingMode::Off;
}

}  // namespace

void setUp(void) {
    TEST_STUBS::WIFI::reset();
    TEST_STUBS::ARDUINO::millisValue = 0;
    s_apStartShouldSucceed = true;
    s_apStartCalls = 0;
    s_apStopCalls = 0;
    RestartService::restartNow();
}

void tearDown(void) {}

void test_default_settings_without_networks_start_in_ap_mode() {
    WiFiSettings settings;

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    const StateUpdateResult result = WiFiSettings::update(root, settings, WIFI_SETTINGS_FILE);

    TEST_ASSERT_EQUAL(static_cast<int>(StateUpdateResult::CHANGED), static_cast<int>(result));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WiFiOperatingMode::AccessPoint),
                            static_cast<uint8_t>(settings.mode));
    TEST_ASSERT_TRUE(settings.wifiSettings.empty());

    PsychicHttpServer server;
    SecurityManager security;
    FS fs;
    WiFiSettingsService service(&server, &fs, &security);
    APSettingsService apService(&server, &fs, &security);
    service._state = settings;
    service.setAPSettingsService(&apService);

    service.applyConfiguredMode();

    TEST_ASSERT_EQUAL(1, s_apStartCalls);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WIFI_AP), static_cast<int>(TEST_STUBS::WIFI::mode));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WiFiConnectivityState::ApOnly),
                          static_cast<int>(service._connectivityState));
}

void test_station_mode_retries_sta_only_and_never_starts_ap() {
    PsychicHttpServer server;
    SecurityManager security;
    FS fs;
    WiFiSettingsService service(&server, &fs, &security);
    APSettingsService apService(&server, &fs, &security);
    configureStationService(service);
    service.setAPSettingsService(&apService);
    TEST_STUBS::WIFI::connected = false;
    TEST_STUBS::WIFI::status = WL_DISCONNECTED;

    service.applyConfiguredMode();
    TEST_ASSERT_EQUAL(1, TEST_STUBS::WIFI::beginCalls);
    TEST_ASSERT_EQUAL(0, s_apStartCalls);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WIFI_STA), static_cast<int>(TEST_STUBS::WIFI::mode));

    service._connectingSince = 0;
    service.connectToWiFi();
    service._connectingSince = 0;
    service.connectToWiFi();
    service._connectingSince = 0;
    service.connectToWiFi();

    TEST_ASSERT_EQUAL(0, s_apStartCalls);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WIFI_STA), static_cast<int>(TEST_STUBS::WIFI::mode));
    TEST_ASSERT_TRUE(service._retryBackoffActive);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WiFiConnectivityState::StaBackoff),
                          static_cast<int>(service._connectivityState));
}

void test_station_mode_without_networks_is_rejected_from_http_update() {
    WiFiSettings settings;

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["hostname"] = "matrixhub";
    root["mode"] = "sta";
    root["wifi_networks"].to<JsonArray>();

    const StateUpdateResult result = WiFiSettings::update(root, settings, HTTP_ENDPOINT_ORIGIN_ID);

    TEST_ASSERT_EQUAL(static_cast<int>(StateUpdateResult::ERROR), static_cast<int>(result));
}

void test_ap_mode_starts_ap_only_without_sta_attempts() {
    PsychicHttpServer server;
    SecurityManager security;
    FS fs;
    WiFiSettingsService service(&server, &fs, &security);
    APSettingsService apService(&server, &fs, &security);
    configureApService(service);
    service.setAPSettingsService(&apService);

    service.applyConfiguredMode();

    TEST_ASSERT_EQUAL(1, s_apStartCalls);
    TEST_ASSERT_EQUAL(0, TEST_STUBS::WIFI::beginCalls);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WIFI_AP), static_cast<int>(TEST_STUBS::WIFI::mode));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WiFiConnectivityState::ApOnly),
                          static_cast<int>(service._connectivityState));
}

void test_off_mode_turns_radio_off_and_does_not_reconnect() {
    PsychicHttpServer server;
    SecurityManager security;
    FS fs;
    WiFiSettingsService service(&server, &fs, &security);
    APSettingsService apService(&server, &fs, &security);
    configureOffService(service);
    service.setAPSettingsService(&apService);

    service.applyConfiguredMode();
    service.loop();

    TEST_ASSERT_EQUAL(0, TEST_STUBS::WIFI::beginCalls);
    TEST_ASSERT_EQUAL(0, s_apStartCalls);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WIFI_OFF), static_cast<int>(TEST_STUBS::WIFI::mode));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WiFiConnectivityState::Off),
                          static_cast<int>(service._connectivityState));
}

void test_recovery_request_in_sta_resets_retry_without_starting_ap() {
    PsychicHttpServer server;
    SecurityManager security;
    FS fs;
    WiFiSettingsService service(&server, &fs, &security);
    APSettingsService apService(&server, &fs, &security);
    configureStationService(service);
    service.setAPSettingsService(&apService);
    TEST_STUBS::WIFI::connected = false;
    TEST_STUBS::WIFI::status = WL_DISCONNECTED;
    service._connectionAttempts = 2;
    service._backoffLevel = 3;
    service._retryBackoffActive = true;
    service._lastConnectionAttempt = 1234;

    TEST_ASSERT_TRUE(service.requestRecovery("health"));
    TEST_ASSERT_TRUE(service.processRecoveryRequest());

    TEST_ASSERT_EQUAL(0, s_apStartCalls);
    TEST_ASSERT_EQUAL(1, TEST_STUBS::WIFI::disconnectCalls);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WIFI_STA), static_cast<int>(TEST_STUBS::WIFI::mode));
    TEST_ASSERT_EQUAL(0, service._connectionAttempts);
    TEST_ASSERT_EQUAL(0, service._backoffLevel);
    TEST_ASSERT_FALSE(service._retryBackoffActive);
    TEST_ASSERT_EQUAL(0UL, service._lastConnectionAttempt);
    TEST_ASSERT_EQUAL_STRING("health", service._lastRecoveryReason);
}

void test_recovery_request_outside_sta_mode_is_ignored() {
    PsychicHttpServer server;
    SecurityManager security;
    FS fs;
    WiFiSettingsService service(&server, &fs, &security);
    configureApService(service);

    TEST_ASSERT_FALSE(service.requestRecovery("health"));
    TEST_ASSERT_FALSE(service._recoveryRequested);
}

void test_same_mode_update_does_not_schedule_restart() {
    PsychicHttpServer server;
    SecurityManager security;
    FS fs;
    WiFiSettingsService service(&server, &fs, &security);
    configureApService(service);

    TEST_ASSERT_TRUE(service.setModeAndRestart(WiFiOperatingMode::AccessPoint));
    TEST_ASSERT_FALSE(RestartService::isRestartPending());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_default_settings_without_networks_start_in_ap_mode);
    RUN_TEST(test_station_mode_retries_sta_only_and_never_starts_ap);
    RUN_TEST(test_station_mode_without_networks_is_rejected_from_http_update);
    RUN_TEST(test_ap_mode_starts_ap_only_without_sta_attempts);
    RUN_TEST(test_off_mode_turns_radio_off_and_does_not_reconnect);
    RUN_TEST(test_recovery_request_in_sta_resets_retry_without_starting_ap);
    RUN_TEST(test_recovery_request_outside_sta_mode_is_ignored);
    RUN_TEST(test_same_mode_update_does_not_schedule_restart);
    return UNITY_END();
}
