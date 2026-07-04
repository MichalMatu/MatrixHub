#ifndef WiFiSettingsService_h
#define WiFiSettingsService_h

/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 - 2025 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <IPAddress.h>
#include <utils/SettingValue.h>
#include <core/StatefulService.h>
#include <core/FSPersistence.h>
#include <core/HttpEndpoint.h>
#include <utils/JsonUtils.h>
#include <security/SecurityManager.h>
#include <wifi/APSettingsService.h>
#include <wifi/WifiDiagnosticsSelector.h>
#include <WiFi.h>
#include <PsychicHttp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <functional>
#include <string_view>

#ifndef FACTORY_WIFI_SSID
#define FACTORY_WIFI_SSID ""
#endif

#ifndef FACTORY_WIFI_PASSWORD
#define FACTORY_WIFI_PASSWORD ""
#endif

#ifndef FACTORY_WIFI_HOSTNAME
#define FACTORY_WIFI_HOSTNAME "matrixhub"
#endif

#define WIFI_SETTINGS_FILE "/config/wifiSettings.json"
#define WIFI_SETTINGS_SERVICE_PATH "/rest/wifiSettings"

#define WIFI_RECONNECTION_DELAY 2000
#define WIFI_MAX_ATTEMPTS_PER_NETWORK 3
#define WIFI_RETRY_BACKOFF_BASE_MS 30000   // Initial backoff: 30s
#define WIFI_RETRY_BACKOFF_MAX_MS 300000   // Max backoff: 5 minutes
#define RSSI_EVENT_DELAY 500
#define DELAYED_RECONNECT_MS 1000

#define EVENT_RSSI "rssi"
#define EVENT_RECONNECT "reconnect"

// Struct defining the wifi settings
typedef struct
{
    String ssid;
    String password;
    bool staticIPConfig;
    IPAddress localIP;
    IPAddress gatewayIP;
    IPAddress subnetMask;
    IPAddress dnsIP1;
    IPAddress dnsIP2;
} wifi_settings_t;

enum class WiFiOperatingMode : uint8_t
{
    Off = 0,
    AccessPoint = 1,
    Station = 2
};

enum class WiFiConnectivityState : uint8_t
{
    Off = 0,
    ApOnly,
    StaConnecting,
    StaConnected,
    StaBackoff
};

const char* wifiOperatingModeName(WiFiOperatingMode mode);
const char* wifiConnectivityStateName(WiFiConnectivityState state);

struct WiFiConnectivityDiagnostics
{
    WiFiConnectivityState state = WiFiConnectivityState::Off;
    WiFiOperatingMode configuredMode = WiFiOperatingMode::AccessPoint;
    wifi_mode_t wifiMode = WIFI_MODE_NULL;
    bool staConnected = false;
    bool apActive = false;
    bool savedStaticIpConfigured = false;
    bool savedStaticIpMatches = false;
    bool portForwardingReady = false;
    uint8_t apStationCount = 0;
    uint8_t lastDisconnectReason = 0;
    uint32_t disconnectedSinceMs = 0;
    uint32_t stableConnectedSinceMs = 0;
    uint32_t lastIpChangeMs = 0;
    IPAddress staIp;
    IPAddress savedStaticIp;
    IPAddress apIp;
    char lastRecoveryReason[32]{0};
};

class WiFiSettings
{
public:
    // core wifi configuration
    String hostname;
    WiFiOperatingMode mode = WiFiOperatingMode::AccessPoint;
    std::vector<wifi_settings_t> wifiSettings;

