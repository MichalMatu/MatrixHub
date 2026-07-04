#include <network/NTPSettingsService.h>
#include <utils/ResponseUtils.h>
#include "../../src/system/errors/ErrorCodes.h"

#include <stdint.h>
#include <time.h>
#include <lwip/apps/sntp.h>

#ifdef CONFIG_LWIP_TCPIP_CORE_LOCKING
#include "lwip/priv/tcpip_priv.h"
#endif

// Some Arduino-ESP32 lwIP header sets don't expose sntp_set_sync_interval(),
// even though the symbol is present in the linked lwIP library.
extern "C" void sntp_set_sync_interval(uint32_t interval_ms);

namespace {
constexpr int kMinValidYear = 2025;
constexpr int kMaxValidYear = 2040;

bool isSystemTimeValid()
{
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    const int year = timeinfo.tm_year + 1900;
    return year >= kMinValidYear && year <= kMaxValidYear;
}
} // namespace

NTPSettingsService::NTPSettingsService(PsychicHttpServer *server,
                                       FS *fs,
                                       SecurityManager *securityManager) : _server(server),
                                                                           _securityManager(securityManager),
                                                                           _httpEndpoint(NTPSettings::read, NTPSettings::update, this, server, NTP_SETTINGS_SERVICE_PATH, securityManager),
                                                                           _fsPersistence(NTPSettings::read, NTPSettings::update, this, fs, NTP_SETTINGS_FILE)
{
    addUpdateHandler([&](std::string_view originId) {
        (void)originId;
        configureNTP();
        return StateHandlerResult::success();
    }, false);
}

void NTPSettingsService::begin()
{
    WiFi.onEvent(
        [this](WiFiEvent_t event, WiFiEventInfo_t info)
        { this->onNetworkDisconnected(event, info); },
        WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
        
    WiFi.onEvent(
        [this](WiFiEvent_t event, WiFiEventInfo_t info)
        { this->onNetworkGotIP(event, info); },
        WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);

    _httpEndpoint.begin();
    _server->on(TIME_PATH,
                HTTP_POST,
                _securityManager->wrapCallback(
                    [this](PsychicRequest *request, JsonVariant &json)
                    { return this->configureTime(request, json); },
                    AuthenticationPredicates::IS_ADMIN));

    ESP_LOGV(SVK_TAG, "Registered POST endpoint: %s", TIME_PATH);

    _fsPersistence.readFromFS();
    configureNTP();
}

void NTPSettingsService::onNetworkGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
    ESP_LOGI(SVK_TAG, "Got IP address, starting NTP synchronization");
    configureNTP();
}

void NTPSettingsService::onNetworkDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    ESP_LOGI(SVK_TAG, "Network connection dropped, stopping NTP");
    configureNTP();
}

void NTPSettingsService::configureNTP()
{
    // Ensure we always have a valid TZ string; fallback to UTC0 if not set
    String tzString = _state.tzFormat.length() ? _state.tzFormat : String("UTC0");



    bool networkConnected = WiFi.isConnected();

    // Apply TZ immediately so localtime()/gmtime() use the expected offset even before SNTP completes
    setenv("TZ", tzString.c_str(), 1);
    tzset();

    if (networkConnected && _state.enabled)
    {
        ESP_LOGI(SVK_TAG, "Starting NTP... (server: %s)", _state.server.c_str());
        // Limit periodic SNTP sync to once per day to reduce network/CPU churn.
        // [CRASH FIX] sntp_set_sync_interval causes crash on ESP32 / IDF 5.x
        // It might be calling into uninitialized LwIP SNTP state or having ABI mismatch.
        // sntp_set_sync_interval(24UL * 60UL * 60UL * 1000UL);
        configTzTime(tzString.c_str(), _state.server.c_str());
    }
    else
    {

#ifdef CONFIG_LWIP_TCPIP_CORE_LOCKING
        if (!sys_thread_tcpip(LWIP_CORE_LOCK_QUERY_HOLDER))
        {
            LOCK_TCPIP_CORE();
        }
#endif
        sntp_stop();

#ifdef CONFIG_LWIP_TCPIP_CORE_LOCKING
        if (sys_thread_tcpip(LWIP_CORE_LOCK_QUERY_HOLDER))
        {
            UNLOCK_TCPIP_CORE();
        }
#endif
    }
}

esp_err_t NTPSettingsService::configureTime(PsychicRequest *request, JsonVariant &json)
{
    // Allow manual time set if system time is clearly invalid, even when NTP is enabled.
    const bool timeValid = isSystemTimeValid();
    if (sntp_enabled() && timeValid) {
        ESP_LOGW(SVK_TAG, "Manual time rejected: NTP is enabled and system time is valid.");
        return Response::error(request, 400, ErrorCodes::Ntp::MANUAL_TIME_REQUIRES_NTP_DISABLED,
            "NTP must be disabled before setting manual time");
    }
    
    if (!json.is<JsonObject>()) {
        return Response::error(request, 400, ErrorCodes::Input::INVALID_JSON, "Invalid JSON payload");
    }
    
    struct tm tm = {0};
    String timeLocal = json["local_time"];
    
    if (timeLocal.isEmpty()) {
        return Response::error(request, 400, ErrorCodes::Ntp::MISSING_LOCAL_TIME, "Missing local_time field");
    }
    
    char *s = strptime(timeLocal.c_str(), "%Y-%m-%dT%H:%M:%S", &tm);
    if (s == nullptr) {
        ESP_LOGW(SVK_TAG, "Manual time rejected: invalid format '%s'", timeLocal.c_str());
        return Response::error(request, 400, ErrorCodes::Input::INVALID_TIME_FORMAT,
            "Invalid time format. Expected: YYYY-MM-DDTHH:MM:SS");
    }
    
    // Validate year range (2025-2040)
    int year = tm.tm_year + 1900;
    if (year < 2025 || year > 2040) {
        ESP_LOGW(SVK_TAG, "Manual time rejected: year %d out of range [2025-2040]", year);
        return Response::error(request, 400, ErrorCodes::Input::YEAR_OUT_OF_RANGE,
            "Year out of valid range (2025-2040)");
    }
    
    time_t time = mktime(&tm);
    struct timeval now = {.tv_sec = time};

        // Debug: log old/new system time (local) before and after settimeofday
        time_t before = ::time(nullptr);
        struct tm beforeLocal;
        localtime_r(&before, &beforeLocal);

    settimeofday(&now, nullptr);

        time_t after = ::time(nullptr);
        struct tm afterLocal;
        localtime_r(&after, &afterLocal);

        ESP_LOGI(SVK_TAG,
             "Manual time set: old=%04d-%02d-%02dT%02d:%02d:%02d new=%04d-%02d-%02dT%02d:%02d:%02d tz=%s",
             beforeLocal.tm_year + 1900, beforeLocal.tm_mon + 1, beforeLocal.tm_mday,
             beforeLocal.tm_hour, beforeLocal.tm_min, beforeLocal.tm_sec,
             afterLocal.tm_year + 1900, afterLocal.tm_mon + 1, afterLocal.tm_mday,
             afterLocal.tm_hour, afterLocal.tm_min, afterLocal.tm_sec,
             getenv("TZ") ? getenv("TZ") : "");
    
    return Response::success(request, [](JsonVariant& root) {
        root["ok"] = true;
    });
}