    static void read(WiFiSettings &settings, JsonObject &root)
    {
        root["hostname"] = settings.hostname;
        root["mode"] = wifiOperatingModeName(settings.mode);

        // create JSON array from root
        JsonArray wifiNetworks = root["wifi_networks"].to<JsonArray>();

        // iterate over the wifiSettings
        for (auto &wifi : settings.wifiSettings)
        {
            // create JSON object for each wifi network
            JsonObject wifiNetwork = wifiNetworks.add<JsonObject>();

            // add the ssid and password to the JSON object
            wifiNetwork["ssid"] = wifi.ssid;
            wifiNetwork["password"] = wifi.password;
            wifiNetwork["static_ip_config"] = wifi.staticIPConfig;

            // extended settings
            JsonUtils::writeIP(wifiNetwork, "local_ip", wifi.localIP);
            JsonUtils::writeIP(wifiNetwork, "gateway_ip", wifi.gatewayIP);
            JsonUtils::writeIP(wifiNetwork, "subnet_mask", wifi.subnetMask);
            JsonUtils::writeIP(wifiNetwork, "dns_ip_1", wifi.dnsIP1);
            JsonUtils::writeIP(wifiNetwork, "dns_ip_2", wifi.dnsIP2);
        }

        ESP_LOGV(SVK_TAG, "WiFi Settings read");
    }

    static StateUpdateResult update(JsonObject &root, WiFiSettings &settings, std::string_view originId)
    {
        const bool fromHttp = originId == HTTP_ENDPOINT_ORIGIN_ID;

        WiFiSettings nextSettings = settings;
        nextSettings.hostname = root["hostname"] | SettingValue::format(FACTORY_WIFI_HOSTNAME);
        nextSettings.wifiSettings.clear();

        // create JSON array from root
        JsonArray wifiNetworks = root["wifi_networks"];
        if (root["wifi_networks"].is<JsonArray>())
        {
            // iterate over the wifiSettings
            int i = 0;
            for (auto wifiNetwork : wifiNetworks)
            {
                // max 5 wifi networks
                if (i++ >= 5)
                {
                    ESP_LOGE(SVK_TAG, "Too many wifi networks");
                    break;
                }

                // create JSON object for each wifi network
                JsonObject wifi = wifiNetwork.as<JsonObject>();

                String ssid = wifi["ssid"] | "";
                String password = wifi["password"] | "";

                // Check if SSID length is between 1 and 32 bytes and password between 0 and 64 bytes
                if (ssid.length() < 1 || ssid.length() > 32 || password.length() > 64)
                {
                    ESP_LOGE(SVK_TAG, "SSID or password length is invalid");
                }
                else
                {
                    // add the ssid and password to the JSON object
                    wifi_settings_t wifiSettings;

                    wifiSettings.ssid = ssid;
                    wifiSettings.password = password;
                    wifiSettings.staticIPConfig = wifi["static_ip_config"];

                    // extended settings
                    JsonUtils::readIP(wifi, "local_ip", wifiSettings.localIP);
                    JsonUtils::readIP(wifi, "gateway_ip", wifiSettings.gatewayIP);
                    JsonUtils::readIP(wifi, "subnet_mask", wifiSettings.subnetMask);
                    JsonUtils::readIP(wifi, "dns_ip_1", wifiSettings.dnsIP1);
                    JsonUtils::readIP(wifi, "dns_ip_2", wifiSettings.dnsIP2);

                    // Swap around the dns servers if 2 is populated but 1 is not
                    if (IPUtils::isNotSet(wifiSettings.dnsIP1) && IPUtils::isSet(wifiSettings.dnsIP2))
                    {
                        wifiSettings.dnsIP1 = wifiSettings.dnsIP2;
                        wifiSettings.dnsIP2 = INADDR_NONE;
                    }

                    // Turning off static ip config if we don't meet the minimum requirements
                    // of ipAddress, gateway and subnet. This may change to static ip only
                    // as sensible defaults can be assumed for gateway and subnet
                    if (wifiSettings.staticIPConfig && (IPUtils::isNotSet(wifiSettings.localIP) || IPUtils::isNotSet(wifiSettings.gatewayIP) ||
                                                        IPUtils::isNotSet(wifiSettings.subnetMask)))
                    {
                        wifiSettings.staticIPConfig = false;
                    }

                    nextSettings.wifiSettings.push_back(wifiSettings);
                }
            }
        }
        else
        {
            // populate with factory defaults if they are present
            if (String(FACTORY_WIFI_SSID).length() > 0)
            {
                nextSettings.wifiSettings.push_back(wifi_settings_t{
                    .ssid = FACTORY_WIFI_SSID,
                    .password = FACTORY_WIFI_PASSWORD,
                    .staticIPConfig = false,
                    .localIP = INADDR_NONE,
                    .gatewayIP = INADDR_NONE,
                    .subnetMask = INADDR_NONE,
                    .dnsIP1 = INADDR_NONE,
                    .dnsIP2 = INADDR_NONE
                });
            }
        }

        const bool modeProvided = root["mode"].is<const char*>();
        if (modeProvided)
        {
            WiFiOperatingMode requestedMode = WiFiOperatingMode::AccessPoint;
            const char* modeValue = root["mode"];
            if (!parseMode(modeValue, requestedMode))
            {
                if (fromHttp)
                {
                    ESP_LOGE(SVK_TAG, "Invalid WiFi mode: %s", modeValue ? modeValue : "<null>");
                    return StateUpdateResult::ERROR;
                }
                requestedMode = WiFiOperatingMode::AccessPoint;
            }
            nextSettings.mode = requestedMode;
        }
        else
        {
            nextSettings.mode = nextSettings.wifiSettings.empty()
                ? WiFiOperatingMode::AccessPoint
                : WiFiOperatingMode::Station;
        }

        if (nextSettings.mode == WiFiOperatingMode::Station && nextSettings.wifiSettings.empty())
        {
            if (fromHttp)
            {
                ESP_LOGE(SVK_TAG, "Rejecting STA mode without saved WiFi networks");
                return StateUpdateResult::ERROR;
            }
            nextSettings.mode = WiFiOperatingMode::AccessPoint;
        }

        const bool changed = !equals(settings, nextSettings);
        settings = nextSettings;
        ESP_LOGV(SVK_TAG, "WiFi Settings updated");

        return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
    };

private:
    static bool parseMode(const char* value, WiFiOperatingMode& mode)
    {
        if (!value)
        {
            return false;
        }
        if (strcmp(value, "off") == 0)
        {
            mode = WiFiOperatingMode::Off;
            return true;
        }
        if (strcmp(value, "ap") == 0)
        {
            mode = WiFiOperatingMode::AccessPoint;
            return true;
        }
        if (strcmp(value, "sta") == 0)
        {
            mode = WiFiOperatingMode::Station;
            return true;
        }
        return false;
    }

    static bool networkEquals(const wifi_settings_t& lhs, const wifi_settings_t& rhs)
    {
        return lhs.ssid == rhs.ssid &&
               lhs.password == rhs.password &&
               lhs.staticIPConfig == rhs.staticIPConfig &&
               lhs.localIP == rhs.localIP &&
               lhs.gatewayIP == rhs.gatewayIP &&
               lhs.subnetMask == rhs.subnetMask &&
               lhs.dnsIP1 == rhs.dnsIP1 &&
               lhs.dnsIP2 == rhs.dnsIP2;
    }

    static bool equals(const WiFiSettings& lhs, const WiFiSettings& rhs)
    {
        if (lhs.hostname != rhs.hostname ||
            lhs.mode != rhs.mode ||
            lhs.wifiSettings.size() != rhs.wifiSettings.size())
        {
            return false;
        }

        for (size_t i = 0; i < lhs.wifiSettings.size(); i++)
        {
            if (!networkEquals(lhs.wifiSettings[i], rhs.wifiSettings[i]))
            {
                return false;
            }
        }
        return true;
    }
};

class WiFiSettingsService : public StatefulService<WiFiSettings>
{
public:
    WiFiSettingsService(PsychicHttpServer *server, FS *fs, SecurityManager *securityManager);

    void setAPSettingsService(APSettingsService *apSettingsService);
    void setActivityCallback(std::function<void()> cb) { _onActivityCallback = cb; }
    void setHostnameChangeCallback(std::function<void()> cb) { _onHostnameChangeCallback = cb; }

    void initWiFi();
    void begin();
    void loop();
    void delayedReconnect();
    String getHostname();
    String getIP();
    WiFiOperatingMode getConfiguredMode() const;
    bool isApModeConfigured() const;
    bool setModeAndRestart(WiFiOperatingMode mode);
    bool requestRecovery(const char* reason);
    WiFiConnectivityDiagnostics getConnectivityDiagnostics() const;

private:
    // Runtime connectivity state is shared between the main loop, API-triggered
    // recovery and health-triggered recovery. We keep one owner model:
    // external callers may only queue intent, while loop() consumes and applies
    // the actual transition. Snapshotting this struct gives policy and API code
    // one coherent view instead of a mix of old and new field values.
    struct RuntimeSnapshot
    {
        size_t currentNetworkIndex = 0;
        uint8_t connectionAttempts = 0;
        uint8_t backoffLevel = 0;
        WiFiConnectivityState connectivityState = WiFiConnectivityState::StaConnecting;
        bool retryCycleCompleted = false;
        bool retryBackoffActive = false;
        bool recoveryRequested = false;
        unsigned long lastConnectionAttempt = 0;
        unsigned long connectingSince = 0;
        unsigned long disconnectedSince = 0;
        unsigned long stableConnectedSince = 0;
        unsigned long lastIpChangeMs = 0;
        IPAddress lastKnownStaIp = INADDR_NONE;
        char pendingRecoveryReason[32]{0};
        char lastRecoveryReason[32]{0};
    };

    PsychicHttpServer *_server;
    SecurityManager *_securityManager;
    std::function<void()> _onActivityCallback = nullptr;
    std::function<void()> _onHostnameChangeCallback = nullptr;
    HttpEndpoint<WiFiSettings> _httpEndpoint;
    FSPersistence<WiFiSettings> _fsPersistence;
    APSettingsService *_apSettingsService = nullptr;
    // Protects every mutable runtime field below. The production fix here was
    // to stop touching recovery/timing flags from multiple paths without one
    // synchronization point, because that could lose recovery requests and
    // expose impossible diagnostic combinations to the dashboard.
    mutable portMUX_TYPE _runtimeStateLock = portMUX_INITIALIZER_UNLOCKED;
    size_t _currentNetworkIndex = 0;
    uint8_t _connectionAttempts = 0;
    uint8_t _backoffLevel = 0;  // Exponential backoff: delay = BASE * 2^level, capped at MAX
    WiFiConnectivityState _connectivityState = WiFiConnectivityState::StaConnecting;
    bool _retryCycleCompleted = false;
    bool _retryBackoffActive = false;
    bool _recoveryRequested = false;
    unsigned long _lastConnectionAttempt = 0;
    unsigned long _connectingSince = 0;
    unsigned long _disconnectedSince = 0;
    unsigned long _stableConnectedSince = 0;
    unsigned long _lastIpChangeMs = 0;
    String _lastAppliedHostname;
    IPAddress _lastKnownStaIp = INADDR_NONE;
    char _pendingRecoveryReason[32]{0};
    char _lastRecoveryReason[32]{0};

    bool hasConfiguredNetworks() const;
    // Returns a coherent copy of the runtime state for policy evaluation and
    // diagnostics without holding the lock across slower WiFi/HTTP operations.
    RuntimeSnapshot snapshotRuntimeState() const;
    void observeConnectivity(unsigned long nowMs);
    void handleConnected(unsigned long nowMs);
    bool processRecoveryRequest();
    void applyConfiguredMode();
    void manageSTA();
    void connectToWiFi();
    void configureNetwork(wifi_settings_t &network);
    bool switchToAPMode(const char* reason);
    void switchToOffMode(const char* reason);
    void resetStaRuntime(WiFiConnectivityState state);
};

#endif // end WiFiSettingsService_h
